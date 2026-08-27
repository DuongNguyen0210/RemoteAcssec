package com.remotecontrol.api.controller;

import com.remotecontrol.api.dto.RegisterRequest;
import com.remotecontrol.api.dto.RegisterResponse;
import com.remotecontrol.api.service.ChildService;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/v1/child")
@RequiredArgsConstructor
public class ChildController {

    private final ChildService childService;

    @PostMapping("/Register")
    public ResponseEntity<RegisterResponse> register(@RequestBody RegisterRequest registerRequest) {
        RegisterResponse r = childService.register(registerRequest);
        if(r.getSuccess())
            return ResponseEntity.ok(r);
        else
            return ResponseEntity.status(HttpStatus.CONFLICT).body(r);
    }
}
