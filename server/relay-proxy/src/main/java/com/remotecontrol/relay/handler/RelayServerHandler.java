package com.remotecontrol.relay.handler;

import com.remotecontrol.relay.protocol.Protocol;
import com.remotecontrol.relay.protocol.ProtocolConstants;
import com.remotecontrol.relay.protocol.ProtocolHeader;
import com.remotecontrol.relay.registry.RelayRegistry;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.Channel;
import io.netty.channel.SimpleChannelInboundHandler;

import com.remotecontrol.relay.auth.RelayAuthorizer;
import com.remotecontrol.relay.auth.RelayAuthPayload;
import java.util.concurrent.TimeUnit;

public class RelayServerHandler extends SimpleChannelInboundHandler<Protocol> {

    private final RelayRegistry relayRegistry;
    private final RelayAuthorizer authorizer;
    private boolean authorizationPending;
    private io.netty.util.concurrent.ScheduledFuture<?> lease;

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

        var target = relayRegistry.findActivePeer(msg.getHeader().getSessionId(), ctx.channel());
        if (target != null && RelayForwardingPolicy.allows(msg.getHeader().getType(), target.fromChild())) {
            // Keep header, flags, sequence and payload unchanged. Never decode/reassemble video here.
            if (!target.channel().isWritable()) {
                ctx.close(); // Bound memory without silently dropping input or frame fragments.
                return;
            }
            target.channel().writeAndFlush(msg).addListener(future -> {
                if (!future.isSuccess()) ctx.close();
            });
        }
    }

    private void handleRegisterHost(ChannelHandlerContext ctx, Protocol msg) {
        var credentials = RelayAuthPayload.decode(msg, false);
        if (credentials == null || authorizationPending || relayRegistry.hasSession(ctx.channel())) {
            sendRegisterAck(ctx, false);
            return;
        }
        authorizationPending = true;
        authorizer.authorize(credentials.token(), null).thenAccept(agentId -> ctx.executor().execute(() -> {
            authorizationPending = false;
            if (!ctx.channel().isActive()) return;
            boolean accepted = agentId != null && relayRegistry.registerChild(agentId, ctx.channel());
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
        ctx.writeAndFlush(new Protocol(header, payload));
    }

    private void handleConnectRequest(ChannelHandlerContext ctx, Protocol msg) {
        var credentials = RelayAuthPayload.decode(msg, true);
        if (credentials == null || authorizationPending || relayRegistry.isRegisteredChild(ctx.channel())) {
            sendConnectResult(ctx.channel(), false, 0L);
            return;
        }
        authorizationPending = true;
        authorizer.authorize(credentials.token(), credentials.targetSessionId()).thenAccept(agentId ->
                ctx.executor().execute(() -> {
            authorizationPending = false;
            if (!ctx.channel().isActive()) return;
            Channel child = agentId == null ? null : relayRegistry.findRegisteredChild(agentId);
            var session = child == null ? null : relayRegistry.createPendingSession(ctx.channel(), child, agentId);
            if (session == null) {
                sendConnectResult(ctx.channel(), false, 0L);
                return;
            }
            startLease(ctx, credentials, agentId);
            sendSessionRequest(child, session.getSessionId());
        }));
    }

    // Revocation, deletion, JWT expiry, or lost presence also terminate existing connections.
    private void startLease(ChannelHandlerContext ctx, RelayAuthPayload credentials, String agentId) {
        if (lease != null) lease.cancel(false);
        lease = ctx.executor().schedule(() -> {
            authorizer.authorize(credentials.token(), credentials.targetSessionId()).thenAccept(authorized ->
                    ctx.executor().execute(() -> {
                if (!ctx.channel().isActive()) return;
                if (!agentId.equals(authorized)) ctx.close();
                else startLease(ctx, credentials, agentId);
            }));
        }, 10, TimeUnit.SECONDS);
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
        childChannel.writeAndFlush(new Protocol(header, new byte[0]));
    }

    private void sendConnectResult(Channel adminChannel, boolean accepted, long sessionId) {
        byte[] payload = new byte[] {(byte) (accepted ? 1 : 0)};
        ProtocolHeader header = new ProtocolHeader(
                ProtocolConstants.PROTOCOL_MAGIC,
                ProtocolConstants.PROTOCOL_VERSION,
                ProtocolConstants.MessageType.CONNECT_RESULT.getValue(),
                (short) 0,
                payload.length,
                accepted ? sessionId : 0L,
                0);
        adminChannel.writeAndFlush(new Protocol(header, payload));
    }

    @Override
    public void channelActive(ChannelHandlerContext ctx) throws Exception {
        System.out.println("[RelayServer] Co ket noi moi: " + ctx.channel().remoteAddress());
        super.channelActive(ctx);
    }

    @Override
    public void channelInactive(ChannelHandlerContext ctx) throws Exception {
        if (lease != null) lease.cancel(false);
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
