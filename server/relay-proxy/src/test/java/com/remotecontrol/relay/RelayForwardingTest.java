package com.remotecontrol.relay;

import com.remotecontrol.relay.handler.RelayServerHandler;
import com.remotecontrol.relay.protocol.*;
import com.remotecontrol.relay.registry.RelayRegistry;
import io.netty.channel.DefaultChannelId;
import io.netty.channel.embedded.EmbeddedChannel;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

class RelayForwardingTest {
    @Test void forwardsOnlyActivePeersInAllowedDirectionWithoutChangingPackets() {
        var registry = new RelayRegistry(session -> {});
        var child = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        var admin = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        var stranger = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        try {
            registry.registerChild("child", child);
            var session = registry.createPendingSession(admin, child, "child");
            long id = session.getSessionId();
            assertNull(registry.findActivePeer(id, child));
            assertEquals(0, registry.activeSessionCount());
            registry.acceptPendingSession(id, child);
            assertEquals(1, registry.registeredChildCount());
            assertEquals(1, registry.activeSessionCount());
            for (var type : ProtocolConstants.MessageType.values()) {
                if (type.getValue() < 0x10) continue;
                int inputLength = switch (type) {
                    case MOUSE_MOVE -> 8;
                    case MOUSE_BUTTON_DOWN, MOUSE_BUTTON_UP -> 9;
                    case MOUSE_WHEEL -> 12;
                    case KEY_PRESS, KEY_RELEASE -> 4;
                    default -> 3;
                };
                byte[] payload = new byte[inputLength];
                var packet = new Protocol(new ProtocolHeader(ProtocolConstants.PROTOCOL_MAGIC,
                        ProtocolConstants.PROTOCOL_VERSION, type.getValue(), (short) 3, inputLength, id, 42),
                        payload);
                child.writeInbound(packet);
                if (type == ProtocolConstants.MessageType.SCREEN_FRAME) assertSame(packet, admin.readOutbound());
                else assertNull(admin.readOutbound());
                admin.writeInbound(packet);
                int value = type.getValue();
                if ((value >= 0x20 && value <= 0x23) || value == 0x30 || value == 0x31)
                    assertSame(packet, child.readOutbound());
                else assertNull(child.readOutbound());
                stranger.writeInbound(packet);
                assertNull(child.readOutbound());
                assertNull(admin.readOutbound());
            }
            assertNull(registry.findActivePeer(id + 1, child));
            assertNull(registry.findActivePeer(id, stranger));
        } finally { child.finishAndReleaseAll(); admin.finishAndReleaseAll(); stranger.finishAndReleaseAll(); }
        assertEquals(0, registry.activeSessionCount());
        assertEquals(0, registry.registeredChildCount());
    }

    @Test void acceptsBoundedFrameReferencesAndRejectsMalformedGeometry() {
        for (var type : new ProtocolConstants.MessageType[]{ProtocolConstants.MessageType.MOUSE_MOVE,
                ProtocolConstants.MessageType.MOUSE_BUTTON_DOWN, ProtocolConstants.MessageType.MOUSE_WHEEL}) {
            int base = type == ProtocolConstants.MessageType.MOUSE_MOVE ? 8
                    : type == ProtocolConstants.MessageType.MOUSE_WHEEL ? 12 : 9;
            byte[] payload = new byte[base + 8];
            var geometry = java.nio.ByteBuffer.wrap(payload, base, 8);
            geometry.putInt(960).putInt(540);
            var header = new ProtocolHeader(ProtocolConstants.PROTOCOL_MAGIC,
                    ProtocolConstants.PROTOCOL_VERSION, type.getValue(), (short)0, payload.length, 1, 0);
            var packet = new Protocol(header, payload);
            assertTrue(com.remotecontrol.relay.handler.RelayForwardingPolicy.validInput(packet));
            java.nio.ByteBuffer.wrap(payload, base, 8).putInt(0);
            assertFalse(com.remotecontrol.relay.handler.RelayForwardingPolicy.validInput(packet));
            java.nio.ByteBuffer.wrap(payload, base, 8).putInt(32769);
            assertFalse(com.remotecontrol.relay.handler.RelayForwardingPolicy.validInput(packet));
        }
    }

    @Test void closesAdminSendingOversizedInputInsteadOfForwardingIt() {
        var registry = new RelayRegistry(session -> {});
        var child = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        var admin = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        try {
            registry.registerChild("child", child);
            var session = registry.createPendingSession(admin, child, "child");
            registry.acceptPendingSession(session.getSessionId(), child);
            byte[] oversized = new byte[256 * 1024];
            var packet = new Protocol(new ProtocolHeader(ProtocolConstants.PROTOCOL_MAGIC,
                    ProtocolConstants.PROTOCOL_VERSION, ProtocolConstants.MessageType.MOUSE_MOVE.getValue(),
                    (short) 0, oversized.length, session.getSessionId(), 0), oversized);
            admin.writeInbound(packet);
            assertNull(child.readOutbound());
            assertFalse(admin.isOpen());
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); }
    }

    @Test void dropsScreenFrameUnderAdminBackpressureWithoutClosingSession() {
        var registry = new RelayRegistry(session -> {});
        var child = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        var admin = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        try {
            admin.config().setWriteBufferWaterMark(new io.netty.channel.WriteBufferWaterMark(32, 64));
            registry.registerChild("child", child);
            var session = registry.createPendingSession(admin, child, "child");
            long id = session.getSessionId();
            registry.acceptPendingSession(id, child);

            // Simulate an in-flight outbound frame that makes admin temporarily unwritable
            var holdPromise = admin.newPromise();
            admin.unsafe().outboundBuffer().addMessage(new byte[128], 128, holdPromise);
            admin.unsafe().outboundBuffer().addFlush();
            assertFalse(admin.isWritable());

            var droppedFrame = new Protocol(new ProtocolHeader(ProtocolConstants.PROTOCOL_MAGIC,
                    ProtocolConstants.PROTOCOL_VERSION, ProtocolConstants.MessageType.SCREEN_FRAME.getValue(),
                    (short) 0, 16, id, 1), new byte[16]);
            child.writeInbound(droppedFrame);

            // Session and both channels must stay open; the frame is dropped under backpressure
            assertTrue(child.isOpen());
            assertTrue(admin.isOpen());
            assertEquals(1, registry.activeSessionCount());
            assertNull(admin.readOutbound());

            // Once the outbound buffer drains, fresh frames are forwarded again
            admin.unsafe().outboundBuffer().remove();
            assertTrue(admin.isWritable());
            var freshFrame = new Protocol(new ProtocolHeader(ProtocolConstants.PROTOCOL_MAGIC,
                    ProtocolConstants.PROTOCOL_VERSION, ProtocolConstants.MessageType.SCREEN_FRAME.getValue(),
                    (short) 0, 16, id, 2), new byte[16]);
            child.writeInbound(freshFrame);
            assertSame(freshFrame, admin.readOutbound());
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); }
    }
}
