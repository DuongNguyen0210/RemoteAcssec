package com.remotecontrol.api.dto.relay;

import jakarta.validation.constraints.*;

public record RelayNodeRegistrationRequest(
        @NotBlank @Pattern(regexp = "[A-Za-z0-9_-]{1,64}") String instanceId,
        @NotBlank @Pattern(regexp = "[A-Za-z0-9-]{1,64}") String bootId,
        @NotBlank @Size(max = 253) String advertisedHost,
        @Min(1) @Max(65535) int advertisedPort,
        @Min(1) @Max(100000) int capacity) {}
