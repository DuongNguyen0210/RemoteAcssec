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
