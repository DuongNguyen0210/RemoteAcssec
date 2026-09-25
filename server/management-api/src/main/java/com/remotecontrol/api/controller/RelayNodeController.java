package com.remotecontrol.api.controller;

import com.remotecontrol.api.dto.relay.*;
import com.remotecontrol.api.service.RelayDispatchService;
import jakarta.validation.Valid;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;

/** Service-to-service endpoints, blocked by the public Nginx listener. */
@RestController
@RequestMapping("/internal/relay/nodes")
public class RelayNodeController {
    private final RelayDispatchService dispatch;
    private final byte[] secret;

    public RelayNodeController(RelayDispatchService dispatch,
            @Value("${relay.node-token:}") String token) {
        this.dispatch = dispatch;
        this.secret = token.getBytes(StandardCharsets.UTF_8);
    }

    @PostMapping
    @ResponseStatus(HttpStatus.NO_CONTENT)
    public void register(@RequestHeader(value = "X-Relay-Token", defaultValue = "") String token,
                         @Valid @RequestBody RelayNodeRegistrationRequest request) {
        authenticate(token);
        dispatch.register(request);
    }

    @PostMapping("/{instanceId}/heartbeat")
    @ResponseStatus(HttpStatus.NO_CONTENT)
    public void heartbeat(@RequestHeader(value = "X-Relay-Token", defaultValue = "") String token,
                          @PathVariable String instanceId,
                          @Valid @RequestBody RelayNodeHeartbeatRequest request) {
        authenticate(token);
        if (!instanceId.matches("[A-Za-z0-9_-]{1,64}"))
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST);
        dispatch.heartbeat(instanceId, request);
    }

    private void authenticate(String token) {
        if (secret.length == 0 || !MessageDigest.isEqual(secret, token.getBytes(StandardCharsets.UTF_8)))
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED);
    }
}
