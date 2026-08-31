package com.remotecontrol.relay.handler;

import com.remotecontrol.relay.handler.screen.ScreenFrameHandler;
import com.remotecontrol.relay.protocol.Protocol;
import com.remotecontrol.relay.protocol.ProtocolConstants;
import com.remotecontrol.relay.protocol.ProtocolHeader;
import com.remotecontrol.relay.registry.RelayRegistry;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.Channel;
import io.netty.channel.SimpleChannelInboundHandler;

import java.nio.ByteBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;

public class RelayServerHandler extends SimpleChannelInboundHandler<Protocol> {

    // One screen handler per channel; the registry is shared through construction.
    private final ScreenFrameHandler screenFrameHandler = new ScreenFrameHandler();
    private final RelayRegistry relayRegistry;

    public RelayServerHandler(RelayRegistry relayRegistry) {
        this.relayRegistry = relayRegistry;
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

        if (msg.getHeader().getType() == ProtocolConstants.MessageType.SCREEN_FRAME.getValue()) {
            handleScreenFrame(ctx, msg);
            return;
        }

        System.out.println("[RelayServer] Nhan duoc goi tin tu Client: " + ctx.channel().remoteAddress());
        System.out.println("Noi dung Header: " + msg.toString());

    }

    private void handleRegisterHost(ChannelHandlerContext ctx, Protocol msg) {
        String username = decodeRegistrationUsername(msg);
        boolean accepted = username != null && relayRegistry.registerChild(username, ctx.channel());

        sendRegisterAck(ctx, accepted);
        System.out.println("[RelayServer] REGISTER_HOST "
                + (accepted ? "accepted" : "rejected")
                + (username == null ? "" : " for " + username));
    }

    private String decodeRegistrationUsername(Protocol msg) {
        ProtocolHeader header = msg.getHeader();
        byte[] payload = msg.getPayload();

        if (header.getFlags() != 0
                || header.getSessionId() != 0
                || header.getSequenceNumber() != 0
                || payload == null
                || header.getPayloadLength() != payload.length
                || payload.length < 2) {
            return null;
        }

        int usernameLength = ((payload[0] & 0xFF) << 8) | (payload[1] & 0xFF);
        if (usernameLength < 1
                || usernameLength > 200
                || payload.length != 2 + usernameLength) {
            return null;
        }

        try {
            String username = StandardCharsets.UTF_8.newDecoder()
                    .onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT)
                    .decode(ByteBuffer.wrap(payload, 2, usernameLength))
                    .toString();
            return username.isEmpty() ? null : username;
        } catch (CharacterCodingException exception) {
            return null;
        }
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
        String targetUsername = decodeConnectTargetUsername(msg);
        if (targetUsername == null || relayRegistry.isRegisteredChild(ctx.channel())) {
            sendConnectResult(ctx.channel(), false, 0L);
            System.out.println("[RelayServer] CONNECT_REQUEST rejected: invalid request");
            return;
        }

        Channel childChannel = relayRegistry.findRegisteredChild(targetUsername);
        if (childChannel == null) {
            sendConnectResult(ctx.channel(), false, 0L);
            System.out.println("[RelayServer] CONNECT_REQUEST rejected: CHILD not registered");
            return;
        }

        RelayRegistry.SessionRecord session = relayRegistry.createPendingSession(
                ctx.channel(), childChannel, targetUsername);
        if (session == null) {
            sendConnectResult(ctx.channel(), false, 0L);
            System.out.println("[RelayServer] CONNECT_REQUEST rejected: ADMIN or CHILD busy");
            return;
        }

        sendSessionRequest(childChannel, session.getSessionId());
        System.out.println("[RelayServer] Session PENDING sessionId="
                + session.getSessionId() + " childUsername=" + targetUsername);
    }

    private String decodeConnectTargetUsername(Protocol msg) {
        ProtocolHeader header = msg.getHeader();
        byte[] payload = msg.getPayload();

        if (header.getFlags() != 0
                || header.getSessionId() != 0
                || header.getSequenceNumber() != 0
                || payload == null
                || header.getPayloadLength() != payload.length
                || payload.length < 2) {
            return null;
        }

        int usernameLength = ((payload[0] & 0xFF) << 8) | (payload[1] & 0xFF);
        if (usernameLength < 1
                || usernameLength > 200
                || payload.length != 2 + usernameLength) {
            return null;
        }

        try {
            String username = StandardCharsets.UTF_8.newDecoder()
                    .onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT)
                    .decode(ByteBuffer.wrap(payload, 2, usernameLength))
                    .toString();
            return username.isEmpty() ? null : username;
        } catch (CharacterCodingException exception) {
            return null;
        }
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

    private void handleScreenFrame(ChannelHandlerContext ctx, Protocol msg) {
        long sessionId = msg.getHeader().getSessionId();
        RelayRegistry.SessionRecord session = relayRegistry.findActiveSessionForChild(
                sessionId, ctx.channel());
        if (session == null) {
            System.out.println("[RelayServer] SCREEN_FRAME rejected: invalid session or source");
            return;
        }

        session.getAdminChannel().writeAndFlush(msg);
        System.out.println("[RelayServer] SCREEN_FRAME forwarded sessionId=" + sessionId
                + " sequenceNumber=" + msg.getHeader().getSequenceNumber()
                + " payloadBytes=" + msg.getHeader().getPayloadLength());

        // Existing reassembly remains diagnostic-only; forwarding above uses the original message.
        screenFrameHandler.handle(msg);
    }

    @Override
    public void channelActive(ChannelHandlerContext ctx) throws Exception {
        System.out.println("[RelayServer] Co ket noi moi: " + ctx.channel().remoteAddress());
        super.channelActive(ctx);
    }

    @Override
    public void channelInactive(ChannelHandlerContext ctx) throws Exception {
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
