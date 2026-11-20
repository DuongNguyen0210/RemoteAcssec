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
    private final com.remotecontrol.api.service.SseNotifier sseNotifier;

    @PostMapping("/login")
    public ResponseEntity<ApiResponse<LoginData>> login(@Valid @RequestBody LoginRequest loginRequest) {
        ApiResponse<LoginData> response = authService.login(loginRequest);
        if (response.getSuccess()) {
            return ResponseEntity.ok(response);
        }
        return ResponseEntity.status("ACCOUNT_IN_USE".equals(response.getErrorCode())
                ? HttpStatus.CONFLICT : HttpStatus.UNAUTHORIZED).body(response);
    }

    @PostMapping("/heartbeat")
    public ResponseEntity<ApiResponse<Void>> heartbeat(
            @RequestAttribute("currentUser") com.remotecontrol.api.dto.common.UserPrincipal user) {
        return authService.renew(user) ? ResponseEntity.ok(ApiResponse.success("Session renewed"))
                : ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(ApiResponse.error("SESSION_EXPIRED", "Please sign in again"));
    }

    @PostMapping("/logout")
    public ApiResponse<Void> logout(@RequestAttribute("currentUser") com.remotecontrol.api.dto.common.UserPrincipal user) {
        authService.logout(user);
        if ("ADMIN".equals(user.getRole())) sseNotifier.closeOwner(Long.valueOf(user.getId()));
        return ApiResponse.success("Logged out");
    }

    @PostMapping("/register")
    public ResponseEntity<ApiResponse<Void>> register(@Valid @RequestBody LoginRequest request,
            jakarta.servlet.http.HttpServletRequest httpRequest) {
        // The public proxy rate-limits this canonical path. Reject MVC path variants.
        if (!"/api/v1/auth/register".equals(httpRequest.getRequestURI())) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("NOT_FOUND", "Route not found"));
        }
        ApiResponse<Void> response = authService.registerAdmin(request);
        if (response.getSuccess()) {
            return ResponseEntity.status(HttpStatus.CREATED).body(response);
        }
        HttpStatus status = "USER_ALREADY_EXISTS".equals(response.getErrorCode())
                ? HttpStatus.CONFLICT : HttpStatus.BAD_REQUEST;
        return ResponseEntity.status(status).body(response);
    }
}
