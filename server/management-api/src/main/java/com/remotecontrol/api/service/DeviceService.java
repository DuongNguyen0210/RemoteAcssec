package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.HeartbeatRequest;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.Optional;

@Service
@RequiredArgsConstructor
@Slf4j
public class DeviceService {

    private static final String STATUS_ONLINE = "ONLINE";

    private final ChildRepository deviceRepository;
    private final UserRepository userRepository;
    private final StringRedisTemplate redisTemplate;

    /**
     * Cập nhật trạng thái online của thiết bị (child) khi nhận được heartbeat.
     * Thiết bị được nhận diện bằng child_username (đã unique trong DB).
     *
     * @return true nếu tìm thấy thiết bị và đã cập nhật, false nếu thiết bị chưa tồn tại.
     */
    @Transactional
    public boolean processHeartbeat(HeartbeatRequest request) {
        Optional<Child> found = deviceRepository.findByChildUsername(request.getUsername());

        if (found.isEmpty()) {
            // Không tự tạo tài khoản mới từ heartbeat: Child.owner là NOT NULL và
            // request không mang thông tin owner nào có thể xác định an toàn.
            log.warn("Heartbeat từ thiết bị chưa đăng ký: username={}, deviceUid={}",
                    request.getUsername(), request.getDeviceUid());
            return false;
        }

        Child device = found.get();
        device.setName(request.getName());
        if (request.getIpAddress() != null && !request.getIpAddress().isBlank()) {
            device.setIpAddress(request.getIpAddress());
        }
        device.setStatus(STATUS_ONLINE);
        device.setLastSeen(LocalDateTime.now());
        deviceRepository.save(device);

        // deviceUid chưa có cột tương ứng trong entity Child nên chỉ log, không lưu.
        log.info("Heartbeat OK: username={}, name={}, ip={}, deviceUid={} (chưa được lưu)",
                device.getChildUsername(), device.getName(), device.getIpAddress(), request.getDeviceUid());
        return true;
    }
}
