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
        // 1. Tìm hoặc tạo User (Tạm thời tự động tạo để dễ test)
        User owner = userRepository.findByUsername(request.getUsername())
                .orElseGet(() -> {
                    User newUser = User.builder()
                            .username(request.getUsername())
                            .password("dummy_password") // Sau này sẽ có API Register thật
                            .build();
                    return userRepository.save(newUser);
                });

        // 2. Tìm hoặc tạo Device
        Device device = deviceRepository.findByDeviceUid(request.getDeviceUid())
                .orElseGet(() -> Device.builder()
                        .deviceUid(request.getDeviceUid())
                        .owner(owner)
                        .build());

        // 3. Cập nhật thông tin
        device.setName(request.getName());
        device.setIpAddress(request.getIpAddress());
        device.setStatus("ONLINE");
        device.setLastSeen(LocalDateTime.now());
        
        deviceRepository.save(device);

        // 4. Lưu trạng thái vào Redis (Hết hạn sau 30 giây nếu mất mạng)
        String redisKey = "device_status:" + request.getDeviceUid();
        redisTemplate.opsForValue().set(redisKey, "ONLINE", 30, TimeUnit.SECONDS);

        log.info("Đã nhận Heartbeat từ thiết bị: {} (UID: {})", request.getName(), request.getDeviceUid());
    }
}
