package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.child.DeviceDto;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import org.junit.jupiter.api.Test;
import org.springframework.http.HttpStatus;
import org.springframework.web.server.ResponseStatusException;

import java.util.List;
import java.util.Optional;

import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.Mockito.*;

class DeviceServiceTest {

    @Test
    void getDevices_ReturnsOwnedDevicesOnly() {
        var users = mock(UserRepository.class);
        var children = mock(ChildRepository.class);
        var presence = mock(PresenceService.class);

        var owner = User.builder().id(1L).username("owner").build();
        var ownedChild = Child.builder().id(10L).owner(owner).build();

        when(users.findById(1L)).thenReturn(Optional.of(owner));
        when(children.findByOwner(owner)).thenReturn(List.of(ownedChild));
        when(presence.getDevices(1L)).thenReturn(List.of(
                DeviceDto.builder().sessionId("session-owned").childId(10L).deviceName("Owned PC").build(),
                DeviceDto.builder().sessionId("session-foreign").childId(99L).deviceName("Foreign PC").build()
        ));

        var service = new DeviceService(presence, children, users);
        var principal = UserPrincipal.builder().id("1").username("owner").role("ADMIN").build();

        List<DeviceDto> result = service.getDevices(principal);

        assertEquals(1, result.size());
        assertEquals("session-owned", result.get(0).getSessionId());
        assertEquals(10L, result.get(0).getChildId());
    }

    @Test
    void getDevices_ThrowsUnauthorized_WhenUserNotFound() {
        var users = mock(UserRepository.class);
        var children = mock(ChildRepository.class);
        var presence = mock(PresenceService.class);

        when(users.findById(1L)).thenReturn(Optional.empty());

        var service = new DeviceService(presence, children, users);
        var principal = UserPrincipal.builder().id("1").username("owner").role("ADMIN").build();

        ResponseStatusException ex = assertThrows(ResponseStatusException.class, () -> service.getDevices(principal));
        assertEquals(HttpStatus.UNAUTHORIZED, ex.getStatusCode());
        verifyNoInteractions(children);
        verifyNoInteractions(presence);
    }

    @Test
    void getDevices_ReturnsEmptyList_WhenNoDevicesOnline() {
        var users = mock(UserRepository.class);
        var children = mock(ChildRepository.class);
        var presence = mock(PresenceService.class);

        var owner = User.builder().id(1L).username("owner").build();
        when(users.findById(1L)).thenReturn(Optional.of(owner));
        when(children.findByOwner(owner)).thenReturn(List.of());
        when(presence.getDevices(1L)).thenReturn(List.of());

        var service = new DeviceService(presence, children, users);
        var principal = UserPrincipal.builder().id("1").username("owner").role("ADMIN").build();

        List<DeviceDto> result = service.getDevices(principal);

        assertTrue(result.isEmpty());
    }
}
