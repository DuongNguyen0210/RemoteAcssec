package com.remotecontrol.api.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import org.junit.jupiter.api.Test;
import java.util.List;
import java.util.Optional;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.Mockito.*;

class AccountContractTest {
    @Test void registrationTrimsNameHashesPasswordAndBoundsFullLogin() {
        var users = mock(UserRepository.class);
        var children = mock(ChildRepository.class);
        var owner = User.builder().id(1L).username("admin").maxChildren(5).build();
        when(users.findByUsernameForUpdate("admin")).thenReturn(Optional.of(owner));
        when(children.findByOwner(owner)).thenReturn(List.of());
        when(children.save(any())).thenAnswer(call -> { Child c = call.getArgument(0); c.setId(2L); return c; });
        var service = new ChildService(children, users, mock(PresenceService.class));
        var principal = UserPrincipal.builder().id("1").username("admin").role("ADMIN").build();
        var request = new com.remotecontrol.api.dto.child.RegisterRequest(" child ", "new-password");
        var result = service.register(request, principal);
        assertTrue(result.getSuccess());
        assertEquals("adminchild", result.getData().getUsername());
        var stored = org.mockito.ArgumentCaptor.forClass(Child.class);
        verify(children).save(stored.capture());
        assertNotEquals("new-password", stored.getValue().getPassword());
        assertTrue(com.remotecontrol.api.util.ChildPasswords.matches("new-password", stored.getValue().getPassword()));
        assertEquals("INVALID_INPUT", service.register(new com.remotecontrol.api.dto.child.RegisterRequest("x".repeat(50), "new-password"), principal).getErrorCode());
        when(users.existsByUsername("adminreserved")).thenReturn(true);
        assertEquals("CHILD_ALREADY_EXISTS", service.register(new com.remotecontrol.api.dto.child.RegisterRequest("reserved", "new-password"), principal).getErrorCode());
    }

    @Test void accountListDoesNotDependOnRedisOrExposeDeviceFieldsAndPasswords() throws Exception {
        User owner = User.builder().id(1L).username("admin").build();
        Child account = Child.builder().id(2L).username("adminchild").password("private").owner(owner).build();
        var users = mock(UserRepository.class);
        var children = mock(ChildRepository.class);
        var presence = mock(PresenceService.class);
        when(users.findByUsername("admin")).thenReturn(Optional.of(owner));
        when(children.findByOwner(owner)).thenReturn(List.of(account));
        var service = new ChildService(children, users, presence);
        var result = service.getListChildren(UserPrincipal.builder().id("1").username("admin").role("ADMIN").build());
        String json = new ObjectMapper().writeValueAsString(result);
        assertTrue(result.getSuccess());
        assertEquals(1, result.getData().size());
        assertFalse(json.contains("password"), json);
        assertFalse(json.contains("online"), json);
        assertFalse(json.contains("deviceUid"), json);
        verifyNoInteractions(presence);
    }
}
