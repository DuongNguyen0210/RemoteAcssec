package com.remotecontrol.api.dto;

import lombok.Data;

@Data
public class LoginResponse {
    private Boolean success;
    private String role;
    private String message;
    private String token;
}
