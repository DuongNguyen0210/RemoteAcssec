package com.remotecontrol.api.service;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.child.DeviceDto;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Service;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArraySet;

/**
 * Manages Server-Sent Event emitters for ADMIN clients.
 * <p>
 * Each ADMIN user (identified by their DB user-id / ownerId) may have
 * one or more active SSE connections (e.g. multiple browser tabs or
 * re-connections during the 60-second timeout window).  This service
 * keeps a thread-safe registry and fans out device-list snapshots to
 * every live connection belonging to that owner whenever the presence
 * state changes.
 * </p>
 */
@Service
@RequiredArgsConstructor
@Slf4j
public class SseNotifier {

    /** SSE event name the client should listen for. */
    public static final String EVENT_DEVICE_LIST = "device-list";

    private final ObjectMapper objectMapper;

    /**
     * Active emitters keyed by owner (admin) user-id.
     * CopyOnWriteArraySet allows concurrent iteration while send() is in progress.
     */
    private final ConcurrentHashMap<Long, Set<SseEmitter>> registry = new ConcurrentHashMap<>();

    /**
     * Registers a new SseEmitter for the given owner.
     * Completion / timeout / error callbacks automatically unregister it.
     */
    public void register(Long ownerId, SseEmitter emitter) {
        Set<SseEmitter> set = registry.computeIfAbsent(ownerId,
                k -> new CopyOnWriteArraySet<>());
        set.add(emitter);

        Runnable cleanup = () -> unregister(ownerId, emitter);
        emitter.onCompletion(cleanup);
        emitter.onTimeout(cleanup);
        emitter.onError(ex -> cleanup.run());

        log.debug("[SSE] Registered emitter for ownerId={} totalConnections={}",
                ownerId, set.size());
    }

    /**
     * Removes an emitter from the registry (called on completion / timeout / error).
     */
    public void unregister(Long ownerId, SseEmitter emitter) {
        Set<SseEmitter> set = registry.get(ownerId);
        if (set != null) {
            set.remove(emitter);
            if (set.isEmpty()) {
                registry.remove(ownerId, set);
            }
        }
        log.debug("[SSE] Unregistered emitter for ownerId={}", ownerId);
    }

    /**
     * Pushes the current device-list snapshot to all active SSE connections
     * owned by {@code ownerId}.  Emitters that fail to send are cleaned up
     * immediately.
     */
    public void notify(Long ownerId, List<DeviceDto> devices) {
        Set<SseEmitter> set = registry.get(ownerId);
        if (set == null || set.isEmpty()) return;

        String json;
        try {
            json = objectMapper.writeValueAsString(devices);
        } catch (JsonProcessingException e) {
            log.error("[SSE] Failed to serialize device list for ownerId={}", ownerId, e);
            return;
        }

        SseEmitter.SseEventBuilder event = SseEmitter.event()
                .name(EVENT_DEVICE_LIST)
                .data(json);

        for (SseEmitter emitter : set) {
            try {
                emitter.send(event);
            } catch (IOException | IllegalStateException ex) {
                // Connection is broken – remove it; client will reconnect
                log.debug("[SSE] Send failed for ownerId={}, removing emitter: {}", ownerId, ex.getMessage());
                unregister(ownerId, emitter);
            }
        }
    }

    /** Returns the number of active connections for a given owner (useful for monitoring/tests). */
    public int connectionCount(Long ownerId) {
        Set<SseEmitter> set = registry.get(ownerId);
        return set == null ? 0 : set.size();
    }
}
