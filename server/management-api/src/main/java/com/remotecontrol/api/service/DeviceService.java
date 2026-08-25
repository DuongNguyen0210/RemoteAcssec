package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.HeartbeatRequest;
import com.remotecontrol.api.entity.Device;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.DeviceRepository;
import com.remotecontrol.api.repository.UserRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.concurrent.TimeUnit;

@Service
@RequiredArgsConstructor
@Slf4j
public class DeviceService {

    private final DeviceRepository deviceRepository;
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
        Device device = deviceRepository.findByDeviceUid(request.getDeviceUid())
                .orElseGet(() -> Device.builder()
                        .deviceUid(request.getDeviceUid())
                        .owner(owner)
                        .build());

        device.setName(request.getName());
        device.setIpAddress(request.getIpAddress());
        device.setStatus("ONLINE");
        device.setLastSeen(LocalDateTime.now());
        
        deviceRepository.save(device);

        String redisKey = "device_status:" + request.getDeviceUid();
        redisTemplate.opsForValue().set(redisKey, "ONLINE", 30, TimeUnit.SECONDS);

        log.info("Đã nhận Heartbeat từ thiết bị: {} (UID: {})", request.getName(), request.getDeviceUid());
    }
}
