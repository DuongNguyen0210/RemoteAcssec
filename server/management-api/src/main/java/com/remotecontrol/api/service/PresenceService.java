package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.child.HeartbeatRequest;
import com.remotecontrol.api.dto.common.InfoPrincipal;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.User;
import lombok.RequiredArgsConstructor;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

@Service
@RequiredArgsConstructor
public class PresenceService {

    private final StringRedisTemplate redisTemplate;

    public void markDeviceOnline(User parent, UserPrincipal currentUser, InfoPrincipal currentInfo, HeartbeatRequest request) {
        String key1 = "presence:" + parent.getUsername() + ":" + currentUser.getUsername();
        redisTemplate.opsForValue().set(key1, "online", 15, TimeUnit.SECONDS);

        String ip = (currentInfo != null && currentInfo.getIp() != null) ? currentInfo.getIp() : "Unknown";
        String key2 = "Ip:" + currentUser.getUsername() + ":" + ip;
        redisTemplate.opsForValue().set(key2, "true", 15, TimeUnit.SECONDS);

        String deviceKey = "device:info:" + currentUser.getUsername();
        if (ip != null) {
            redisTemplate.opsForHash().put(deviceKey, "ip", ip);
        }
        if (request != null) {
            if (request.getDeviceUid() != null) {
                redisTemplate.opsForHash().put(deviceKey, "deviceUid", request.getDeviceUid());
            }
            String devName = request.getName() != null ? request.getName() : request.getHostname();
            if (devName != null) {
                redisTemplate.opsForHash().put(deviceKey, "deviceName", devName);
            }
            if (request.getOs() != null) {
                redisTemplate.opsForHash().put(deviceKey, "os", request.getOs());
            }
        }
        redisTemplate.expire(deviceKey, 15, TimeUnit.SECONDS);
    }

    public void markDeviceOnline(User parent, UserPrincipal currentUser, InfoPrincipal currentInfo) {
        markDeviceOnline(parent, currentUser, currentInfo, null);
    }

    public boolean isDeviceOnline(String adminUsername, String childUsername) {
        String key = "presence:" + adminUsername + ":" + childUsername;
        return Boolean.TRUE.equals(redisTemplate.hasKey(key));
    }

    public Map<Object, Object> getDeviceInfo(String childUsername) {
        String deviceKey = "device:info:" + childUsername;
        return redisTemplate.opsForHash().entries(deviceKey);
    }

    public Set<String> getOnlineChildrenOfAdmin(String adminUsername) {
        String pattern = "presence:" + adminUsername + ":*";
        return redisTemplate.keys(pattern);
    }
}
