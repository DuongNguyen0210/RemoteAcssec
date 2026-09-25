package com.remotecontrol.api.dto.relay;

/** expiresAt is epoch milliseconds; clients must renew before expiry. */
public record RelayEndpointDto(String instanceId, String host, int port, long expiresAt) {}
