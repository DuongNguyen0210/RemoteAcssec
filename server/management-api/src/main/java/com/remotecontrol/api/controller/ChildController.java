package com.remotecontrol.api.controller;

import com.remotecontrol.api.dto.*;
import com.remotecontrol.api.service.ChildService;
import jakarta.validation.Valid;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;

import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/child")
@RequiredArgsConstructor
public class ChildController {

    private final ChildService childService;

    @PostMapping({"/Register", "/register"})
    public ResponseEntity<ApiResponse<ChildDto>> register(
            @RequestAttribute("currentUser") UserPrincipal currentUser,
            @Valid @RequestBody RegisterRequest registerRequest) {

        ApiResponse<ChildDto> r = childService.register(registerRequest, currentUser);
        if (r.getSuccess())
            return ResponseEntity.ok(r);
        else
            return ResponseEntity.status(HttpStatus.CONFLICT).body(r);
    }

    @DeleteMapping("/{childUsername}")
    public ResponseEntity<ApiResponse<Void>> delete(
            @PathVariable String childUsername,
            @RequestAttribute("currentUser") UserPrincipal currentUser) {
        ApiResponse<Void> res = childService.deleteChild(childUsername, currentUser);
        if (res.getSuccess()) {
            return ResponseEntity.ok(res);
        }
        return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(res);
    }

    @GetMapping({"", "/list"})
    public ResponseEntity<ApiResponse<List<ChildDto>>> getListChildren(@RequestAttribute("currentUser") UserPrincipal currentUser) {
        ApiResponse<List<ChildDto>> response = childService.getListChildren(currentUser);
        if (response.getSuccess())
            return ResponseEntity.ok(response);
        else
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(response);
    }

    @PostMapping("/heartbeat")
    public ResponseEntity<ApiResponse<Void>> heartbeat(
            @RequestAttribute("currentUser") UserPrincipal currentUser,
            @RequestAttribute("currentInfo") InfoPrincipal currentInfo) {
        boolean success = childService.handleHeartbeat(currentUser, currentInfo);
        if (success)
            return ResponseEntity.ok(ApiResponse.success("Heartbeat OK"));
        else
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(ApiResponse.error("Heartbeat failed"));
    }
}
