package com.remotecontrol.api.controller;

import com.remotecontrol.api.config.JwtInterceptor;
import com.remotecontrol.api.config.GlobalExceptionHandler;
import com.remotecontrol.api.dto.child.DeviceDto;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import com.remotecontrol.api.service.DeviceService;
import com.remotecontrol.api.service.PresenceService;
import com.remotecontrol.api.util.JwtUtil;
import org.junit.jupiter.api.Test;
import org.springframework.test.web.servlet.setup.MockMvcBuilders;
import java.util.*;
import static org.mockito.Mockito.*;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

class DeviceAuthorizationTest {
    @Test void devicesRequireAdminAndNeverReturnDeletedOrForeignAccounts() throws Exception {
        var users = mock(UserRepository.class);
        var children = mock(ChildRepository.class);
        var presence = mock(PresenceService.class);
        var jwt = new JwtUtil();
        var owner = User.builder().id(1L).username("owner").build();
        when(users.findById(1L)).thenReturn(Optional.of(owner));
        when(children.findByOwner(owner)).thenReturn(List.of(Child.builder().id(12L).owner(owner).build()));
        when(presence.getDevices(1L)).thenReturn(List.of(
                DeviceDto.builder().sessionId("a").childId(12L).build(),
                DeviceDto.builder().sessionId("b").childId(99L).build()));
        var deviceService = new DeviceService(presence, children, users);
        var mvc = MockMvcBuilders.standaloneSetup(new DeviceController(deviceService, new RelayAuthorizationController(children, presence, mock(com.remotecontrol.api.service.RelayDispatchService.class)), mock(com.remotecontrol.api.service.RelayDispatchService.class)))
                .setControllerAdvice(new GlobalExceptionHandler()).addInterceptors(new JwtInterceptor(jwt, presence)).build();
        mvc.perform(get("/api/v1/devices")).andExpect(status().isUnauthorized());
        mvc.perform(get("/api/v1/devices").header("Authorization", "Bearer invalid"))
                .andExpect(status().isUnauthorized());
        mvc.perform(get("/api/v1/devices").header("Authorization", "Bearer " + jwt.generateToken("owner", "ADMIN", "1")))
                .andExpect(status().isOk()).andExpect(jsonPath("$.data.length()").value(1))
                .andExpect(jsonPath("$.data[0].sessionId").value("a"));
        when(presence.isSessionActive("12", "child-session")).thenReturn(true);
        mvc.perform(get("/api/v1/devices").header("Authorization", "Bearer " + jwt.generateToken("child", "CHILD", "12", "child-session")))
                .andExpect(status().isForbidden());
        when(presence.getDevices(1L)).thenThrow(new org.springframework.data.redis.RedisConnectionFailureException("down"));
        mvc.perform(get("/api/v1/devices").header("Authorization", "Bearer " + jwt.generateToken("owner", "ADMIN", "1")))
                .andExpect(status().isServiceUnavailable()).andExpect(jsonPath("$.success").value(false));
    }

    @Test void relayAuthorizesOnlyOwnedLiveSessionsAndRejectsOldChildTokens() throws Exception {
        var children = mock(ChildRepository.class);
        var presence = mock(PresenceService.class);
        var jwt = new JwtUtil();
        String sid = UUID.randomUUID().toString();
        var owner = User.builder().id(1L).build();
        when(presence.getDevice(1L, sid)).thenReturn(DeviceDto.builder().childId(12L).sessionId(sid).build());
        when(children.findById(12L)).thenReturn(Optional.of(Child.builder().id(12L).owner(owner).build()));
        when(presence.isSessionActive("12", sid)).thenReturn(true);
        var mvc = MockMvcBuilders.standaloneSetup(new RelayAuthorizationController(children, presence, mock(com.remotecontrol.api.service.RelayDispatchService.class)))
                .setControllerAdvice(new GlobalExceptionHandler()).addInterceptors(new JwtInterceptor(jwt, presence)).build();
        mvc.perform(get("/api/v1/relay/targets/" + sid).header("Authorization", "Bearer " + jwt.generateToken("owner", "ADMIN", "1")))
                .andExpect(status().isOk()).andExpect(content().string(sid));
        mvc.perform(get("/api/v1/relay/targets/" + sid).header("Authorization", "Bearer " + jwt.generateToken("other", "ADMIN", "2")))
                .andExpect(status().isNotFound());
        mvc.perform(get("/api/v1/relay/agent").header("Authorization", "Bearer " + jwt.generateToken("child", "CHILD", "12")))
                .andExpect(status().isUnauthorized());
    }
}
