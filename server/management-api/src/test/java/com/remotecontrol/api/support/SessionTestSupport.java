package com.remotecontrol.api.support;
import com.remotecontrol.api.service.*;
import static org.mockito.Mockito.*;
import static org.mockito.ArgumentMatchers.*;

/** Controller unit fixtures delegate CHILD validity to the existing presence stub. */
public final class SessionTestSupport {
    private SessionTestSupport() {}
    public static AuthService auth(PresenceService presence) {
        var auth = mock(AuthService.class);
        when(auth.isSessionActive(anyString(), anyString(), anyString())).thenAnswer(call -> {
            String role = call.getArgument(0), id = call.getArgument(1), session = call.getArgument(2);
            return "ADMIN".equals(role) || ("CHILD".equals(role) && presence.isSessionActive(id, session));
        });
        return auth;
    }
    public static LoginLeaseService validLease() {
        var leases = mock(LoginLeaseService.class);
        when(leases.active(anyString(), anyString(), anyString(), anyString())).thenReturn(true);
        when(leases.claim(anyString(), anyString(), anyString(), anyString())).thenReturn(true);
        return leases;
    }
}
