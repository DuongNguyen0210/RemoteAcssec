package com.remotecontrol.api.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.child.DeviceDto;
import com.remotecontrol.api.dto.common.UserPrincipal;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Collectors;

@Service
@RequiredArgsConstructor
@Slf4j
public class SseNotifier {
    public static final String EVENT_DEVICE_LIST = "device-list";

    private final DeviceService deviceService;
    private final ObjectMapper objectMapper;
    private final ConcurrentHashMap<SseEmitter, Subscription> subscriptions = new ConcurrentHashMap<>();

    public void register(Long ownerId, SseEmitter emitter, UserPrincipal principal, List<DeviceDto> snapshot) {
        subscriptions.put(emitter, new Subscription(emitter, ownerId, principal, deviceStates(snapshot)));
        Runnable cleanup = () -> subscriptions.remove(emitter);
        emitter.onCompletion(cleanup);
        emitter.onTimeout(cleanup);
        emitter.onError(error -> cleanup.run());
    }

    @Scheduled(fixedDelay = 5000)
    public void reconcile() {
        var owners = subscriptions.values().stream().collect(Collectors.groupingBy(s -> s.ownerId));
        for (var entry : owners.entrySet()) {
            try {
                publishChanges(entry.getValue());
            } catch (IOException | RuntimeException error) {
                log.warn("[SSE] Could not reconcile devices for ownerId={}", entry.getKey(), error);
            }
        }
    }

    private void publishChanges(List<Subscription> ownerSubscriptions) throws IOException {
        List<DeviceDto> devices = deviceService.getDevices(ownerSubscriptions.get(0).principal);
        List<DeviceState> state = deviceStates(devices);
        String json = null;
        for (Subscription subscription : ownerSubscriptions) {
            if (!subscriptions.containsKey(subscription.emitter) || subscription.lastState.equals(state)) {
                continue;
            }
            if (json == null) {
                json = objectMapper.writeValueAsString(devices);
            }
            try {
                subscription.emitter.send(SseEmitter.event().name(EVENT_DEVICE_LIST).data(json));
                subscription.lastState = state;
            } catch (IOException | IllegalStateException error) {
                subscriptions.remove(subscription.emitter);
                log.debug("[SSE] Removed failed emitter for ownerId={}", subscription.ownerId, error);
            }
        }
    }

    public long connectionCount(Long ownerId) {
        return subscriptions.values().stream().filter(s -> ownerId.equals(s.ownerId)).count();
    }

    private static List<DeviceState> deviceStates(List<DeviceDto> devices) {
        return devices.stream().map(d -> new DeviceState(d.getSessionId(), d.getChildId(),
                d.getUsername(), d.getDeviceName(), d.getOs(), d.getIpAddress())).toList();
    }

    // Each connection starts with its own initial snapshot. Only the scheduler updates lastState.
    private static final class Subscription {
        private final SseEmitter emitter;
        private final Long ownerId;
        private final UserPrincipal principal;
        private List<DeviceState> lastState;

        private Subscription(SseEmitter emitter, Long ownerId, UserPrincipal principal, List<DeviceState> lastState) {
            this.emitter = emitter;
            this.ownerId = ownerId;
            this.principal = principal;
            this.lastState = lastState;
        }
    }

    // Heartbeat timestamps renew the Redis lease without changing the displayed device state.
    private record DeviceState(String sessionId, Long childId, String username,
                               String deviceName, String os, String ipAddress) {}
}
