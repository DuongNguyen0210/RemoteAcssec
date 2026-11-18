package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.auth.LoginRequest;
import com.remotecontrol.api.entity.*;
import com.remotecontrol.api.repository.*;
import com.remotecontrol.api.util.*;
import org.junit.jupiter.api.Test;
import java.util.Optional;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.Mockito.*;
import static org.mockito.ArgumentMatchers.*;

class SingleLoginTest {
    @Test void secondLoginForEitherRoleDoesNotMintTokenOrCreateChildPresence() {
        for (String role : new String[]{"ADMIN", "CHILD"}) {
            var users = mock(UserRepository.class);
            var children = mock(ChildRepository.class);
            var leases = mock(LoginLeaseService.class);
            var presence = mock(PresenceService.class);
            var jwt = mock(JwtUtil.class);
            if (role.equals("ADMIN")) when(users.findByUsername("account"))
                    .thenReturn(Optional.of(User.builder().id(1L).password("password").build()));
            else when(children.findByUsername("account"))
                    .thenReturn(Optional.of(Child.builder().id(1L).password(ChildPasswords.encode("password")).build()));
            var auth = new AuthService(users, children, jwt, presence, leases);
            var result = auth.login(new LoginRequest("account", "password"));
            assertFalse(result.getSuccess());
            assertEquals("ACCOUNT_IN_USE", result.getErrorCode());
            verifyNoInteractions(jwt, presence);
        }
    }

    @Test void adminTokenAndLoginResponseUseTheSameClaimedSession() {
        var users = mock(UserRepository.class);
        var children = mock(ChildRepository.class);
        var leases = mock(LoginLeaseService.class);
        var presence = mock(PresenceService.class);
        var jwt = new JwtUtil("single-login-test-secret-of-at-least-32-bytes");
        when(users.findByUsername("owner")).thenReturn(Optional.of(User.builder().id(1L).password("password").build()));
        when(leases.claim(eq("ADMIN"), eq("1"), anyString(), eq("password"))).thenReturn(true);
        var result = new AuthService(users, children, jwt, presence, leases).login(new LoginRequest("owner", "password"));
        assertTrue(result.getSuccess());
        var data = result.getData();
        assertNotNull(java.util.UUID.fromString(data.getSessionId()));
        assertEquals(data.getSessionId(), jwt.extractSessionId(data.getToken()));
        verify(leases).claim("ADMIN", "1", data.getSessionId(), "password");
    }

    @Test void failedPresenceWriteReleasesOnlyTheClaimedChildLogin() {
        var users = mock(UserRepository.class);
        var children = mock(ChildRepository.class);
        var leases = mock(LoginLeaseService.class);
        var presence = mock(PresenceService.class);
        String credential = ChildPasswords.encode("password");
        when(children.findByUsername("child")).thenReturn(Optional.of(Child.builder().id(2L).password(credential).build()));
        when(leases.claim(eq("CHILD"), eq("2"), anyString(), eq(credential))).thenReturn(true);
        doThrow(new IllegalStateException("storage failed")).when(presence).openSession(eq(2L), anyString(), eq(credential));
        var auth = new AuthService(users, children, mock(JwtUtil.class), presence, leases);
        assertThrows(IllegalStateException.class, () -> auth.login(new LoginRequest("child", "password")));
        verify(leases).release(eq("CHILD"), eq("2"), anyString(), eq(credential));
    }
}
