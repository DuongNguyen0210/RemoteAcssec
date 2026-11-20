package com.remotecontrol.api.service;

import com.remotecontrol.api.dto.auth.LoginRequest;
import com.remotecontrol.api.dto.user.ChangePasswordRequest;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.*;
import com.remotecontrol.api.repository.*;
import com.remotecontrol.api.util.*;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;
import java.util.Optional;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.Mockito.*;

/** PR #40 credential tests adapted to main's credential-bound single-login leases. */
class SecurityCredentialsTest {
    private final UserRepository users = mock(UserRepository.class);
    private final ChildRepository children = mock(ChildRepository.class);
    private final LoginLeaseService leases = mock(LoginLeaseService.class);
    private final PresenceService presence = mock(PresenceService.class);
    private final JwtUtil jwt = new JwtUtil("security-test-only-secret-of-at-least-32-bytes");
    private final AuthService auth = new AuthService(users, children, jwt, presence, leases);

    @Test void newAdminsAreSaltedAndHashed() {
        auth.registerAdmin(new LoginRequest("owner", "example-password"));
        var admin = ArgumentCaptor.forClass(User.class);
        verify(users).save(admin.capture());
        String stored = admin.getValue().getPassword();
        assertFalse(ChildPasswords.needsUpgrade(stored));
        assertTrue(ChildPasswords.matches("example-password", stored));
        assertNotEquals(stored, ChildPasswords.encode("example-password"));
        assertFalse(ChildPasswords.matches(stored, stored));
    }

    @Test void registrationRejectsInvalidInputBeforeAccessingTheDatabase() {
        assertEquals("INVALID_INPUT", auth.registerAdmin(new LoginRequest("bad name", "example-password")).getErrorCode());
        assertEquals("INVALID_INPUT", auth.registerAdmin(new LoginRequest("x".repeat(49), "example-password")).getErrorCode());
        assertEquals("INVALID_INPUT", auth.registerAdmin(new LoginRequest("owner", "short")).getErrorCode());
        assertEquals("INVALID_INPUT", auth.registerAdmin(new LoginRequest("owner", "x".repeat(256))).getErrorCode());
        verifyNoInteractions(users, children);
    }

    @Test void registrationCannotClaimAnExistingChildLoginName() {
        when(children.findByUsername("owner-child")).thenReturn(Optional.of(Child.builder().build()));
        assertEquals("USER_ALREADY_EXISTS", auth.registerAdmin(new LoginRequest("owner-child", "example-password")).getErrorCode());
        verify(users).lockAccountName("owner-child");
        verify(users, never()).save(any());
    }

    private User legacy() {
        var account = User.builder().id(1L).username("owner").password("legacy").build();
        when(users.findByUsername("owner")).thenReturn(Optional.of(account));
        when(users.findById(1L)).thenReturn(Optional.of(account));
        when(leases.claim(eq("ADMIN"), eq("1"), anyString(), eq("legacy"))).thenReturn(true);
        return account;
    }

    @Test void legacyMigrationKeepsTheSameLiveSessionAndBindsItsNewHash() {
        var account = legacy();
        when(users.upgradePassword(eq(1L), eq("legacy"), anyString())).thenAnswer(call -> {
            account.setPassword(call.getArgument(2)); return 1;
        });
        when(leases.rebindCredential(eq("ADMIN"), eq("1"), anyString(), eq("legacy"), anyString())).thenReturn(true);
        var result = auth.login(new LoginRequest("owner", "legacy"));
        assertTrue(result.getSuccess());
        assertTrue(ChildPasswords.matches("legacy", account.getPassword()));
        String sid = result.getData().getSessionId();
        assertEquals(sid, jwt.extractSessionId(result.getData().getToken()));
        verify(leases).rebindCredential("ADMIN", "1", sid, "legacy", account.getPassword());
        verify(leases, never()).releaseSession(anyString(), anyString(), anyString());
    }

    @Test void concurrentResetCannotBeOverwrittenOrMintAToken() {
        legacy(); // CAS defaults to zero: someone changed the DB credential first.
        var result = auth.login(new LoginRequest("owner", "legacy"));
        assertEquals("AUTH_RETRY", result.getErrorCode());
        assertNull(result.getData());
        verify(leases, never()).rebindCredential(anyString(), anyString(), anyString(), anyString(), anyString());
        verify(leases).releaseSession(eq("ADMIN"), eq("1"), anyString());
    }

    @Test void failedRebindReleasesOnlyItsOwnClaimAndDoesNotIssueToken() {
        legacy();
        when(users.upgradePassword(eq(1L), eq("legacy"), anyString())).thenReturn(1);
        assertEquals("AUTH_RETRY", auth.login(new LoginRequest("owner", "legacy")).getErrorCode());
        var owner = ArgumentCaptor.forClass(String.class);
        verify(leases).claim(eq("ADMIN"), eq("1"), owner.capture(), eq("legacy"));
        verify(leases).releaseSession("ADMIN", "1", owner.getValue());
        verifyNoInteractions(presence);
    }

    @Test void resetAfterHashVerificationFailsClosed() {
        var account = User.builder().id(1L).password(ChildPasswords.encode("old-password")).build();
        when(users.findByUsername("owner")).thenReturn(Optional.of(account));
        when(users.findById(1L)).thenReturn(Optional.of(User.builder().password(ChildPasswords.encode("new-password")).build()));
        when(leases.claim(eq("ADMIN"), eq("1"), anyString(), anyString())).thenReturn(true);
        assertEquals("AUTH_RETRY", auth.login(new LoginRequest("owner", "old-password")).getErrorCode());
        verify(leases).releaseSession(eq("ADMIN"), eq("1"), anyString());
    }

    @Test void legacyChildPresenceUsesTheUpgradedCredential() {
        var child = Child.builder().id(2L).password("legacy-child").build();
        when(children.findByUsername("child")).thenReturn(Optional.of(child));
        when(children.findById(2L)).thenReturn(Optional.of(child));
        when(leases.claim(eq("CHILD"), eq("2"), anyString(), eq("legacy-child"))).thenReturn(true);
        when(children.upgradePassword(eq(2L), eq("legacy-child"), anyString())).thenAnswer(call -> {
            child.setPassword(call.getArgument(2)); return 1;
        });
        when(leases.rebindCredential(eq("CHILD"), eq("2"), anyString(), eq("legacy-child"), anyString())).thenReturn(true);
        var result = auth.login(new LoginRequest("child", "legacy-child"));
        assertTrue(result.getSuccess());
        verify(presence).openSession(2L, result.getData().getSessionId(), child.getPassword());
        assertTrue(ChildPasswords.matches("legacy-child", child.getPassword()));
    }

    @Test void hashedAdminChangesPasswordConditionallyAndOldTokenBecomesInactive() {
        var account = User.builder().id(1L).password(ChildPasswords.encode("old-password")).build();
        when(users.findByUsername("owner")).thenReturn(Optional.of(account));
        when(users.findById(1L)).thenReturn(Optional.of(account));
        String previous = account.getPassword();
        when(leases.claim(eq("ADMIN"), eq("1"), anyString(), eq(previous))).thenReturn(true);
        assertTrue(auth.login(new LoginRequest("owner", "old-password")).getSuccess());
        assertFalse(auth.login(new LoginRequest("owner", previous)).getSuccess());
        var request = new ChangePasswordRequest();
        request.setOldPassword("old-password"); request.setNewPassword("new-password");
        var user = UserPrincipal.builder().username("owner").build();
        assertEquals("PASSWORD_CHANGED", new UserService(users).changePassword(request, user).getErrorCode());
        when(users.upgradePassword(eq(1L), eq(previous), anyString())).thenAnswer(call -> {
            account.setPassword(call.getArgument(2)); return 1;
        });
        assertTrue(new UserService(users).changePassword(request, user).getSuccess());
        assertTrue(ChildPasswords.matches("new-password", account.getPassword()));
        assertFalse(auth.isSessionActive("ADMIN", "1", "old-session"));
        verify(leases).drain("ADMIN", "1", "old-session");
        verify(users, never()).save(any());
    }
}
