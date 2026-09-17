package com.remotecontrol.api.controller;

import com.remotecontrol.api.annotation.RequireRole;
import com.remotecontrol.api.dto.common.ApiResponse;
import com.remotecontrol.api.dto.common.InfoPrincipal;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.dto.child.ChildDto;
import com.remotecontrol.api.dto.child.HeartbeatRequest;
import com.remotecontrol.api.dto.child.RegisterRequest;
import com.remotecontrol.api.service.ChildService;
import jakarta.validation.Valid;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/v1/child")
@RequiredArgsConstructor
public class ChildController {

    private final ChildService childService;

    @RequireRole("ADMIN")
    @PostMapping({"/Register", "/register"})
    public ResponseEntity<ApiResponse<ChildDto>> register(
            @RequestAttribute("currentUser") UserPrincipal currentUser,
            @Valid @RequestBody RegisterRequest registerRequest) {
        ApiResponse<ChildDto> r = childService.register(registerRequest, currentUser);
        if (r.getSuccess()) {
            return ResponseEntity.ok(r);
        }
        return ResponseEntity.status(HttpStatus.CONFLICT).body(r);
    }

    @RequireRole("ADMIN")
    @PutMapping("/{id}")
    public ResponseEntity<ApiResponse<ChildDto>> update(
            @PathVariable Long id,
            @RequestAttribute("currentUser") UserPrincipal currentUser,
            @Valid @RequestBody com.remotecontrol.api.dto.child.UpdateChildRequest request) {
        ApiResponse<ChildDto> result;
        try {
            result = childService.updateChild(id, request, currentUser);
        } catch (org.springframework.dao.DataIntegrityViolationException conflict) {
            return ResponseEntity.status(HttpStatus.CONFLICT)
                    .body(ApiResponse.error("CHILD_ALREADY_EXISTS", "Account name already exists"));
        }
        if (result.getSuccess()) return ResponseEntity.ok(result);
        return ResponseEntity.status(switch (result.getErrorCode()) {
            case "CHILD_NOT_FOUND" -> HttpStatus.NOT_FOUND;
            case "FORBIDDEN" -> HttpStatus.FORBIDDEN;
            case "CHILD_ALREADY_EXISTS" -> HttpStatus.CONFLICT;
            default -> HttpStatus.BAD_REQUEST;
        }).body(result);
    }

    @RequireRole("ADMIN")
    @DeleteMapping("/{childUsername}")
    public ResponseEntity<ApiResponse<Void>> delete(
            @PathVariable String childUsername,
            @RequestAttribute("currentUser") UserPrincipal currentUser) {
        ApiResponse<Void> res = childService.deleteChild(childUsername, currentUser);
        if (res.getSuccess())
            return ResponseEntity.ok(res);
        return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(res);
    }

    @RequireRole("ADMIN")
    @GetMapping({"", "/list"})
    public ResponseEntity<ApiResponse<List<ChildDto>>> getListChildren(
            @RequestAttribute("currentUser") UserPrincipal currentUser) {
        ApiResponse<List<ChildDto>> response = childService.getListChildren(currentUser);
        if (response.getSuccess()) {
            return ResponseEntity.ok(response);
        }
        return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(response);
    }

    @RequireRole("CHILD")
    @PostMapping("/logout")
    public ApiResponse<Void> logout(@RequestAttribute("currentUser") UserPrincipal principal) {
        childService.logout(principal);
        return ApiResponse.success("Logged out");
    }

    @RequireRole("CHILD")
    @PostMapping("/heartbeat")
    public ResponseEntity<ApiResponse<Void>> heartbeat(
            @RequestAttribute("currentUser") UserPrincipal currentUser,
            @RequestAttribute("currentInfo") InfoPrincipal currentInfo,
            @Valid @RequestBody HeartbeatRequest heartbeatRequest) {
        boolean success = childService.handleHeartbeat(currentUser, currentInfo, heartbeatRequest);
        if (success) {
            return ResponseEntity.ok(ApiResponse.success("Heartbeat OK"));
        }
        return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(ApiResponse.error("UNAUTHORIZED", "Heartbeat failed"));
    }
}
