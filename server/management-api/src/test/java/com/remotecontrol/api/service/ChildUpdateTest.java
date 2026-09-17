package com.remotecontrol.api.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.child.UpdateChildRequest;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.Child;
import com.remotecontrol.api.entity.User;
import com.remotecontrol.api.repository.ChildRepository;
import com.remotecontrol.api.repository.UserRepository;
import com.remotecontrol.api.util.ChildPasswords;
import org.junit.jupiter.api.Test;
import java.util.Optional;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.Mockito.*;

class ChildUpdateTest {
    private final ChildRepository children = mock(ChildRepository.class);
    private final ChildService service = new ChildService(children, mock(UserRepository.class), mock(PresenceService.class));
    private final User owner = User.builder().id(1L).username("admin").build();
    private final Child child = Child.builder().id(2L).owner(owner).username("adminold").password("old-password").build();
    private final UserPrincipal principal = UserPrincipal.builder().id("1").username("admin").role("ADMIN").build();
    private UpdateChildRequest request(String password) {
        var request = new UpdateChildRequest();
        request.setChildUsername("new");
        request.setNewPassword(password);
        when(children.findById(2L)).thenReturn(Optional.of(child));
        return request;
    }
    @Test void otherOwnerCannotChangeAccount() {
        var request = request("new-password");
        principal.setId("9");
        assertEquals("FORBIDDEN", service.updateChild(2L, request, principal).getErrorCode());
        verify(children, never()).saveAndFlush(any());
        assertEquals("old-password", child.getPassword());
    }
    @Test void renameWithoutPasswordPreservesCredential() {
        assertTrue(service.updateChild(2L, request(""), principal).getSuccess());
        assertEquals("adminnew", child.getUsername());
        assertEquals("old-password", child.getPassword());
    }
    @Test void resetHashesPasswordAndNeverReturnsIt() throws Exception {
        var result = service.updateChild(2L, request("new-password"), principal);
        assertTrue(result.getSuccess());
        assertTrue(ChildPasswords.matches("new-password", child.getPassword()));
        assertFalse(ChildPasswords.matches("old-password", child.getPassword()));
        assertNotEquals("new-password", child.getPassword());
        String json = new ObjectMapper().writeValueAsString(result);
        assertFalse(json.contains("password"));
        assertFalse(json.contains(child.getPassword()));
    }
    @Test void duplicateRenameDoesNotChangeCredential() {
        var request = request("new-password");
        when(children.findByUsername("adminnew")).thenReturn(Optional.of(Child.builder().id(3L).build()));
        assertEquals("CHILD_ALREADY_EXISTS", service.updateChild(2L, request, principal).getErrorCode());
        assertEquals("adminold", child.getUsername());
        verify(children, never()).saveAndFlush(any());
    }
    @Test void blankPasswordAndOversizedNameAreRejected() {
        var request = request("   ");
        assertEquals("INVALID_INPUT", service.updateChild(2L, request, principal).getErrorCode());
        request.setNewPassword(null);
        request.setChildUsername("x".repeat(50));
        assertEquals("INVALID_INPUT", service.updateChild(2L, request, principal).getErrorCode());
        verify(children, never()).saveAndFlush(any());
    }
    @Test void resetCredentialCanLoginAndOldPasswordCannot() {
        service.updateChild(2L, request("new-password"), principal);
        when(children.findByUsername("adminnew")).thenReturn(Optional.of(child));
        var presence = mock(PresenceService.class);
        var auth = new AuthService(mock(UserRepository.class), children,
                new com.remotecontrol.api.util.JwtUtil(), presence);
        assertTrue(auth.login(new com.remotecontrol.api.dto.auth.LoginRequest("adminnew", "new-password")).getSuccess());
        assertFalse(auth.login(new com.remotecontrol.api.dto.auth.LoginRequest("adminnew", "old-password")).getSuccess());
        verify(presence).openSession(eq(2L), anyString());
    }
    @Test void legacyCredentialsRemainUsableAndHashCannotBeUsedAsPassword() {
        assertTrue(ChildPasswords.matches("legacy", "legacy"));
        var hash = ChildPasswords.encode("new-password");
        assertFalse(ChildPasswords.matches(hash, hash));
        assertFalse(ChildPasswords.matches("anything", "{pbkdf2-sha256}broken"));
    }
}
