package com.remotecontrol.api.dto.child;

import lombok.*;

@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class DeviceDto {
    private String sessionId;
    private Long childId;
    private String username;
    private String deviceName;
    private String os;
    private String ipAddress;
    private long lastHeartbeatAt;
}
