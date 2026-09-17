package com.remotecontrol.api.controller;

import com.remotecontrol.api.config.GlobalExceptionHandler;
import com.remotecontrol.api.config.JwtInterceptor;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import com.remotecontrol.api.service.ChildService;
import com.remotecontrol.api.service.PresenceService;
import com.remotecontrol.api.util.JwtUtil;
import org.junit.jupiter.api.Test;
import org.springframework.test.web.servlet.setup.MockMvcBuilders;
import org.springframework.http.MediaType;
import java.util.Optional;
import static org.mockito.Mockito.*;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.put;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

class ChildUpdateControllerTest {
    @Test void endpointValidatesInputRoleAndOwnership() throws Exception {
        var children = mock(ChildRepository.class);
        var presence = mock(PresenceService.class);
        var jwt = new JwtUtil();
        var owner = User.builder().id(1L).username("owner").build();
        when(children.findById(12L)).thenReturn(Optional.of(Child.builder()
                .id(12L).owner(owner).username("ownerchild").password("existing").build()));
        var mvc = MockMvcBuilders.standaloneSetup(new ChildController(
                new ChildService(children, mock(UserRepository.class), presence)))
                .setControllerAdvice(new GlobalExceptionHandler())
                .addInterceptors(new JwtInterceptor(jwt, presence)).build();
        String route = "/api/v1/child/12";
        String body = "{\"childUsername\":\"renamed\"}";
        String admin = "Bearer " + jwt.generateToken("owner", "ADMIN", "1");
        mvc.perform(put(route).contentType(MediaType.APPLICATION_JSON).content(body))
                .andExpect(status().isUnauthorized());
        mvc.perform(put(route).header("Authorization", "Bearer " + jwt.generateToken("other", "ADMIN", "2"))
                .contentType(MediaType.APPLICATION_JSON).content(body)).andExpect(status().isForbidden());
        when(presence.isSessionActive("12", "sid")).thenReturn(true);
        mvc.perform(put(route).header("Authorization", "Bearer " + jwt.generateToken("ownerchild", "CHILD", "12", "sid"))
                .contentType(MediaType.APPLICATION_JSON).content(body)).andExpect(status().isForbidden());
        mvc.perform(put(route).header("Authorization", admin).contentType(MediaType.APPLICATION_JSON)
                .content("{\"childUsername\":\" \"}")).andExpect(status().isBadRequest());
        mvc.perform(put(route).header("Authorization", admin).contentType(MediaType.APPLICATION_JSON).content(body))
                .andExpect(status().isOk()).andExpect(jsonPath("$.data.username").value("ownerrenamed"))
                .andExpect(jsonPath("$.data.password").doesNotExist());
    }
}
