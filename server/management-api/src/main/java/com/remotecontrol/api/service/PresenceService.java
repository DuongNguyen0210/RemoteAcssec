package com.remotecontrol.api.service;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.child.DeviceDto;
import com.remotecontrol.api.dto.child.HeartbeatRequest;
import com.remotecontrol.api.dto.child.TelemetrySnapshot;
import com.remotecontrol.api.dto.common.InfoPrincipal;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.util.ChildPasswords;
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
    private final ChildRepository childRepository;
    private final LoginLeaseService loginLeases;

    private static final DefaultRedisScript<Long> HEARTBEAT = new DefaultRedisScript<>(
            "if redis.call('GET',KEYS[1]) ~= ARGV[1] then return 0 end " +
            "local login=redis.call('GET',KEYS[3]); if not login or string.sub(login,1,string.len(ARGV[4])) ~= ARGV[4] then return 0 end " +
            "if redis.call('PTTL',KEYS[3]) <= tonumber(ARGV[5]) then return 0 end " +
            "local incoming=cjson.decode(ARGV[2]); local old=redis.call('GET',KEYS[2]); " +
            "if old then local previous=cjson.decode(old); " +
            "if incoming.telemetry and incoming.telemetry ~= cjson.null and previous.telemetry and previous.telemetry ~= cjson.null " +
            "and incoming.telemetry.sequence <= previous.telemetry.sequence then return 1 end end " +
            "redis.call('SET',KEYS[2],ARGV[2],'EX',ARGV[3]); return 1", Long.class);

    private String binding(String childId, String credential) {
        if (childId == null || credential == null || credential.isBlank()) return null;
        String fp = ChildPasswords.fingerprint(credential);
        if (fp.isEmpty()) return null;
        return childId + ":" + fp;
    }

    public void openSession(Long childId, String sessionId, String credential) {
        if (childId == null || sessionId == null) return;
        String value = binding(childId.toString(), credential);
        if (value == null) return;
        redisTemplate.opsForValue().set(sessionKey(sessionId), value, Duration.ofHours(10));
    }

    public boolean isSessionActive(String childId, String sessionId) {
        if (sessionId == null || childId == null) return false;
        String stored = redisTemplate.opsForValue().get(sessionKey(sessionId));
        if (stored == null) return false;
        String currentCredential = childRepository.findById(Long.valueOf(childId))
                .map(Child::getPassword).orElse(null);
        String expected = binding(childId, currentCredential);
        if (expected == null || !expected.equals(stored)) {
            redisTemplate.delete(sessionKey(sessionId));
            return false;
        }
        return loginLeases.active("CHILD", childId, sessionId, currentCredential);
    }

    public boolean markDeviceOnline(User owner, UserPrincipal principal, InfoPrincipal info,
                                    HeartbeatRequest request, String currentCredential) {
        if (request == null || principal.getSessionId() == null) return false;
        String credential = currentCredential != null ? currentCredential :
                childRepository.findById(Long.valueOf(principal.getId()))
                        .map(Child::getPassword).orElse(null);
        String expectedBinding = binding(principal.getId(), credential);
        if (expectedBinding == null) return false;
        String name = request.getHostname();
        if (name == null || name.isBlank()) name = request.getName();
        if (name == null || name.isBlank()) name = "Unknown device";
        long receivedAt = System.currentTimeMillis();
        DeviceDto snapshot = DeviceDto.builder()
                .sessionId(principal.getSessionId())
                .childId(Long.valueOf(principal.getId()))
                .username(principal.getUsername())
                .deviceName(name).os(request.getOs())
                .ipAddress(info == null ? null : info.getIp())
                .lastHeartbeatAt(receivedAt)
                .telemetry(TelemetrySnapshot.from(request.getTelemetry(), receivedAt))
                .telemetryAgeMs(request.getTelemetry() == null ? null : 0L).build();
        try {
            boolean updated = Long.valueOf(1).equals(redisTemplate.execute(
                    HEARTBEAT,
                    List.of(
                            sessionKey(principal.getSessionId()),
                            deviceKey(owner.getId(), principal.getSessionId()),
                            "auth:login:CHILD:" + principal.getId()
                    ),
                    expectedBinding,
                    objectMapper.writeValueAsString(snapshot),
                    String.valueOf(TTL_SECONDS), principal.getSessionId() + ":",
                    Integer.toString(LoginLeaseService.DRAIN_SECONDS * 1000)
            ));
            return updated;
        } catch (JsonProcessingException e) {
            throw new IllegalStateException("Cannot serialize device presence", e);
        }
    }

    public boolean markDeviceOnline(User owner, UserPrincipal principal, InfoPrincipal info,
                                    HeartbeatRequest request) {
        return markDeviceOnline(owner, principal, info, request, null);
    }

    public List<DeviceDto> getDevices(Long ownerId) {
        Set<String> keys = new HashSet<>();
        try (var cursor = redisTemplate.scan(ScanOptions.scanOptions()
                .match("presence:" + ownerId + ":*").count(100).build())) {
            cursor.forEachRemaining(keys::add);
        }
        if (keys.isEmpty()) return List.of();
        List<String> snapshots = redisTemplate.opsForValue().multiGet(keys);
        List<DeviceDto> devices = new ArrayList<>();
        if (snapshots != null) {
            for (String snapshot : snapshots) {
                if (snapshot != null) {
                    var device = decode(snapshot);
                    if (loginLeases.owns("CHILD", device.getChildId().toString(), device.getSessionId())) devices.add(device);
                }
            }
        }
        devices.sort(Comparator.comparing(DeviceDto::getSessionId));
        return devices;
    }

    public DeviceDto getDevice(Long ownerId, String sessionId) {
        String snapshot = redisTemplate.opsForValue().get(deviceKey(ownerId, sessionId));
        if (snapshot == null) return null;
        var device = decode(snapshot);
        return loginLeases.owns("CHILD", device.getChildId().toString(), sessionId) ? device : null;
    }

    public void closeSession(Long ownerId, String sessionId) {
        String stored = redisTemplate.opsForValue().get(sessionKey(sessionId));
        if (stored != null && stored.contains(":"))
            loginLeases.releaseSession("CHILD", stored.substring(0, stored.indexOf(':')), sessionId);
        redisTemplate.delete(List.of(sessionKey(sessionId), deviceKey(ownerId, sessionId)));
    }

    public void hideDevice(Long ownerId, String sessionId) {
        if (ownerId != null && sessionId != null) {
            redisTemplate.delete(deviceKey(ownerId, sessionId));
        }
    }

    private DeviceDto decode(String json) {
        try {
            var device = objectMapper.readValue(json, DeviceDto.class);
            device.setTelemetryAgeMs(device.getTelemetry() == null ? null
                    : Math.max(0, System.currentTimeMillis() - device.getTelemetry().receivedAt()));
            return device;
        }
        catch (JsonProcessingException e) { throw new IllegalStateException("Invalid device presence", e); }
    }

    private String sessionKey(String sessionId) { return "auth:child:" + sessionId; }
    private String deviceKey(Long ownerId, String sessionId) { return "presence:" + ownerId + ":" + sessionId; }
}
