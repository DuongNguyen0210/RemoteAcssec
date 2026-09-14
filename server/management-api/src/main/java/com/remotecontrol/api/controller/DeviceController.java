package com.remotecontrol.api.controller;

import com.remotecontrol.api.annotation.RequireRole;
import com.remotecontrol.api.dto.child.DeviceDto;
import com.remotecontrol.api.dto.common.ApiResponse;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.service.DeviceService;
import lombok.RequiredArgsConstructor;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestAttribute;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;

@RestController
@RequestMapping("/api/v1/devices")
@RequiredArgsConstructor
public class DeviceController {

    private final DeviceService deviceService;

    @GetMapping
    @RequireRole("ADMIN")
    public ApiResponse<List<DeviceDto>> list(@RequestAttribute("currentUser") UserPrincipal principal) {
        return ApiResponse.success(deviceService.getDevices(principal));
    }
}
