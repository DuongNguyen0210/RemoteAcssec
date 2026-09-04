package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.InfoPrincipal;
import com.remotecontrol.api.dto.UserPrincipal;
import com.remotecontrol.api.entity.User;
import lombok.RequiredArgsConstructor;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.util.Set;
import java.util.concurrent.TimeUnit;

@Service
@RequiredArgsConstructor
public class PresenceService {

    private final StringRedisTemplate redisTemplate;

    public void markDeviceOnline(User parent, UserPrincipal currentUser, InfoPrincipal currentInfo) {
        String key1 = "presence:" + parent.getUsername() + ":" + currentUser.getUsername();
        redisTemplate.opsForValue().set(key1, "online", 15, TimeUnit.SECONDS);

        String key2 = "Ip:" + currentUser.getUsername() + ":" + currentInfo.getIp();
        redisTemplate.opsForValue().set(key2, "true", 15, TimeUnit.SECONDS);
    }

    public boolean isDeviceOnline(String adminUsername, String childUsername) {
        String key = "presence:" + adminUsername + ":" + childUsername;
        return redisTemplate.hasKey(key);
    }

    public Set<String> getOnlineChildrenOfAdmin(String adminUsername) {
        String pattern = "presence:" + adminUsername + ":*";
        return redisTemplate.keys(pattern);
    }
}
