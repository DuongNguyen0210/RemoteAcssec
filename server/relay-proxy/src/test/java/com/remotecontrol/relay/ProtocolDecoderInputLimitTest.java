package com.remotecontrol.relay;

import com.remotecontrol.relay.handler.ProtocolDecoder;
import com.remotecontrol.relay.protocol.ProtocolConstants;
import io.netty.buffer.Unpooled;
import io.netty.channel.embedded.EmbeddedChannel;
import io.netty.handler.codec.DecoderException;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class ProtocolDecoderInputLimitTest {
    @Test void rejectsOversizedInputFromHeaderBeforePayloadArrives() {
        var channel = new EmbeddedChannel(new ProtocolDecoder());
        var header = Unpooled.buffer(24)
                .writeInt(ProtocolConstants.PROTOCOL_MAGIC)
                .writeByte(ProtocolConstants.PROTOCOL_VERSION)
                .writeByte(ProtocolConstants.MessageType.MOUSE_MOVE.getValue())
                .writeShort(0).writeInt(256 * 1024).writeLong(1).writeInt(0);
        try {
            assertThrows(DecoderException.class, () -> channel.writeInbound(header));
            assertFalse(channel.isOpen());
        } finally { channel.finishAndReleaseAll(); }
    }

    @Test void rejectsLargeNonScreenControlPacketFromHeader() {
        var channel = new EmbeddedChannel(new ProtocolDecoder());
        var header = Unpooled.buffer(24)
                .writeInt(ProtocolConstants.PROTOCOL_MAGIC)
                .writeByte(ProtocolConstants.PROTOCOL_VERSION)
                .writeByte(ProtocolConstants.MessageType.PING.getValue())
                .writeShort(0).writeInt(9000).writeLong(0).writeInt(0);
        try {
            assertThrows(DecoderException.class, () -> channel.writeInbound(header));
            assertFalse(channel.isOpen());
        } finally { channel.finishAndReleaseAll(); }
    }
}
