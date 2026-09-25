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
        var registry = new RelayRegistry();
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
                var packet = new Protocol(new ProtocolHeader(ProtocolConstants.PROTOCOL_MAGIC,
                        ProtocolConstants.PROTOCOL_VERSION, type.getValue(), (short) 3, 3, id, 42),
                        new byte[]{0, (byte)255, 1});
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
}
