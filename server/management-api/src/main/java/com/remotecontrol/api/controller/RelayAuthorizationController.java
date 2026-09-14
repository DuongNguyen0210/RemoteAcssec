package com.remotecontrol.api.controller;

import com.remotecontrol.api.annotation.RequireRole;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.service.PresenceService;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;
import java.util.UUID;

/** Relay forwards the connecting client's bearer token for authorization. */
@RestController
@RequestMapping("/api/v1/relay")
@RequiredArgsConstructor
public class RelayAuthorizationController {
    private final ChildRepository children;
    private final PresenceService presence;

    @GetMapping(value = "/agent", produces = "text/plain")
    @RequireRole("CHILD")
    public String agent(@RequestAttribute("currentUser") UserPrincipal principal) {
        if (!children.existsById(Long.valueOf(principal.getId())))
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED);
        return principal.getSessionId();
    }

    @GetMapping(value = "/targets/{sessionId}", produces = "text/plain")
    @RequireRole("ADMIN")
    public String target(@RequestAttribute("currentUser") UserPrincipal principal, @PathVariable UUID sessionId) {
        Long ownerId = Long.valueOf(principal.getId());
        var device = presence.getDevice(ownerId, sessionId.toString());
        if (device == null) throw new ResponseStatusException(HttpStatus.NOT_FOUND);
        var child = children.findById(device.getChildId())
                .orElseThrow(() -> new ResponseStatusException(HttpStatus.NOT_FOUND));
        if (!ownerId.equals(child.getOwner().getId()) ||
                !presence.isSessionActive(child.getId().toString(), sessionId.toString()))
            throw new ResponseStatusException(HttpStatus.NOT_FOUND);
        return sessionId.toString();
    }
}
