package com.remotecontrol.relay;

import com.remotecontrol.relay.config.RelayConfig;
import org.junit.jupiter.api.Test;
import java.util.Map;
import static org.junit.jupiter.api.Assertions.*;

class RelayConfigTest {
    @Test void localDefaultsAndExplicitDiscoveryConfiguration() {
        var local = RelayConfig.fromEnvironment(Map.of());
        assertEquals(8080, local.port());
        assertEquals("localhost", local.advertisedHost());
        assertFalse(local.discoveryEnabled());
        var config = RelayConfig.fromEnvironment(Map.of("RELAY_INSTANCE_ID", "node-1", "RELAY_PORT", "8081",
                "RELAY_ADVERTISED_PORT", "9091", "RELAY_ADVERTISED_HOST", "relay.example.test",
                "RELAY_DISCOVERY_ENABLED", "true", "RELAY_NODE_TOKEN", "test-token"));
        assertEquals(9091, config.advertisedPort());
        assertEquals(8081, config.port());
        assertTrue(config.discoveryEnabled());
        assertNotEquals(local.bootId(), config.bootId());
    }
    @Test void rejectsInvalidSettingsBeforeBinding() {
        assertThrows(IllegalArgumentException.class, () -> RelayConfig.fromEnvironment(Map.of("RELAY_PORT", "0")));
        assertThrows(IllegalArgumentException.class, () -> RelayConfig.fromEnvironment(Map.of("RELAY_PORT", "bad")));
        assertThrows(IllegalArgumentException.class, () -> RelayConfig.fromEnvironment(Map.of("RELAY_DISCOVERY_ENABLED", "true")));
        assertThrows(IllegalArgumentException.class, () -> RelayConfig.fromEnvironment(Map.of("RELAY_HEARTBEAT_SECONDS", "31")));
    }
}
