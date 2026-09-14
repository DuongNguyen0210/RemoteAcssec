package com.remotecontrol.api.service;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.child.DeviceDto;
import com.remotecontrol.api.dto.child.HeartbeatRequest;
import com.remotecontrol.api.dto.common.InfoPrincipal;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.User;
import lombok.RequiredArgsConstructor;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.data.redis.core.ScanOptions;
import org.springframework.data.redis.core.script.DefaultRedisScript;
import org.springframework.stereotype.Service;
import java.time.Duration;
import java.util.*;

@Service
@RequiredArgsConstructor
public class PresenceService {
    public static final int TTL_SECONDS = 20;
    private final StringRedisTemplate redisTemplate;
    private final ObjectMapper objectMapper;

    // Atomic check/write: logout cannot be undone by an in-flight heartbeat.
    private static final DefaultRedisScript<Long> HEARTBEAT = new DefaultRedisScript<>(
            "if redis.call('GET',KEYS[1]) ~= ARGV[1] then return 0 end " +
            "redis.call('SET',KEYS[2],ARGV[2],'EX',ARGV[3]); return 1", Long.class);

    public void openSession(Long childId, String sessionId) {
        redisTemplate.opsForValue().set(sessionKey(sessionId), childId.toString(), Duration.ofHours(10));
    }

    public boolean isSessionActive(String childId, String sessionId) {
        return sessionId != null && childId != null
                && childId.equals(redisTemplate.opsForValue().get(sessionKey(sessionId)));
    }

    public boolean markDeviceOnline(User owner, UserPrincipal principal, InfoPrincipal info,
                                    HeartbeatRequest request) {
        if (request == null || principal.getSessionId() == null) return false;
        String name = request.getHostname();
        if (name == null || name.isBlank()) name = request.getName();
        if (name == null || name.isBlank()) name = "Unknown device";
        DeviceDto snapshot = DeviceDto.builder()
                .sessionId(principal.getSessionId()).childId(Long.valueOf(principal.getId()))
                .username(principal.getUsername()).deviceName(name).os(request.getOs())
                .ipAddress(info == null ? null : info.getIp())
                .lastHeartbeatAt(System.currentTimeMillis()).build();
        try {
            return Long.valueOf(1).equals(redisTemplate.execute(HEARTBEAT,
                    List.of(sessionKey(principal.getSessionId()), deviceKey(owner.getId(), principal.getSessionId())),
                    principal.getId(), objectMapper.writeValueAsString(snapshot), String.valueOf(TTL_SECONDS)));
        } catch (JsonProcessingException e) {
            throw new IllegalStateException("Cannot serialize device presence", e);
        }
    }

    public List<DeviceDto> getDevices(Long ownerId) {
        Set<String> keys = new HashSet<>();
        try (var cursor = redisTemplate.scan(ScanOptions.scanOptions()
                .match("presence:v2:" + ownerId + ":*").count(100).build())) {
            cursor.forEachRemaining(keys::add);
        }
        if (keys.isEmpty()) return List.of();
        List<String> snapshots = redisTemplate.opsForValue().multiGet(keys);
        List<DeviceDto> devices = new ArrayList<>();
        if (snapshots != null) {
            for (String snapshot : snapshots) {
                if (snapshot != null) devices.add(decode(snapshot)); // May expire between SCAN and MGET.
            }
        }
        devices.sort(Comparator.comparing(DeviceDto::getSessionId));
        return devices;
    }

    public DeviceDto getDevice(Long ownerId, String sessionId) {
        String snapshot = redisTemplate.opsForValue().get(deviceKey(ownerId, sessionId));
        return snapshot == null ? null : decode(snapshot);
    }

    public void closeSession(Long ownerId, String sessionId) {
        redisTemplate.delete(List.of(sessionKey(sessionId), deviceKey(ownerId, sessionId)));
    }

    private DeviceDto decode(String json) {
        try { return objectMapper.readValue(json, DeviceDto.class); }
        catch (JsonProcessingException e) { throw new IllegalStateException("Invalid device presence", e); }
    }

    private String sessionKey(String sessionId) { return "auth:child:" + sessionId; }
    private String deviceKey(Long ownerId, String sessionId) { return "presence:v2:" + ownerId + ":" + sessionId; }
}
