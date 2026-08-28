package com.remotecontrol.api.controller;

import com.remotecontrol.api.dto.HeartbeatRequest;
import com.remotecontrol.api.service.DeviceService;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/devices")
@RequiredArgsConstructor
public class DeviceController {

    private final DeviceService deviceService;

    @PostMapping("/heartbeat")
    public ResponseEntity<String> heartbeat(@Valid @RequestBody HeartbeatRequest request, HttpServletRequest httpRequest) {

        if (request.getIpAddress() == null || request.getIpAddress().isEmpty()) {
            request.setIpAddress(httpRequest.getRemoteAddr());
        }

        boolean updated = deviceService.processHeartbeat(request);

        if (!updated) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body("DEVICE_NOT_FOUND");
        }

        return ResponseEntity.ok("OK");
    }
}
