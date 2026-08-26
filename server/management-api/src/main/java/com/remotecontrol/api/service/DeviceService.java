package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.HeartbeatRequest;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
@RequiredArgsConstructor
@Slf4j
public class DeviceService {

    private final ChildRepository deviceRepository;
    private final UserRepository userRepository;
    private final StringRedisTemplate redisTemplate;

    @Transactional
    public void processHeartbeat(HeartbeatRequest request) {
        User owner = userRepository.findByUsername(request.getUsername())
                .orElseGet(() -> {
                    User newUser = User.builder()
                            .username(request.getUsername())
                            .password("dummy_password")
                            .build();
                    return userRepository.save(newUser);
                });
    }
}
