package com.remotecontrol.api.dto;

import jakarta.validation.constraints.NotBlank;
import lombok.Data;

@Data
public class LoginResponse {
    private Boolean success;
    private String role;
    private String message;
}
