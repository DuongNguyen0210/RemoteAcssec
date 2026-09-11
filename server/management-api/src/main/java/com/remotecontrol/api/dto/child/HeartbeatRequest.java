package com.remotecontrol.api.dto.child;

import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class HeartbeatRequest {
    private String deviceUid;
    private String name;
    private String hostname;
    private String os;
}
