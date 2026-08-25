package com.remotecontrol.relay.handler;

import com.remotecontrol.relay.protocol.Protocol;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;

public class RelayServerHandler extends SimpleChannelInboundHandler<Protocol> {

    @Override
    protected void channelRead0(ChannelHandlerContext ctx, Protocol msg) throws Exception {
        System.out.println("[RelayServer] Nhận được gói tin từ Client: " + ctx.channel().remoteAddress());
        System.out.println("Nội dung Header: " + msg.toString());

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
