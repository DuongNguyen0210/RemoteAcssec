package com.remotecontrol.api.service;

import lombok.RequiredArgsConstructor;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.util.Set;
import java.util.concurrent.TimeUnit;

@Service
@RequiredArgsConstructor
public class PresenceService {

    private final StringRedisTemplate redisTemplate;

    public void markDeviceOnline(String adminUsername, String childUsername) {
        String key = "presence:" + adminUsername + ":" + childUsername;
        redisTemplate.opsForValue().set(key, "online", 15, TimeUnit.SECONDS);
    }

    public boolean isDeviceOnline(String adminUsername, String childUsername) {
        String key = "presence:" + adminUsername + ":" + childUsername;
        return Boolean.TRUE.equals(redisTemplate.hasKey(key));
    }

    public Set<String> getOnlineChildrenOfAdmin(String adminUsername) {
        String pattern = "presence:" + adminUsername + ":*";
        return redisTemplate.keys(pattern);
    }
}
