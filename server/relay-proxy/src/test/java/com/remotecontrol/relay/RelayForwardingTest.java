package com.remotecontrol.relay;

import com.remotecontrol.relay.auth.RelayAuthorizer;
import com.remotecontrol.relay.handler.RelayServerHandler;
import com.remotecontrol.relay.protocol.*;
import com.remotecontrol.relay.registry.RelayRegistry;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelOutboundHandlerAdapter;
import io.netty.channel.DefaultChannelId;
import io.netty.channel.WriteBufferWaterMark;
import io.netty.channel.embedded.EmbeddedChannel;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.CompletableFuture;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

class RelayForwardingTest {
    private static final byte SCREEN_FRAME_ACK = 0x11;
    private static final String CHILD_ID = "00000000-0000-0000-0000-000000000001";

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

    @Test void forwardsAcknowledgementsOnlyFromTheNegotiatedActiveAdmin() {
        var registry = new RelayRegistry();
        var child = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        var admin = authorizedAdmin(registry);
        var stranger = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        try {
            var advertise = packet(SCREEN_FRAME_ACK, 1, 0, 0, new byte[0]);
            child.writeInbound(advertise);
            admin.writeInbound(advertise);
            assertTrue(registry.registerChild(CHILD_ID, child));
            admin.writeInbound(connectRequest());
            Protocol capability = child.readOutbound();
            Protocol request = child.readOutbound();
            assertNotNull(request, "The Child must receive its capability before SESSION_REQUEST");
            assertEquals(ProtocolConstants.MessageType.SESSION_REQUEST.getValue(), request.getHeader().getType());
            long id = request.getHeader().getSessionId();
            assertCapability(capability, id);
            var ack = packet(SCREEN_FRAME_ACK, 0, id, 0x80000000, new byte[0]);
            admin.writeInbound(ack);
            assertNull(child.readOutbound());
            assertNull(admin.readOutbound());

            child.writeInbound(packet(ProtocolConstants.MessageType.SESSION_ACCEPT.getValue(), 0, id, 0, new byte[0]));
            Protocol result = admin.readOutbound();
            assertEquals(ProtocolConstants.MessageType.CONNECT_RESULT.getValue(), result.getHeader().getType());
            assertCapability(admin.readOutbound(), id);
            assertNull(child.readOutbound());

            child.writeInbound(ack);
            stranger.writeInbound(ack);
            admin.writeInbound(packet(SCREEN_FRAME_ACK, 1, id, 0, new byte[0]));
            admin.writeInbound(packet(SCREEN_FRAME_ACK, 0, id, 1, new byte[1]));
            assertNull(child.readOutbound());
            assertNull(admin.readOutbound());

            admin.writeInbound(ack);
            assertSame(ack, child.readOutbound());
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); stranger.finishAndReleaseAll(); }
    }

    @Test void aLegacyPeerKeepsTheOriginalSessionHandshake() {
        var registry = new RelayRegistry();
        var child = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        var admin = authorizedAdmin(registry);
        try {
            admin.writeInbound(packet(SCREEN_FRAME_ACK, 1, 0, 0, new byte[0]));
            assertTrue(registry.registerChild(CHILD_ID, child));
            admin.writeInbound(connectRequest());
            Protocol request = child.readOutbound();
            assertEquals(ProtocolConstants.MessageType.SESSION_REQUEST.getValue(), request.getHeader().getType());
            assertNull(child.readOutbound());
            long id = request.getHeader().getSessionId();
            child.writeInbound(packet(ProtocolConstants.MessageType.SESSION_ACCEPT.getValue(), 0, id, 0, new byte[0]));
            Protocol result = admin.readOutbound();
            assertEquals(ProtocolConstants.MessageType.CONNECT_RESULT.getValue(), result.getHeader().getType());
            child.writeInbound(packet(SCREEN_FRAME_ACK, 1, 0, 0, new byte[0]));
            admin.writeInbound(packet(SCREEN_FRAME_ACK, 0, id, 1, new byte[0]));
            assertNull(admin.readOutbound());
            assertNull(child.readOutbound());
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); }
    }

    @Test void temporaryChildBackpressureAllowsOrderedInputToDrain() {
        var registry = new RelayRegistry();
        var child = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        var admin = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        try {
            child.config().setWriteBufferWaterMark(new WriteBufferWaterMark(64 * 1024, 256 * 1024));
            assertTrue(registry.registerChild("child", child));
            var session = registry.createPendingSession(admin, child, "child");
            long id = session.getSessionId();
            registry.acceptPendingSession(id, child);

            byte[] pending = new byte[256 * 1024 + 1];
            child.unsafe().outboundBuffer().addMessage(pending, pending.length, child.newPromise());
            child.unsafe().outboundBuffer().addFlush();
            assertFalse(child.isWritable());

            var press = packet(ProtocolConstants.MessageType.KEY_PRESS.getValue(), 0, id, 0, new byte[4]);
            var release = packet(ProtocolConstants.MessageType.KEY_RELEASE.getValue(), 0, id, 0, new byte[4]);
            var move = packet(ProtocolConstants.MessageType.MOUSE_MOVE.getValue(), 0, id, 0, new byte[8]);
            admin.writeInbound(move, press, release);

            assertEquals(1, registry.activeSessionCount());
            assertSame(pending, child.readOutbound());
            assertSame(press, child.readOutbound());
            assertSame(release, child.readOutbound());
            assertNull(child.readOutbound());
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); }
    }

    @Test void reservesAtMostOnePacedScreenWriteUnderBackpressure() {
        class HoldFlush extends ChannelOutboundHandlerAdapter {
            boolean holding;

            @Override public void flush(ChannelHandlerContext ctx) {
                if (!holding) ctx.flush();
            }
        }
        var block = new HoldFlush();
        var registry = new RelayRegistry();
        var child = new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry));
        var admin = new EmbeddedChannel(DefaultChannelId.newInstance(), block, new RelayServerHandler(registry));
        try {
            admin.config().setWriteBufferWaterMark(new WriteBufferWaterMark(32, 64));
            var advertise = packet(SCREEN_FRAME_ACK, 1, 0, 0, new byte[0]);
            child.writeInbound(advertise);
            admin.writeInbound(advertise);
            assertTrue(registry.registerChild("child", child));
            long id = registry.createPendingSession(admin, child, "child").getSessionId();
            child.writeInbound(packet(ProtocolConstants.MessageType.SESSION_ACCEPT.getValue(), 0, id, 0, new byte[0]));
            admin.readOutbound();
            assertCapability(admin.readOutbound(), id);

            byte[] pending = new byte[128];
            admin.unsafe().outboundBuffer().addMessage(pending, pending.length, admin.newPromise());
            admin.unsafe().outboundBuffer().addFlush();
            assertFalse(admin.isWritable());
            block.holding = true;
            byte screenFrame = ProtocolConstants.MessageType.SCREEN_FRAME.getValue();
            var first = packet(screenFrame, 0, id, 0, new byte[16]);
            var second = packet(screenFrame, 0, id, 1, new byte[16]);
            child.writeInbound(first, second);
            assertNull(admin.readOutbound());
            assertEquals(1, registry.activeSessionCount());

            block.holding = false;
            admin.flushOutbound();
            assertSame(pending, admin.readOutbound());
            assertSame(first, admin.readOutbound());
            assertNull(admin.readOutbound());
            child.writeInbound(second);
            assertSame(second, admin.readOutbound());
        } finally { admin.finishAndReleaseAll(); child.finishAndReleaseAll(); }
    }

    private static Protocol packet(byte type, int flags, long session, int sequence, byte[] payload) {
        return new Protocol(new ProtocolHeader(ProtocolConstants.PROTOCOL_MAGIC, ProtocolConstants.PROTOCOL_VERSION,
                type, (short) flags, payload.length, session, sequence), payload);
    }

    private static EmbeddedChannel authorizedAdmin(RelayRegistry registry) {
        var authorizer = new RelayAuthorizer() {
            @Override public CompletableFuture<String> authorize(String token, String target) {
                return CompletableFuture.completedFuture(target);
            }
        };
        return new EmbeddedChannel(DefaultChannelId.newInstance(), new RelayServerHandler(registry, authorizer));
    }

    private static Protocol connectRequest() {
        byte[] target = CHILD_ID.getBytes(StandardCharsets.UTF_8);
        byte[] payload = ByteBuffer.allocate(5 + target.length)
                .putShort((short) 1).put((byte) 'x').putShort((short) target.length).put(target).array();
        return packet(ProtocolConstants.MessageType.CONNECT_REQUEST.getValue(), 0, 0, 0, payload);
    }

    private static void assertCapability(Protocol capability, long session) {
        assertNotNull(capability);
        ProtocolHeader header = capability.getHeader();
        assertEquals(SCREEN_FRAME_ACK, header.getType());
        assertEquals(1, header.getFlags());
        assertEquals(session, header.getSessionId());
        assertEquals(0, header.getSequenceNumber());
        assertEquals(0, capability.getPayload().length);
    }
}
