package com.remotecontrol.relay.server;

import com.remotecontrol.relay.handler.ProtocolDecoder;
import com.remotecontrol.relay.handler.ProtocolEncoder;
import com.remotecontrol.relay.handler.RelayServerHandler;
import com.remotecontrol.relay.registry.RelayRegistry;
import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;

public class RelayServer {

    private final int port;

    public RelayServer(int port) {
        this.port = port;
    }

    public void start() throws InterruptedException {

        EventLoopGroup bossGroup = new NioEventLoopGroup(1);
        EventLoopGroup workerGroup = new NioEventLoopGroup();
        RelayRegistry relayRegistry = new RelayRegistry();

        try {

            ServerBootstrap b = new ServerBootstrap();

            b.group(bossGroup, workerGroup)
             .channel(NioServerSocketChannel.class)
             .childHandler(new ChannelInitializer<SocketChannel>() {
                 @Override
                 protected void initChannel(SocketChannel ch) {
                     ch.pipeline().addLast(new ProtocolDecoder());
                     ch.pipeline().addLast(new ProtocolEncoder());
                     ch.pipeline().addLast(new RelayServerHandler(relayRegistry));
                 }
             })
             .option(ChannelOption.SO_BACKLOG, 128)
             .childOption(ChannelOption.SO_KEEPALIVE, true);

            ChannelFuture f = b.bind(port).sync();
            f.channel().closeFuture().sync();
            
        }
        finally {
            workerGroup.shutdownGracefully();
            bossGroup.shutdownGracefully();
        }
    }

    public static void main(String[] args) throws Exception {
        int port = 8080;
        new RelayServer(port).start();
    }
}
