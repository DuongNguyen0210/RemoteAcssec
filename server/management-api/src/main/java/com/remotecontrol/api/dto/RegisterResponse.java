package com.remotecontrol.api.dto;

import lombok.Data;
import lombok.RequiredArgsConstructor;

@Data
@RequiredArgsConstructor
public class RegisterResponse {
    private final Boolean success; // true: success, false: child already exists
    private final String message;
    private final String childUsername;
    private final String childPassword;
}
