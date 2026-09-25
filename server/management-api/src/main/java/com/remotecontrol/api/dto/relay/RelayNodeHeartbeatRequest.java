package com.remotecontrol.api.dto.relay;

import jakarta.validation.constraints.*;

public record RelayNodeHeartbeatRequest(
        @NotBlank @Pattern(regexp = "[A-Za-z0-9-]{1,64}") String bootId,
        @Min(0) int registeredChildren,
        @Min(0) int activeSessions) {}
