package com.remotecontrol.api.controller;

import com.remotecontrol.api.annotation.RequireRole;
import com.remotecontrol.api.dto.common.*;
import com.remotecontrol.api.dto.relay.RelayEndpointDto;
import com.remotecontrol.api.service.RelayDispatchService;
import com.remotecontrol.api.repository.ChildRepository;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;

@RestController
@RequestMapping("/api/v1/relay/allocations")
@RequiredArgsConstructor
public class RelayAllocationController {
    private final RelayDispatchService dispatch;
    private final ChildRepository children;

    @PostMapping
    @RequireRole("CHILD")
    public ApiResponse<RelayEndpointDto> allocate(@RequestAttribute("currentUser") UserPrincipal principal) {
        if (!children.existsById(Long.valueOf(principal.getId())))
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED);
        return ApiResponse.success(dispatch.allocate(principal.getSessionId()));
    }
}
