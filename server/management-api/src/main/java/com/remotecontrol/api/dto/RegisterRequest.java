package com.remotecontrol.api.dto;

import com.remotecontrol.api.entity.User;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import lombok.Data;

@Data
public class RegisterRequest {
    @NotBlank
    private String childUsername;
    @NotBlank
    private String password;
}
