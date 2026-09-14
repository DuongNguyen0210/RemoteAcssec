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
    @jakarta.validation.constraints.Size(max = 255)
    private String deviceUid;
    @jakarta.validation.constraints.Size(max = 255)
    private String name;
    @jakarta.validation.constraints.Size(max = 255)
    private String hostname;
    @jakarta.validation.constraints.NotBlank
    @jakarta.validation.constraints.Size(max = 255)
    private String os;
}
