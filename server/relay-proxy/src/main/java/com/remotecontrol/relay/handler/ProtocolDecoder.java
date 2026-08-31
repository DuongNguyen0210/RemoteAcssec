package com.remotecontrol.relay.handler;

import com.remotecontrol.relay.protocol.ProtocolHeader;
import com.remotecontrol.relay.protocol.Protocol;
import com.remotecontrol.relay.protocol.ProtocolConstants;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.ByteToMessageDecoder;
import io.netty.handler.codec.CorruptedFrameException;
import io.netty.buffer.ByteBuf;

import java.util.List;

public class ProtocolDecoder extends ByteToMessageDecoder {

    @Override
    protected void decode(ChannelHandlerContext ctx, ByteBuf in, List<Object> out) throws Exception {

        if (in.readableBytes() < ProtocolConstants.HEADER_SIZE) {
            return;
        }

        int readerIndex = in.readerIndex();

        int magic = in.getInt(readerIndex);
        if (magic != ProtocolConstants.PROTOCOL_MAGIC) {
            reject(ctx, "Invalid magic number");
        }

        byte version = in.getByte(readerIndex + 4);
        if (version != ProtocolConstants.PROTOCOL_VERSION) {
            reject(ctx, "Unsupported protocol version: " + version);
        }

        int payloadLength = in.getInt(readerIndex + 8);
        if (payloadLength < 0) {
            reject(ctx, "Negative payload length: " + payloadLength);
        }
        if (payloadLength > ProtocolConstants.MAX_PAYLOAD_LENGTH) {
            reject(ctx, "Payload length exceeds maximum: " + payloadLength);
        }

        long totalLength = (long) ProtocolConstants.HEADER_SIZE + payloadLength;
        if (totalLength > Integer.MAX_VALUE) {
            reject(ctx, "RDTP message length overflows integer range");
        }

        if (in.readableBytes() < (int) totalLength) {
            return;
        }

        in.skipBytes(4); // magic was validated without consuming the header
        in.skipBytes(1); // version was validated without consuming the header
        byte type = in.readByte();
        short flags = in.readShort();
        in.skipBytes(4); // payloadLength was validated above
        long sessionId = in.readLong();
        int sequenceNumber = in.readInt();

        ProtocolHeader header = new ProtocolHeader(
                magic,
                version,
                type,
                flags,
                payloadLength,
                sessionId,
                sequenceNumber);

        byte[] payload = new byte[payloadLength];
        in.readBytes(payload);

        out.add(new Protocol(header, payload));
    }

    private static void reject(ChannelHandlerContext ctx, String message) {
        ctx.close();
        throw new CorruptedFrameException(message);
    }
}
