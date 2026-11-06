package com.remotecontrol.api.controller;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.annotation.RequireRole;
import com.remotecontrol.api.dto.child.DeviceDto;
import com.remotecontrol.api.dto.common.ApiResponse;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.service.DeviceService;
import com.remotecontrol.api.service.SseNotifier;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestAttribute;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.util.List;

@RestController
@RequestMapping("/api/v1/devices")
@RequiredArgsConstructor
@Slf4j
public class DeviceController {

    /** SSE connection lifetime (ms). Client must reconnect after this. */
    private static final long SSE_TIMEOUT_MS = 60_000L;

    private final DeviceService deviceService;
    private final RelayAuthorizationController authorization;
    private final com.remotecontrol.api.service.RelayDispatchService dispatch;
    private final SseNotifier sseNotifier;
    private final ObjectMapper objectMapper;

    @GetMapping("/{agentSessionId}/relay")
    @RequireRole("ADMIN")
    public ApiResponse<com.remotecontrol.api.dto.relay.RelayEndpointDto> relay(
            @RequestAttribute("currentUser") UserPrincipal principal,
            @org.springframework.web.bind.annotation.PathVariable java.util.UUID agentSessionId) {
        authorization.target(principal, agentSessionId);
        return ApiResponse.success(dispatch.lookup(agentSessionId.toString()));
    }

    @GetMapping
    @RequireRole("ADMIN")
    public ApiResponse<List<DeviceDto>> list(@RequestAttribute("currentUser") UserPrincipal principal) {
        return ApiResponse.success(deviceService.getDevices(principal));
    }

    /**
     * Server-Sent Events endpoint for real-time device presence updates.
     * <p>
     * The client opens a persistent HTTP connection; the server pushes a
     * {@code device-list} event whenever a child device comes online or goes
     * offline.  An initial snapshot is sent immediately on connect so the
     * client does not need a separate REST fetch on startup.
     * </p>
     * <p>
     * The connection times out after {@value SSE_TIMEOUT_MS} ms; the Qt
     * client is expected to reconnect automatically.
     * </p>
     */
    @GetMapping(value = "/events", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    @RequireRole("ADMIN")
    public SseEmitter streamDeviceEvents(
            @RequestAttribute("currentUser") UserPrincipal principal) {

        Long ownerId = Long.valueOf(principal.getId());
        SseEmitter emitter = new SseEmitter(SSE_TIMEOUT_MS);

        // Register before sending the initial snapshot to avoid a race where
        // a heartbeat fires between the snapshot fetch and registration.
        sseNotifier.register(ownerId, emitter);

        // Send current snapshot immediately so the client has up-to-date data
        // right after connecting (no extra REST call needed).
        try {
            List<DeviceDto> snapshot = deviceService.getDevices(principal);
            String json = objectMapper.writeValueAsString(snapshot);
            emitter.send(SseEmitter.event()
                    .name(SseNotifier.EVENT_DEVICE_LIST)
                    .data(json));
        } catch (IOException e) {
            log.warn("[SSE] Failed to send initial snapshot for ownerId={}: {}", ownerId, e.getMessage());
            // Non-fatal: the emitter stays registered; next heartbeat will push an update.
        }

        log.info("[SSE] New connection for ownerId={} connections={}",
                ownerId, sseNotifier.connectionCount(ownerId));
        return emitter;
    }
}
