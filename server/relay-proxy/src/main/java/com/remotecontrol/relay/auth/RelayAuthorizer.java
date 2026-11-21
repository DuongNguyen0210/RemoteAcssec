package com.remotecontrol.relay.auth;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Validates both authentication and current ownership through the management API. */
public class RelayAuthorizer {
    private static final Duration REQUEST_TIMEOUT = Duration.ofSeconds(5);
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
                    .timeout(REQUEST_TIMEOUT).header("Authorization", "Bearer " + token).GET().build();
            var responseFuture = client.sendAsync(request, HttpResponse.BodyHandlers.ofString());
            // Keep the original cancellable if the response headers arrive but its body stalls.
            return responseFuture.copy()
                    .orTimeout(REQUEST_TIMEOUT.toMillis(), TimeUnit.MILLISECONDS)
                    .whenComplete((response, failure) -> {
                        if (failure instanceof TimeoutException) responseFuture.cancel(true);
                    })
                    .thenApply(response -> {
                        int status = response.statusCode();
                        if (status >= 500 && status < 600) {
                            throw new CompletionException(new IOException("Authorization endpoint returned HTTP " + status));
                        }
                        if (status != 200) return null;
                        try {
                            String body = response.body().trim();
                            String id = UUID.fromString(body).toString();
                            return id.equalsIgnoreCase(body) ? id : null;
                        } catch (IllegalArgumentException invalid) {
                            return null;
                        }
                    });
        } catch (IllegalArgumentException invalid) {
            return CompletableFuture.completedFuture(null);
        }
    }
}
