package com.remotecontrol.api.controller;

import com.remotecontrol.api.dto.ApiResponse;
import com.remotecontrol.api.dto.LoginData;
import com.remotecontrol.api.dto.LoginRequest;
import com.remotecontrol.api.service.AuthService;
import jakarta.validation.Valid;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("api/v1/auth")
@RequiredArgsConstructor
public class AuthController {

    private final AuthService authService;

    @PostMapping("/login")
    public ResponseEntity<ApiResponse<LoginData>> login(@Valid @RequestBody LoginRequest loginRequest) {

        ApiResponse<LoginData> l = authService.login(loginRequest);
        if (l.getSuccess()) {
            return ResponseEntity.ok(l);
        }
        return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(l);
    }
}

