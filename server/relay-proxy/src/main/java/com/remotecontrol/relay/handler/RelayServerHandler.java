package com.remotecontrol.relay.handler;

import com.remotecontrol.relay.protocol.Protocol;
import com.remotecontrol.relay.protocol.ProtocolConstants;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class RelayServerHandler extends SimpleChannelInboundHandler<Protocol> {

    /** MOUSE_MOVE payload: x (int32 BE) + y (int32 BE) */
    private static final int MOUSE_MOVE_PAYLOAD_SIZE = 8;

    @Override
    protected void channelRead0(ChannelHandlerContext ctx, Protocol msg) throws Exception {
        if (msg.getHeader() != null
                && msg.getHeader().getType() == ProtocolConstants.MessageType.MOUSE_MOVE.getValue()) {
            handleMouseMove(ctx, msg);
            return;
        }

        System.out.println("[RelayServer] Nhận được gói tin từ Client: " + ctx.channel().remoteAddress());
        System.out.println("Nội dung Header: " + msg.toString());

    }

    private void handleMouseMove(ChannelHandlerContext ctx, Protocol msg) {
        byte[] payload = msg.getPayload();
        int length = (payload != null) ? payload.length : 0;

        if (length != MOUSE_MOVE_PAYLOAD_SIZE) {
            System.err.println("[Relay] MOUSE_MOVE payload không hợp lệ: payloadLength=" + length
                    + " (mong đợi " + MOUSE_MOVE_PAYLOAD_SIZE + ") từ " + ctx.channel().remoteAddress()
                    + " -> bỏ qua gói tin");
            return;
        }

        ByteBuffer buffer = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN);
        int x = buffer.getInt();
        int y = buffer.getInt();

        System.out.println("[Relay] MOUSE_MOVE x=" + x + " y=" + y);
    }

    @Override
    public void channelActive(ChannelHandlerContext ctx) throws Exception {
        System.out.println("[RelayServer] Có kết nối mới: " + ctx.channel().remoteAddress());
        super.channelActive(ctx);
    }

    @Override
    public void channelInactive(ChannelHandlerContext ctx) throws Exception {
        System.out.println("[RelayServer] Kết nối đã đóng: " + ctx.channel().remoteAddress());
        super.channelInactive(ctx);
    }

    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) throws Exception {
        System.err.println("[RelayServer] Lỗi kết nối: " + cause.getMessage());
        ctx.close();
    }
}
