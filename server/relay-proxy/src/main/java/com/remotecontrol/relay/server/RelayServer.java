package com.remotecontrol.relay.server;

import com.remotecontrol.relay.handler.ProtocolDecoder;
import com.remotecontrol.relay.handler.ProtocolEncoder;
import com.remotecontrol.relay.handler.RelayServerHandler;
import com.remotecontrol.relay.protocol.Protocol;
import com.remotecontrol.relay.protocol.ProtocolConstants;
import com.remotecontrol.relay.registry.RelayRegistry;
import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelInitializer;
import io.netty.channel.ChannelOption;
import io.netty.channel.DefaultMessageSizeEstimator;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.MessageSizeEstimator;
import io.netty.channel.WriteBufferWaterMark;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.SocketChannel;
import io.netty.channel.socket.nio.NioServerSocketChannel;

public class RelayServer {

    private static final WriteBufferWaterMark RELAY_WRITE_BUFFER_WATER_MARK =
            new WriteBufferWaterMark(512 * 1024, 2 * 1024 * 1024);

    private static final MessageSizeEstimator RDTP_MESSAGE_SIZE_ESTIMATOR =
            new MessageSizeEstimator() {
                @Override
                public MessageSizeEstimator.Handle newHandle() {
                    final MessageSizeEstimator.Handle fallback =
                            DefaultMessageSizeEstimator.DEFAULT.newHandle();
                    return msg -> {
                        if (msg instanceof Protocol) {
                            byte[] payload = ((Protocol) msg).getPayload();
                            return ProtocolConstants.HEADER_SIZE
                                    + (payload == null ? 0 : payload.length);
                        }
                        return fallback.size(msg);
                    };
                }
            };

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
             .childOption(ChannelOption.SO_KEEPALIVE, true)
             .childOption(ChannelOption.WRITE_BUFFER_WATER_MARK,
                     RELAY_WRITE_BUFFER_WATER_MARK)
             .childOption(ChannelOption.MESSAGE_SIZE_ESTIMATOR,
                     RDTP_MESSAGE_SIZE_ESTIMATOR);

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
