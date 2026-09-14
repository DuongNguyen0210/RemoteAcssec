package com.remotecontrol.relay;

import com.remotecontrol.relay.auth.RelayAuthPayload;
import com.remotecontrol.relay.protocol.*;
import com.remotecontrol.relay.registry.RelayRegistry;
import io.netty.channel.embedded.EmbeddedChannel;
import io.netty.channel.DefaultChannelId;
import org.junit.jupiter.api.Test;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.UUID;
import static org.junit.jupiter.api.Assertions.*;

class RelaySessionTest {
    @Test void sameAccountSessionsRouteIndependentlyAndDuplicateCannotReplaceChannel() {
        var registry = new RelayRegistry();
        var laptop = new EmbeddedChannel(DefaultChannelId.newInstance());
        var pc = new EmbeddedChannel(DefaultChannelId.newInstance());
        var admin = new EmbeddedChannel(DefaultChannelId.newInstance());
        try {
            assertTrue(registry.registerChild("session-laptop", laptop));
            assertTrue(registry.registerChild("session-pc", pc));
            assertFalse(registry.registerChild("session-laptop", pc));
            assertSame(laptop, registry.findRegisteredChild("session-laptop"));
            var session = registry.createPendingSession(admin, pc, "session-pc");
            assertNotNull(session);
            assertNull(registry.acceptPendingSession(session.getSessionId(), laptop));
            assertNotNull(registry.acceptPendingSession(session.getSessionId(), pc));
            registry.unregisterChild(laptop);
            assertSame(pc, registry.findRegisteredChild("session-pc"));
        } finally { laptop.finishAndReleaseAll(); pc.finishAndReleaseAll(); admin.finishAndReleaseAll(); }
    }

    @Test void authenticatedPayloadRejectsTruncationAndTrailingBytes() {
        String sid = UUID.randomUUID().toString();
        byte[] token = "jwt-token".getBytes(StandardCharsets.UTF_8);
        byte[] target = sid.getBytes(StandardCharsets.UTF_8);
        byte[] payload = ByteBuffer.allocate(4 + token.length + target.length)
                .putShort((short)token.length).put(token).putShort((short)target.length).put(target).array();
        var header = new ProtocolHeader(ProtocolConstants.PROTOCOL_MAGIC, ProtocolConstants.PROTOCOL_VERSION,
                ProtocolConstants.MessageType.CONNECT_REQUEST.getValue(), (short)0, payload.length, 0, 0);
        var decoded = RelayAuthPayload.decode(new Protocol(header, payload), true);
        assertNotNull(decoded);
        assertEquals(sid, decoded.targetSessionId());
        assertEquals("jwt-token", decoded.token());
        assertNull(RelayAuthPayload.decode(new Protocol(header, java.util.Arrays.copyOf(payload, payload.length - 1)), true));
        assertNull(RelayAuthPayload.decode(new Protocol(header, payload), false));
    }
}
