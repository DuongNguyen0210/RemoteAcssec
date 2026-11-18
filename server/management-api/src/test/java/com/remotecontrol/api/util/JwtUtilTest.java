package com.remotecontrol.api.util;

import io.jsonwebtoken.security.WeakKeyException;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

class JwtUtilTest {
    @Test void tokensAreSharedOnlyByInstancesWithTheSameConfiguredSecret() {
        var first = new JwtUtil("a-test-secret-that-is-at-least-thirty-two-bytes");
        var second = new JwtUtil("a-test-secret-that-is-at-least-thirty-two-bytes");
        var different = new JwtUtil("another-test-secret-of-at-least-thirty-two-bytes");
        String token = first.generateToken("alice", "ADMIN", "42");

        assertEquals("alice", second.extractUsername(token));
        assertThrows(RuntimeException.class, () -> different.extractUsername(token));
        assertThrows(WeakKeyException.class, () -> new JwtUtil("short"));
    }
}
