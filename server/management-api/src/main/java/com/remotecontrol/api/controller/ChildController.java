package com.remotecontrol.api.controller;

import com.remotecontrol.api.dto.ListChillResponse;
import com.remotecontrol.api.dto.RegisterRequest;
import com.remotecontrol.api.dto.RegisterResponse;
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
}
