package com.remotecontrol.relay.config;

import java.net.URI;
import java.util.Map;
import java.util.UUID;

public record RelayConfig(String instanceId, String bootId, int port, String advertisedHost,
        int advertisedPort, String managementApiUrl, String nodeToken, int capacity,
        int heartbeatSeconds, boolean discoveryEnabled) {
    public RelayConfig {
        if (!instanceId.matches("[A-Za-z0-9_-]{1,64}") || !bootId.matches("[A-Za-z0-9-]{1,64}"))
            throw new IllegalArgumentException("Invalid relay identity");
        if (port < 1 || port > 65535 || advertisedPort < 1 || advertisedPort > 65535)
            throw new IllegalArgumentException("Invalid relay port");
        if (advertisedHost.isBlank() || advertisedHost.length() > 253 || advertisedHost.contains("/"))
            throw new IllegalArgumentException("Invalid advertised host");
        URI uri = URI.create(managementApiUrl);
        if (!("http".equals(uri.getScheme()) || "https".equals(uri.getScheme())) || uri.getHost() == null)
            throw new IllegalArgumentException("Invalid management API URL");
        if (capacity < 1 || capacity > 100000 || heartbeatSeconds < 1 || heartbeatSeconds > 10)
            throw new IllegalArgumentException("Invalid capacity or heartbeat interval");
        if (discoveryEnabled && nodeToken.isBlank())
            throw new IllegalArgumentException("RELAY_NODE_TOKEN is required for discovery");
        managementApiUrl = managementApiUrl.replaceAll("/+$", "");
    }

    public static RelayConfig fromEnvironment() { return fromEnvironment(System.getenv()); }

    public static RelayConfig fromEnvironment(Map<String, String> env) {
        int port = integer(env, "RELAY_PORT", 8080);
        return new RelayConfig(env.getOrDefault("RELAY_INSTANCE_ID", "relay-local"), UUID.randomUUID().toString(),
                port, env.getOrDefault("RELAY_ADVERTISED_HOST", "localhost"),
                integer(env, "RELAY_ADVERTISED_PORT", port),
                env.getOrDefault("MANAGEMENT_API_URL", "http://localhost:9090"),
                env.getOrDefault("RELAY_NODE_TOKEN", ""), integer(env, "RELAY_CAPACITY", 1000),
                integer(env, "RELAY_HEARTBEAT_SECONDS", 5),
                Boolean.parseBoolean(env.getOrDefault("RELAY_DISCOVERY_ENABLED", "false")));
    }

    private static int integer(Map<String, String> env, String key, int fallback) {
        try { return Integer.parseInt(env.getOrDefault(key, Integer.toString(fallback))); }
        catch (NumberFormatException e) { throw new IllegalArgumentException("Invalid " + key, e); }
    }
}
