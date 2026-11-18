package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.auth.LoginRequest;
import com.remotecontrol.api.dto.user.ChangePasswordRequest;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.repository.UserRepository;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.util.ChildPasswords;
import com.remotecontrol.api.util.JwtUtil;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;
import java.util.Optional;
import java.util.Base64;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.Mockito.*;

class SecurityCredentialsTest {
    private final UserRepository users = mock(UserRepository.class);
    private final ChildRepository children = mock(ChildRepository.class);
    private final JwtUtil jwt = new JwtUtil(Base64.getEncoder().encodeToString(new byte[32]));
    private final AuthService auth = new AuthService(users, children, jwt, mock(PresenceService.class));

    @Test void newAccountsAreHashed() {
        auth.registerAdmin(new LoginRequest("owner", "example-password"));
        var admin = ArgumentCaptor.forClass(User.class);
        verify(users).save(admin.capture());
        assertNotEquals("example-password", admin.getValue().getPassword());
        assertTrue(ChildPasswords.matches("example-password", admin.getValue().getPassword()));
        new ChildService(children, users, mock(PresenceService.class))
                .addChild("-child", "example-password", User.builder().username("owner").build());
        var child = ArgumentCaptor.forClass(Child.class);
        verify(children).save(child.capture());
        assertTrue(ChildPasswords.matches("example-password", child.getValue().getPassword()));
        assertNotEquals("example-password", child.getValue().getPassword());
    }

    @Test void legacyUpgradeIsConditionalAndCannotOverwriteConcurrentChanges() {
        var user = User.builder().id(1L).username("owner").password("legacy").build();
        when(users.findByUsername("owner")).thenReturn(Optional.of(user));
        when(users.upgradePassword(eq(1L), eq("legacy"), anyString())).thenReturn(1);
        assertTrue(auth.login(new LoginRequest("owner", "legacy")).getSuccess());
        var hash = ArgumentCaptor.forClass(String.class);
        verify(users).upgradePassword(eq(1L), eq("legacy"), hash.capture());
        assertTrue(ChildPasswords.matches("legacy", hash.getValue()));
        when(users.upgradePassword(eq(1L), eq("legacy"), anyString())).thenReturn(0);
        assertFalse(auth.login(new LoginRequest("owner", "legacy")).getSuccess());
    }

    @Test void hashedAdminCanLoginAndChangePassword() {
        var user = User.builder().id(1L).username("owner")
                .password(ChildPasswords.encode("old-password")).build();
        when(users.findByUsername("owner")).thenReturn(Optional.of(user));
        assertTrue(auth.login(new LoginRequest("owner", "old-password")).getSuccess());
        assertFalse(auth.login(new LoginRequest("owner", user.getPassword())).getSuccess());
        var request = new ChangePasswordRequest();
        request.setOldPassword("old-password");
        request.setNewPassword("new-password");
        when(users.upgradePassword(eq(1L), eq(user.getPassword()), anyString())).thenReturn(1);
        assertTrue(new UserService(users).changePassword(request,
                UserPrincipal.builder().username("owner").build()).getSuccess());
        verify(users, never()).save(any());
    }

    @Test void jwtRequiresValidPrivateConfiguration() {
        assertThrows(IllegalArgumentException.class, () -> new JwtUtil(""));
        assertThrows(IllegalArgumentException.class, () -> new JwtUtil("not-base64!"));
        assertThrows(IllegalArgumentException.class, () -> new JwtUtil(Base64.getEncoder().encodeToString(new byte[16])));
        var token = jwt.generateToken("owner", "ADMIN", "1");
        assertEquals("owner", jwt.extractUsername(token));
        var other = new byte[32];
        other[0] = 1;
        var different = new JwtUtil(Base64.getEncoder().encodeToString(other));
        assertThrows(RuntimeException.class, () -> different.extractUsername(token));
    }
}
