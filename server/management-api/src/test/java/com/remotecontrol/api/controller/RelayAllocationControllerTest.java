package com.remotecontrol.api.controller;

import com.remotecontrol.api.config.*;
import com.remotecontrol.api.dto.child.DeviceDto;
import com.remotecontrol.api.dto.relay.RelayEndpointDto;
import com.remotecontrol.api.entity.*;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.service.*;
import com.remotecontrol.api.util.JwtUtil;
import org.junit.jupiter.api.Test;
import org.springframework.test.web.servlet.setup.MockMvcBuilders;
import java.util.*;
import static org.mockito.Mockito.*;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.*;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

class RelayAllocationControllerTest {
    @Test void rolesOwnershipAndLivePresenceAreRequired() throws Exception {
        var jwt = mock(JwtUtil.class);
        var presence = mock(PresenceService.class);
        var children = mock(ChildRepository.class);
        var dispatch = mock(RelayDispatchService.class);
        var auth = new RelayAuthorizationController(children, presence, dispatch);
        var mvc = MockMvcBuilders.standaloneSetup(new RelayAllocationController(dispatch, children),
                new DeviceController(mock(DeviceService.class), auth, dispatch))
                .addInterceptors(new JwtInterceptor(jwt, presence))
                .setControllerAdvice(new GlobalExceptionHandler()).build();
        String id = UUID.randomUUID().toString();
        when(jwt.extractId("child")).thenReturn("2");
        when(jwt.extractRole("child")).thenReturn("CHILD");
        when(jwt.extractSessionId("child")).thenReturn(id);
        when(presence.isSessionActive("2", id)).thenReturn(true);
        when(children.existsById(2L)).thenReturn(true);
        when(jwt.extractId("admin")).thenReturn("1");
        when(jwt.extractRole("admin")).thenReturn("ADMIN");
        var endpoint = new RelayEndpointDto("node", "localhost", 9091, System.currentTimeMillis()+30000);
        when(dispatch.allocate(id)).thenReturn(endpoint);
        when(dispatch.lookup(id)).thenReturn(endpoint);
        mvc.perform(post("/api/v1/relay/allocations")).andExpect(status().isUnauthorized());
        mvc.perform(post("/api/v1/relay/allocations").header("Authorization", "Bearer admin"))
                .andExpect(status().isForbidden());
        mvc.perform(post("/api/v1/relay/allocations").header("Authorization", "Bearer child"))
                .andExpect(status().isOk()).andExpect(jsonPath("$.data.port").value(9091));
        String path = "/api/v1/devices/" + id + "/relay";
        mvc.perform(get(path).header("Authorization", "Bearer child")).andExpect(status().isForbidden());
        mvc.perform(get(path).header("Authorization", "Bearer admin")).andExpect(status().isNotFound());
        when(presence.getDevice(1L, id)).thenReturn(DeviceDto.builder().childId(2L).sessionId(id).build());
        when(children.findById(2L)).thenReturn(Optional.of(Child.builder().id(2L)
                .owner(User.builder().id(3L).build()).build()));
        mvc.perform(get(path).header("Authorization", "Bearer admin")).andExpect(status().isNotFound());
        when(children.findById(2L)).thenReturn(Optional.of(Child.builder().id(2L)
                .owner(User.builder().id(1L).build()).build()));
        mvc.perform(get(path).header("Authorization", "Bearer admin"))
                .andExpect(status().isOk()).andExpect(jsonPath("$.data.instanceId").value("node"));
        when(presence.isSessionActive("2", id)).thenReturn(false);
        mvc.perform(get(path).header("Authorization", "Bearer admin")).andExpect(status().isNotFound());
    }
}
