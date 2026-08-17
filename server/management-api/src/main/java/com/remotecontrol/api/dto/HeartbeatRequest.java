package com.remotecontrol.api.dto;

import jakarta.validation.constraints.NotBlank;
import lombok.Data;

@Data
public class HeartbeatRequest {
    
    @NotBlank(message = "deviceUid can not be empty")
    private String deviceUid;
    
    @NotBlank(message = "Name can not be empty")
    private String name;
    
    @NotBlank(message = "username can not be empty")
    private String username;
    
    private String ipAddress;
}
