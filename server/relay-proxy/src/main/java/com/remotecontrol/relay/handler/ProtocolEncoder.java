package com.remotecontrol.relay.handler;

import com.remotecontrol.relay.protocol.Protocol;
import com.remotecontrol.relay.protocol.ProtocolConstants;
import com.remotecontrol.relay.protocol.ProtocolHeader;
import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.MessageToByteEncoder;

/**
 * Serializes one Protocol object to the 24-byte RDTP header followed by its
 * payload. ByteBuf writes use network byte order (Big Endian).
 */
public class ProtocolEncoder extends MessageToByteEncoder<Protocol> {

    @Override
    protected void encode(ChannelHandlerContext ctx, Protocol msg, ByteBuf out) {
        ProtocolHeader header = msg.getHeader();
        if (header == null) {
            throw new IllegalArgumentException("Protocol header must not be null");
        }

        byte[] payload = msg.getPayload();
        int payloadLength = payload == null ? 0 : payload.length;
        int declaredLength = header.getPayloadLength();

        if (declaredLength < 0 || declaredLength > ProtocolConstants.MAX_PAYLOAD_LENGTH) {
            throw new IllegalArgumentException("Invalid payload length: " + declaredLength);
        }
        if (payloadLength != declaredLength) {
            throw new IllegalArgumentException(
                    "Payload length mismatch: header=" + declaredLength
                            + " actual=" + payloadLength);
        }

        out.writeInt(header.getMagic());
        out.writeByte(header.getVersion());
        out.writeByte(header.getType());
        out.writeShort(header.getFlags());
        out.writeInt(declaredLength);
        out.writeLong(header.getSessionId());
        out.writeInt(header.getSequenceNumber());

        if (payloadLength > 0) {
            out.writeBytes(payload);
        }
    }
}
