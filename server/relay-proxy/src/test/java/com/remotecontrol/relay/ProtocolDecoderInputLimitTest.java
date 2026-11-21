package com.remotecontrol.relay;

import com.remotecontrol.relay.handler.ProtocolDecoder;
import com.remotecontrol.relay.protocol.ProtocolConstants;
import io.netty.buffer.Unpooled;
import io.netty.channel.embedded.EmbeddedChannel;
import io.netty.handler.codec.DecoderException;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class ProtocolDecoderInputLimitTest {
    @Test void decodesFrameReferenceInputAcrossTcpFragments() {
        var channel = new EmbeddedChannel(new ProtocolDecoder());
        var packet = Unpooled.buffer(40)
                .writeInt(ProtocolConstants.PROTOCOL_MAGIC)
                .writeByte(ProtocolConstants.PROTOCOL_VERSION)
                .writeByte(ProtocolConstants.MessageType.MOUSE_MOVE.getValue())
                .writeShort(0).writeInt(16).writeLong(1).writeInt(0)
                .writeInt(480).writeInt(270).writeInt(960).writeInt(540);
        try {
            assertFalse(channel.writeInbound(packet.readRetainedSlice(24)));
            assertTrue(channel.isOpen());
            assertTrue(channel.writeInbound(packet.readRetainedSlice(16)));
            var decoded = (com.remotecontrol.relay.protocol.Protocol) channel.readInbound();
            assertTrue(com.remotecontrol.relay.handler.RelayForwardingPolicy.validInput(decoded));
        } finally { packet.release(); channel.finishAndReleaseAll(); }
    }

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

    @Test void rejectsScreenAcknowledgementWithPayloadFromHeader() {
        var channel = new EmbeddedChannel(new ProtocolDecoder());
        var header = Unpooled.buffer(24)
                .writeInt(ProtocolConstants.PROTOCOL_MAGIC)
                .writeByte(ProtocolConstants.PROTOCOL_VERSION)
                .writeByte(0x11)
                .writeShort(0).writeInt(1).writeLong(1).writeInt(1);
        try {
            assertThrows(DecoderException.class, () -> channel.writeInbound(header));
            assertFalse(channel.isOpen());
        } finally { channel.finishAndReleaseAll(); }
    }
}
