package com.remotecontrol.relay.handler;

import com.remotecontrol.relay.protocol.Protocol;
import com.remotecontrol.relay.protocol.ProtocolConstants;
import com.remotecontrol.relay.protocol.ProtocolHeader;
import com.remotecontrol.relay.registry.RelayRegistry;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.Channel;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.util.AttributeKey;

import com.remotecontrol.relay.auth.RelayAuthorizer;
import com.remotecontrol.relay.auth.RelayAuthPayload;
import java.util.concurrent.TimeUnit;
import java.util.function.Function;

public class RelayServerHandler extends SimpleChannelInboundHandler<Protocol> {

    private static final long DISCONNECT_WRITE_TIMEOUT_SECONDS = 15;
    private static final long AUTHORIZATION_LEASE_INTERVAL_SECONDS = 10;
    private static final long AUTHORIZATION_RETRY_DELAY_SECONDS = 1;
    private static final int MAX_CONTROL_RESERVE_BYTES = 128 * 1024;
    private static final int MAX_RESERVED_SCREEN_BYTES = 256 * 1024;
    private static final AttributeKey<Boolean> SCREEN_ACK_CAPABLE =
            AttributeKey.valueOf(RelayServerHandler.class, "screenAckCapable");

    private final RelayRegistry relayRegistry;
    private final RelayAuthorizer authorizer;
    private boolean authorizationPending;
    private io.netty.util.concurrent.ScheduledFuture<?> lease;
    private long leaseGeneration;
    private long reservedScreenSessionId;

    public RelayServerHandler(RelayRegistry relayRegistry) {
        this(relayRegistry, new RelayAuthorizer());
    }

    public RelayServerHandler(RelayRegistry relayRegistry, RelayAuthorizer authorizer) {
        this.relayRegistry = relayRegistry;
        this.authorizer = authorizer;
    }

    @Override
    protected void channelRead0(ChannelHandlerContext ctx, Protocol msg) throws Exception {
        if (msg.getHeader().getType() == ProtocolConstants.MessageType.REGISTER_HOST.getValue()) {
            handleRegisterHost(ctx, msg);
            return;
        }

        if (msg.getHeader().getType() == ProtocolConstants.MessageType.CONNECT_REQUEST.getValue()) {
            handleConnectRequest(ctx, msg);
            return;
        }

        if (msg.getHeader().getType() == ProtocolConstants.MessageType.SESSION_ACCEPT.getValue()) {
            handleSessionAccept(ctx, msg);
            return;
        }

        if (msg.getHeader().getType() == ProtocolConstants.MessageType.SESSION_REJECT.getValue()) {
            handleSessionReject(ctx, msg);
            return;
        }

        if (msg.getHeader().getType() == ProtocolConstants.MessageType.DISCONNECT.getValue()) {
            handleDisconnect(ctx, msg);
            return;
        }

        if (msg.getHeader().getType() == ProtocolConstants.MessageType.SCREEN_FRAME_ACK.getValue()) {
            handleScreenAcknowledgement(ctx, msg);
            return;
        }

        var target = relayRegistry.findActivePeer(msg.getHeader().getSessionId(), ctx.channel());
        if (target == null || !RelayForwardingPolicy.allows(msg.getHeader().getType(), target.fromChild())) return;

        if (target.fromChild()) {
            forwardScreen(ctx, target.channel(), msg);
        } else if (RelayForwardingPolicy.validInput(msg)) {
            forwardControl(ctx, target.channel(), msg);
        } else {
            ctx.close();
        }
    }

    private void handleScreenAcknowledgement(ChannelHandlerContext ctx, Protocol msg) {
        ProtocolHeader header = msg.getHeader();
        byte[] payload = msg.getPayload();
        if (header.getPayloadLength() != 0 || payload == null || payload.length != 0) return;
        if (header.getFlags() == ProtocolConstants.SCREEN_ACK_CAPABILITY_FLAG) {
            if (header.getSessionId() == 0 && header.getSequenceNumber() == 0 && !authorizationPending
                    && !relayRegistry.isRegisteredChild(ctx.channel()) && !relayRegistry.hasSession(ctx.channel())) {
                ctx.channel().attr(SCREEN_ACK_CAPABLE).set(true);
            }
            return;
        }
        if (header.getFlags() != 0) return;

        var target = relayRegistry.findActivePeer(header.getSessionId(), ctx.channel());
        if (target != null && !target.fromChild() && screenAcknowledgementsEnabled(ctx.channel(), target.channel())) {
            forwardControl(ctx, target.channel(), msg);
        }
    }

    private static boolean screenAcknowledgementsEnabled(Channel first, Channel second) {
        return Boolean.TRUE.equals(first.attr(SCREEN_ACK_CAPABLE).get())
                && Boolean.TRUE.equals(second.attr(SCREEN_ACK_CAPABLE).get());
    }

    private void forwardScreen(ChannelHandlerContext ctx, Channel target, Protocol msg) {
        // Keep RDTP frames opaque. Legacy peers can start another frame after a drop.
        if (target.isWritable()) {
            forward(ctx, target, msg);
            return;
        }

        // A paced sender is waiting for this frame's ACK. Give its single packet room to drain.
        long sessionId = msg.getHeader().getSessionId();
        if (!screenAcknowledgementsEnabled(ctx.channel(), target)
                || msg.getHeader().getPayloadLength() > MAX_RESERVED_SCREEN_BYTES - ProtocolConstants.HEADER_SIZE
                || reservedScreenSessionId == sessionId) {
            return;
        }
        reservedScreenSessionId = sessionId;
        forward(ctx, target, msg).addListener(future -> ctx.executor().execute(() -> {
            if (reservedScreenSessionId == sessionId) reservedScreenSessionId = 0;
        }));
    }

    private void forwardControl(ChannelHandlerContext ctx, Channel target, Protocol msg) {
        // Moves are replaceable. Button events carry their own coordinates.
        if (msg.getHeader().getType() == ProtocolConstants.MessageType.MOUSE_MOVE.getValue()
                && !target.isWritable()) return;
        // Keep input transitions and ACKs ordered in Netty's queue across temporary unwritability.
        var watermark = target.config().getWriteBufferWaterMark();
        long writableThreshold = (long) watermark.high() - watermark.low() + MAX_CONTROL_RESERVE_BYTES;
        if (target.bytesBeforeWritable() >= writableThreshold) {
            System.err.println("[RelayServer] Control write backlog exceeded for session "
                    + msg.getHeader().getSessionId());
            ctx.close();
            return;
        }
        forward(ctx, target, msg);
    }

    private ChannelFuture forward(ChannelHandlerContext ctx, Channel target, Protocol msg) {
        return target.writeAndFlush(msg).addListener(future -> {
            if (!future.isSuccess() && relayRegistry.findActivePeer(msg.getHeader().getSessionId(), ctx.channel()) != null) {
                ctx.close();
            }
        });
    }

    private void handleRegisterHost(ChannelHandlerContext ctx, Protocol msg) {
        var credentials = RelayAuthPayload.decode(msg, false);
        if (credentials == null || authorizationPending || relayRegistry.hasSession(ctx.channel())) {
            sendRegisterAck(ctx, false);
            return;
        }
        authorizationPending = true;
        authorizer.authorize(credentials.token(), null).whenComplete((agentId, failure) -> ctx.executor().execute(() -> {
            authorizationPending = false;
            if (!ctx.channel().isActive()) return;
            boolean accepted = failure == null && agentId != null && relayRegistry.registerChild(agentId, ctx.channel());
            sendRegisterAck(ctx, accepted);
            if (accepted) startLease(ctx, credentials, agentId);
        }));
    }

    private void sendRegisterAck(ChannelHandlerContext ctx, boolean accepted) {
        byte[] payload = new byte[] {(byte) (accepted ? 1 : 0)};
        ProtocolHeader header = new ProtocolHeader(
                ProtocolConstants.PROTOCOL_MAGIC,
                ProtocolConstants.PROTOCOL_VERSION,
                ProtocolConstants.MessageType.REGISTER_ACK.getValue(),
                (short) 0,
                payload.length,
                0L,
                0);
        ctx.writeAndFlush(new Protocol(header, payload)).addListener(ChannelFutureListener.CLOSE_ON_FAILURE);
    }

    private void handleConnectRequest(ChannelHandlerContext ctx, Protocol msg) {
        var credentials = RelayAuthPayload.decode(msg, true);
        if (credentials == null || authorizationPending || relayRegistry.isRegisteredChild(ctx.channel())) {
            sendConnectResult(ctx.channel(), false, 0L);
            return;
        }
        authorizationPending = true;
        authorizer.authorize(credentials.token(), credentials.targetSessionId()).whenComplete((agentId, failure) ->
                ctx.executor().execute(() -> {
            authorizationPending = false;
            if (!ctx.channel().isActive()) return;
            Channel child = failure == null && agentId != null ? relayRegistry.findRegisteredChild(agentId) : null;
            var session = child == null ? null : relayRegistry.createPendingSession(ctx.channel(), child, agentId);
            if (session == null) {
                sendConnectResult(ctx.channel(), false, 0L);
                return;
            }
            startLease(ctx, credentials, agentId);
            if (screenAcknowledgementsEnabled(ctx.channel(), child)) {
                // The CHILD must know to pace its first frame before it sees SESSION_REQUEST.
                sendScreenAcknowledgementCapability(child, session.getSessionId());
            }
            sendSessionRequest(child, session.getSessionId());
        }));
    }

    // Revocation, deletion, JWT expiry, or lost presence also terminate existing connections.
    private void startLease(ChannelHandlerContext ctx, RelayAuthPayload credentials, String agentId) {
        if (lease != null) lease.cancel(false);
        long generation = ++leaseGeneration;
        scheduleLeaseCheck(ctx, credentials, agentId, generation, false);
    }

    private void scheduleLeaseCheck(ChannelHandlerContext ctx, RelayAuthPayload credentials, String agentId,
                                    long generation, boolean retry) {
        long delay = retry ? AUTHORIZATION_RETRY_DELAY_SECONDS : AUTHORIZATION_LEASE_INTERVAL_SECONDS;
        lease = ctx.executor().schedule(() -> {
            authorizer.authorize(credentials.token(), credentials.targetSessionId()).whenComplete((authorized, failure) ->
                    ctx.executor().execute(() -> {
                if (!ctx.channel().isActive() || generation != leaseGeneration) return;
                if (failure != null && !retry) {
                    scheduleLeaseCheck(ctx, credentials, agentId, generation, true);
                } else if (failure != null || !agentId.equals(authorized)) {
                    System.err.println(failure != null
                            ? "[RelayServer] Closing connection after authorization service failed on retry"
                            : "[RelayServer] Closing connection after session authorization expired or was revoked");
                    ctx.close();
                } else {
                    startLease(ctx, credentials, agentId);
                }
            }));
        }, delay, TimeUnit.SECONDS);
    }

    private void stopLease() {
        ++leaseGeneration;
        if (lease != null) lease.cancel(false);
        lease = null;
    }

    private void handleSessionAccept(ChannelHandlerContext ctx, Protocol msg) {
        if (!isValidSessionResponse(msg)) {
            System.out.println("[RelayServer] Invalid SESSION_ACCEPT ignored");
            return;
        }

        RelayRegistry.SessionRecord session = relayRegistry.acceptPendingSession(
                msg.getHeader().getSessionId(), ctx.channel());
        if (session == null) {
            System.out.println("[RelayServer] Invalid SESSION_ACCEPT source or state");
            return;
        }

        sendConnectResult(session.getAdminChannel(), true, session.getSessionId());
        if (screenAcknowledgementsEnabled(session.getAdminChannel(), session.getChildChannel())) {
            sendScreenAcknowledgementCapability(session.getAdminChannel(), session.getSessionId());
        }
        System.out.println("[RelayServer] Session ACTIVE sessionId=" + session.getSessionId());
    }

    private void handleSessionReject(ChannelHandlerContext ctx, Protocol msg) {
        if (!isValidSessionResponse(msg)) {
            System.out.println("[RelayServer] Invalid SESSION_REJECT ignored");
            return;
        }

        RelayRegistry.SessionRecord session = relayRegistry.rejectPendingSession(
                msg.getHeader().getSessionId(), ctx.channel());
        if (session == null) {
            System.out.println("[RelayServer] Invalid SESSION_REJECT source or state");
            return;
        }

        sendConnectResult(session.getAdminChannel(), false, 0L);
        System.out.println("[RelayServer] Session rejected sessionId=" + session.getSessionId());
    }

    private void handleDisconnect(ChannelHandlerContext ctx, Protocol msg) {
        if (!isValidSessionResponse(msg)) return;
        var session = relayRegistry.beginDisconnect(msg.getHeader().getSessionId(), ctx.channel());
        if (session == null) return;
        stopLease();
        notifyDisconnectedChild(session, child -> child.writeAndFlush(msg));
    }

    private void notifyDisconnectedChild(RelayRegistry.SessionRecord session,
                                         Function<Channel, ChannelFuture> writer) {
        Channel child = session.getChildChannel();
        try {
            var deadline = child.eventLoop().schedule(() -> {
                if (relayRegistry.completeDisconnect(session, false)) closeFailedChild(child);
            }, DISCONNECT_WRITE_TIMEOUT_SECONDS, TimeUnit.SECONDS);
            try {
                writer.apply(child).addListener(future -> {
                    deadline.cancel(false);
                    if (relayRegistry.completeDisconnect(session, future.isSuccess())) closeFailedChild(child);
                });
            } catch (RuntimeException failure) {
                deadline.cancel(false);
                throw failure;
            }
        } catch (RuntimeException failure) {
            if (relayRegistry.completeDisconnect(session, false)) closeFailedChild(child);
        }
    }

    private void closeFailedChild(Channel child) {
        child.closeFuture().addListener(future -> relayRegistry.releaseFailedDisconnectChannel(child));
        child.close();
    }

    private boolean isValidSessionResponse(Protocol msg) {
        ProtocolHeader header = msg.getHeader();
        byte[] payload = msg.getPayload();
        return header.getFlags() == 0
                && header.getSessionId() != 0
                && header.getSequenceNumber() == 0
                && header.getPayloadLength() == 0
                && payload != null
                && payload.length == 0;
    }

    private void sendSessionRequest(Channel childChannel, long sessionId) {
        ProtocolHeader header = new ProtocolHeader(
                ProtocolConstants.PROTOCOL_MAGIC,
                ProtocolConstants.PROTOCOL_VERSION,
                ProtocolConstants.MessageType.SESSION_REQUEST.getValue(),
                (short) 0,
                0,
                sessionId,
                0);
        childChannel.writeAndFlush(new Protocol(header, new byte[0])).addListener(ChannelFutureListener.CLOSE_ON_FAILURE);
    }

    private static void sendScreenAcknowledgementCapability(Channel channel, long sessionId) {
        ProtocolHeader header = new ProtocolHeader(
                ProtocolConstants.PROTOCOL_MAGIC,
                ProtocolConstants.PROTOCOL_VERSION,
                ProtocolConstants.MessageType.SCREEN_FRAME_ACK.getValue(),
                ProtocolConstants.SCREEN_ACK_CAPABILITY_FLAG,
                0, sessionId, 0);
        channel.writeAndFlush(new Protocol(header, new byte[0])).addListener(ChannelFutureListener.CLOSE_ON_FAILURE);
    }

    public static void notifyPendingTimeout(RelayRegistry.SessionRecord session) {
        if (session.getAdminChannel().isActive()) sendConnectResult(session.getAdminChannel(), false, 0L);
        session.getChildChannel().close();
    }

    private static void sendConnectResult(Channel adminChannel, boolean accepted, long sessionId) {
        byte[] payload = new byte[] {(byte) (accepted ? 1 : 0)};
        ProtocolHeader header = new ProtocolHeader(
                ProtocolConstants.PROTOCOL_MAGIC,
                ProtocolConstants.PROTOCOL_VERSION,
                ProtocolConstants.MessageType.CONNECT_RESULT.getValue(),
                (short) 0,
                payload.length,
                accepted ? sessionId : 0L,
                0);
        adminChannel.writeAndFlush(new Protocol(header, payload)).addListener(ChannelFutureListener.CLOSE_ON_FAILURE);
    }

    @Override
    public void channelActive(ChannelHandlerContext ctx) throws Exception {
        System.out.println("[RelayServer] Co ket noi moi: " + ctx.channel().remoteAddress());
        super.channelActive(ctx);
    }

    @Override
    public void channelInactive(ChannelHandlerContext ctx) throws Exception {
        stopLease();
        relayRegistry.removeSessionForChannel(ctx.channel());
        relayRegistry.unregisterChild(ctx.channel());
        System.out.println("[RelayServer] Ket noi da dong: " + ctx.channel().remoteAddress());
        super.channelInactive(ctx);
    }

    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) throws Exception {
        System.err.println("[RelayServer] Loi ket noi: " + cause.getMessage());
        ctx.close();
    }
}
