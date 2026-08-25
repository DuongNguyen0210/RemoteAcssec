package com.remotecontrol.relay.handler;

import com.remotecontrol.relay.protocol.ProtocolHeader;
import com.remotecontrol.relay.protocol.Protocol;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.ByteToMessageDecoder;
import io.netty.buffer.ByteBuf;

import java.util.List;

public class ProtocolDecoder extends ByteToMessageDecoder{

    @Override
    protected void decode(ChannelHandlerContext ctx, ByteBuf in, List<Object> out) throws Exception {

        if(in.readableBytes() < ProtocolHeader.HEADER_LENGTH) {
            return;
        }

        int pLen = in.getInt(in.readerIndex() + 8);
        int total = ProtocolHeader.HEADER_LENGTH + pLen;

        if(in.readableBytes() < total) {
            return;
        }

        in.markReaderIndex();

        int magic = in.readInt();
        if(magic != ProtocolHeader.MAGIC) {
            ctx.close();
            throw new RuntimeException("Invalid magic number");
        }

        byte version = in.readByte();
        byte type = in.readByte();
        short flags = in.readShort();
        in.readInt();
        long sessionId = in.readLong();
        int sequenceNumber = in.readInt();

        ProtocolHeader header = new ProtocolHeader(magic, version, type, flags, pLen, sessionId, sequenceNumber);

        byte[] payload = new byte[pLen];
        in.readBytes(payload);

        out.add(new Protocol(header, payload));
    }
}
