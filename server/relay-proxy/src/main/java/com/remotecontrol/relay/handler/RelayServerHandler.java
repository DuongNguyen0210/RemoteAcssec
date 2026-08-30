package com.remotecontrol.relay.handler;

import com.remotecontrol.relay.handler.screen.ScreenFrameHandler;
import com.remotecontrol.relay.protocol.Protocol;
import com.remotecontrol.relay.protocol.ProtocolConstants;
import com.remotecontrol.relay.protocol.ProtocolHeader;
import com.remotecontrol.relay.registry.RelayRegistry;
import io.netty.channel.ChannelHandlerContext;
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

        // Dispatch SCREEN_FRAME to dedicated handler; preserve existing path for all other types.
        if (msg.getHeader().getType() == ProtocolConstants.MessageType.SCREEN_FRAME.getValue()) {
            screenFrameHandler.handle(msg);
            return;
        }

        System.out.println("[RelayServer] Nhận được gói tin từ Client: " + ctx.channel().remoteAddress());
        System.out.println("Nội dung Header: " + msg.toString());

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

    @Override
    public void channelActive(ChannelHandlerContext ctx) throws Exception {
        System.out.println("[RelayServer] Có kết nối mới: " + ctx.channel().remoteAddress());
        super.channelActive(ctx);
    }

    @Override
    public void channelInactive(ChannelHandlerContext ctx) throws Exception {
        relayRegistry.unregisterChild(ctx.channel());
        System.out.println("[RelayServer] Kết nối đã đóng: " + ctx.channel().remoteAddress());
        super.channelInactive(ctx);
    }

    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) throws Exception {
        System.err.println("[RelayServer] Lỗi kết nối: " + cause.getMessage());
        ctx.close();
    }
}
