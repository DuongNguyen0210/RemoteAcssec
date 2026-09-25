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

    private final com.remotecontrol.relay.config.RelayConfig config;

    public RelayServer(com.remotecontrol.relay.config.RelayConfig config) { this.config = config; }

    public void start() throws InterruptedException {

        EventLoopGroup bossGroup = new NioEventLoopGroup(1);
        EventLoopGroup workerGroup = new NioEventLoopGroup();
        RelayRegistry relayRegistry = new RelayRegistry();
        var authorizer = new com.remotecontrol.relay.auth.RelayAuthorizer(config);
        var reporter = new RelayNodeReporter(config, relayRegistry);

        try {

            ServerBootstrap b = new ServerBootstrap();

            b.group(bossGroup, workerGroup)
             .channel(NioServerSocketChannel.class)
             .childHandler(new ChannelInitializer<SocketChannel>() {
                 @Override
                 protected void initChannel(SocketChannel ch) {
                     ch.pipeline().addLast(new ProtocolDecoder());
                     ch.pipeline().addLast(new ProtocolEncoder());
                     ch.pipeline().addLast(new RelayServerHandler(relayRegistry, authorizer));
                 }
             })
             .option(ChannelOption.SO_BACKLOG, 128)
             .childOption(ChannelOption.SO_KEEPALIVE, true)
             .childOption(ChannelOption.WRITE_BUFFER_WATER_MARK,
                     new io.netty.channel.WriteBufferWaterMark(1024 * 1024, 2 * 1024 * 1024));

            ChannelFuture f = b.bind(config.port()).sync();
            if (config.discoveryEnabled()) reporter.start();
            f.channel().closeFuture().sync();
            
        }
        finally {
            reporter.close();
            workerGroup.shutdownGracefully();
            bossGroup.shutdownGracefully();
        }
    }

    public static void main(String[] args) throws Exception {
        new RelayServer(com.remotecontrol.relay.config.RelayConfig.fromEnvironment()).start();
    }
}
