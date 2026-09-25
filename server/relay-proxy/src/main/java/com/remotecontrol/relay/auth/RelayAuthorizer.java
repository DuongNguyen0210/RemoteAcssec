package com.remotecontrol.relay.auth;

import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;

/** Validates both authentication and current ownership through the management API. */
public class RelayAuthorizer {
    private final HttpClient client = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(3)).build();
    private final String baseUrl;
    private String nodeQuery = "";

    public RelayAuthorizer() {
        this(System.getenv().getOrDefault("MANAGEMENT_API_URL", "http://localhost:9090"));
    }

    public RelayAuthorizer(String baseUrl) { this.baseUrl = baseUrl; }

    public RelayAuthorizer(com.remotecontrol.relay.config.RelayConfig config) {
        this(config.managementApiUrl());
        if (config.discoveryEnabled()) nodeQuery = "?instanceId=" + config.instanceId() + "&bootId=" + config.bootId();
    }

    public CompletableFuture<String> authorize(String token, String targetSessionId) {
        try {
            String path = targetSessionId == null ? "/api/v1/relay/agent"
                    : "/api/v1/relay/targets/" + UUID.fromString(targetSessionId);
            var request = HttpRequest.newBuilder(URI.create(baseUrl + path + nodeQuery))
                    .timeout(Duration.ofSeconds(5)).header("Authorization", "Bearer " + token).GET().build();
            return client.sendAsync(request, HttpResponse.BodyHandlers.ofString()).thenApply(response -> {
                if (response.statusCode() != 200) return null;
                try { return UUID.fromString(response.body().trim()).toString(); }
                catch (IllegalArgumentException invalid) { return null; }
            }).exceptionally(error -> null);
        } catch (IllegalArgumentException invalid) {
            return CompletableFuture.completedFuture(null);
        }
    }
}
