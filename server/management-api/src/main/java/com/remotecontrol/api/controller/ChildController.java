package com.remotecontrol.api.controller;

import com.remotecontrol.api.dto.ListChillResponse;
import com.remotecontrol.api.dto.RegisterRequest;
import com.remotecontrol.api.dto.RegisterResponse;
import com.remotecontrol.api.dto.HeartbeatRequest;
import com.remotecontrol.api.dto.ChildSummaryResponse;
import com.remotecontrol.api.service.ChildService;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.bind.annotation.RequestHeader;

import java.util.List;
import java.util.Optional;

@RestController
@RequestMapping("/api/v1/child")
@RequiredArgsConstructor
public class ChildController {

    private final ChildService childService;

    @GetMapping
    public ResponseEntity<List<ChildSummaryResponse>> listOwnedChildren(
            @RequestHeader(value = "Authorization", required = false) String authHeader) {

        String token = null;
        if (authHeader != null && authHeader.startsWith("Bearer ")) {
            token = authHeader.substring(7);
        }

        Optional<List<ChildSummaryResponse>> children = childService.findOwnedChildren(token);
        if (!children.isPresent()) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).build();
        }
        return ResponseEntity.ok(children.get());
    }

    @PostMapping("/Register")
    public ResponseEntity<RegisterResponse> register(
            @RequestHeader(value = "Authorization") String authHeader,
            @Valid @RequestBody RegisterRequest registerRequest) {

        String token = null;
        if (authHeader != null && authHeader.startsWith("Bearer ")) {
            token = authHeader.substring(7);
        }

        RegisterResponse r = childService.register(registerRequest, token);
        if(r.getSuccess())
            return ResponseEntity.ok(r);
        else
            return ResponseEntity.status(HttpStatus.CONFLICT).body(r);
    }

    @GetMapping("/list")
    public ResponseEntity<ListChillResponse> getListChillResponse(@RequestHeader(value = "Authorization") String authHeader) {
        String token = null;
        if (authHeader != null && authHeader.startsWith("Bearer ")) {
            token = authHeader.substring(7);
        }
        ListChillResponse response = childService.getListChillResponse(token);
        if(response.getSuccess())
            return ResponseEntity.ok(response);
        else
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(response);
    }

    @PostMapping("/heartbeat")
    public ResponseEntity<String> heartbeat(
            @RequestHeader(value = "Authorization", required = false) String authHeader,
            @RequestBody HeartbeatRequest heartbeatRequest,
            HttpServletRequest request) {

        String token = null;
        if (authHeader != null && authHeader.startsWith("Bearer ")) {
            token = authHeader.substring(7);
        }

        String remoteIp = request.getRemoteAddr();

        boolean success = childService.handleHeartbeat(token, heartbeatRequest, remoteIp);
        if (success) {
            return ResponseEntity.ok("Heartbeat OK");
        } else {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body("Heartbeat failed");
        }
    }
}
