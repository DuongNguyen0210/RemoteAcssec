package com.remotecontrol.relay.server;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.relay.config.RelayConfig;
import com.remotecontrol.relay.registry.RelayRegistry;
import java.net.URI;
import java.net.http.*;
import java.time.Duration;
import java.util.Map;
import java.util.concurrent.*;

/** Runs blocking management I/O on its own thread, never on a Netty event loop. */
final class RelayNodeReporter implements AutoCloseable {
    private final RelayConfig config;
    private final RelayRegistry registry;
    private final ObjectMapper mapper = new ObjectMapper();
    private final HttpClient client = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(3)).build();
    private final ScheduledExecutorService executor = Executors.newSingleThreadScheduledExecutor(r -> {
        Thread thread = new Thread(r, "relay-node-reporter");
        thread.setDaemon(true);
        return thread;
    });
    private boolean registered;

    RelayNodeReporter(RelayConfig config, RelayRegistry registry) { this.config = config; this.registry = registry; }

    void start() {
        executor.scheduleWithFixedDelay(this::report, 0, config.heartbeatSeconds(), TimeUnit.SECONDS);
    }

    private void report() {
        try {
            if (!registered) {
                int status = post("", Map.of("instanceId", config.instanceId(), "bootId", config.bootId(),
                        "advertisedHost", config.advertisedHost(), "advertisedPort", config.advertisedPort(),
                        "capacity", config.capacity()));
                registered = status == 204;
                if (!registered) { System.err.println("Relay registration failed: HTTP " + status); return; }
            }
            int status = post("/" + config.instanceId() + "/heartbeat", Map.of("bootId", config.bootId(),
                    "registeredChildren", registry.registeredChildCount(), "activeSessions", registry.activeSessionCount()));
            if (status != 204) {
                registered = false;
                System.err.println("Relay heartbeat failed: HTTP " + status);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        } catch (Exception e) {
            registered = false;
            System.err.println("Relay discovery unavailable: " + e.getClass().getSimpleName());
        }
    }

    private int post(String path, Object body) throws Exception {
        var request = HttpRequest.newBuilder(URI.create(config.managementApiUrl() + "/internal/relay/nodes" + path))
                .timeout(Duration.ofSeconds(5)).header("Content-Type", "application/json")
                .header("X-Relay-Token", config.nodeToken())
                .POST(HttpRequest.BodyPublishers.ofString(mapper.writeValueAsString(body))).build();
        return client.send(request, HttpResponse.BodyHandlers.discarding()).statusCode();
    }

    @Override public void close() { executor.shutdownNow(); }
}
