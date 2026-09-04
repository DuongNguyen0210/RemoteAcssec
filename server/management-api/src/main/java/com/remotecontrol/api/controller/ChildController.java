package com.remotecontrol.api.controller;

import com.remotecontrol.api.dto.*;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.service.ChildService;
import jakarta.servlet.http.HttpServletRequest;
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
    public ResponseEntity<RegisterResponse> register(
            @RequestAttribute("currentUser") UserPrincipal currentUser,
            @Valid @RequestBody RegisterRequest registerRequest) {

        RegisterResponse r = childService.register(registerRequest, currentUser);
        if(r.getSuccess())
            return ResponseEntity.ok(r);
        else
            return ResponseEntity.status(HttpStatus.CONFLICT).body(r);
    }

    @GetMapping
    public ResponseEntity<ListChillResponse> getListChillResponse(@RequestAttribute("currentUser") UserPrincipal currentUser) {
        ListChillResponse response = childService.getListChillResponse(currentUser);
        if(response.getSuccess())
            return ResponseEntity.ok(response);
        else
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(response);
    }

    @PostMapping("/heartbeat")
    public ResponseEntity<String> heartbeat(
            @RequestAttribute("currentUser") UserPrincipal currentUser,
            @RequestAttribute("currentInfo") InfoPrincipal currentInfo) {
        boolean success = childService.handleHeartbeat(currentUser, currentInfo);
        if (success)
            return ResponseEntity.ok("Heartbeat OK");
        else
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body("Heartbeat failed");
    }
}
