package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.common.UserPrincipal;
import jakarta.annotation.PreDestroy;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;
import org.springframework.web.server.ResponseStatusException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import java.util.Set;
import java.util.concurrent.*;

/** Coalesces heartbeats into one authorized snapshot per subscribed owner per tick. */
@Service
public class DevicePresencePublisher implements AutoCloseable {
    private static final Logger log = LoggerFactory.getLogger(DevicePresencePublisher.class);
    private final DeviceService devices;
    private final SseNotifier notifier;
    private final Set<Long> pending = ConcurrentHashMap.newKeySet();
    private final ThreadPoolExecutor workers = new ThreadPoolExecutor(4, 4, 0, TimeUnit.SECONDS,
            new ArrayBlockingQueue<>(256), task -> {
                Thread thread = new Thread(task, "presence-publisher");
                thread.setDaemon(true);
                return thread;
            });

    public DevicePresencePublisher(DeviceService devices, SseNotifier notifier) {
        this.devices = devices;
        this.notifier = notifier;
    }

    @Scheduled(fixedDelay = 3000)
    public void publish() {
        for (Long ownerId : notifier.subscribedOwners()) {
            if (!pending.add(ownerId)) continue;
            try {
                workers.execute(() -> {
                    try {
                        if (notifier.connectionCount(ownerId) == 0) return;
                        var principal = UserPrincipal.builder().id(ownerId.toString()).role("ADMIN").build();
                        notifier.notify(ownerId, devices.getDevices(principal));
                    } catch (ResponseStatusException ex) {
                        if (ex.getStatusCode().value() == 401) notifier.closeOwner(ownerId);
                        else log.warn("Presence snapshot rejected for owner {}", ownerId);
                    } catch (RuntimeException ex) {
                        // Do not send an empty list on storage failure: the client's cached data is still useful.
                        log.warn("Presence snapshot unavailable for owner {}: {}", ownerId, ex.getClass().getSimpleName());
                    } finally {
                        pending.remove(ownerId);
                    }
                });
            } catch (RejectedExecutionException ex) {
                pending.remove(ownerId); // Retry current state next tick; never queue obsolete snapshots.
            }
        }
    }

    @Override
    @PreDestroy
    public void close() { workers.shutdownNow(); }
}
