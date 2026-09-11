package com.remotecontrol.api.controller;

import com.remotecontrol.api.dto.common.ApiResponse;
import com.remotecontrol.api.dto.auth.LoginData;
import com.remotecontrol.api.dto.auth.LoginRequest;
import com.remotecontrol.api.service.AuthService;
import jakarta.validation.Valid;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/auth")
@RequiredArgsConstructor
public class AuthController {

    private final AuthService authService;

    @PostMapping("/login")
    public ResponseEntity<ApiResponse<LoginData>> login(@Valid @RequestBody LoginRequest loginRequest) {
        ApiResponse<LoginData> response = authService.login(loginRequest);
        if (response.getSuccess()) {
            return ResponseEntity.ok(response);
        }
        return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(response);
    }

    @PostMapping("/register")
    public ResponseEntity<ApiResponse<Void>> register(@Valid @RequestBody LoginRequest request) {
        ApiResponse<Void> response = authService.registerAdmin(request);
        if (response.getSuccess()) {
            return ResponseEntity.status(HttpStatus.CREATED).body(response);
        }
        return ResponseEntity.status(HttpStatus.CONFLICT).body(response);
    }
}
