package com.remotecontrol.api.controller;

import com.remotecontrol.api.dto.common.ApiResponse;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.dto.user.ChangePasswordRequest;
import com.remotecontrol.api.dto.user.UserDto;
import com.remotecontrol.api.service.UserService;
import jakarta.validation.Valid;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/user")
@RequiredArgsConstructor
public class UserController {

    private final UserService userService;

    @GetMapping("/profile")
    public ResponseEntity<ApiResponse<UserDto>> getProfile(
            @RequestAttribute("currentUser") UserPrincipal currentUser) {
        ApiResponse<UserDto> response = userService.getProfile(currentUser);
        if (response.getSuccess()) {
            return ResponseEntity.ok(response);
        }
        return ResponseEntity.status(HttpStatus.NOT_FOUND).body(response);
    }

    @PutMapping("/password")
    public ResponseEntity<ApiResponse<Void>> changePassword(
            @RequestAttribute("currentUser") UserPrincipal currentUser,
            @Valid @RequestBody ChangePasswordRequest request) {
        ApiResponse<Void> response = userService.changePassword(request, currentUser);
        if (response.getSuccess()) {
            return ResponseEntity.ok(response);
        }
        return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(response);
    }
}
