package com.remotecontrol.api.util;

import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.Base64;
import java.util.Objects;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.PBEKeySpec;

/** Supports legacy credentials while newly reset child passwords are hashed. */
public final class ChildPasswords {
    private static final String PREFIX = "{pbkdf2-sha256}";
    private static final int ITERATIONS = 600_000;
    private ChildPasswords() {}

    public static String encode(String password) {
        byte[] salt = new byte[16];
        new SecureRandom().nextBytes(salt);
        return PREFIX + Base64.getEncoder().encodeToString(salt) + "$"
                + Base64.getEncoder().encodeToString(derive(password, salt));
    }

    public static boolean matches(String password, String stored) {
        if (password == null || stored == null) return false;
        if (!stored.startsWith(PREFIX)) return Objects.equals(password, stored);
        try {
            String[] parts = stored.substring(PREFIX.length()).split("\\$", -1);
            if (parts.length != 2) return false;
            byte[] salt = Base64.getDecoder().decode(parts[0]);
            byte[] hash = Base64.getDecoder().decode(parts[1]);
            return salt.length == 16 && hash.length == 32
                    && MessageDigest.isEqual(hash, derive(password, salt));
        } catch (IllegalArgumentException e) {
            return false;
        }
    }

    private static byte[] derive(String password, byte[] salt) {
        PBEKeySpec spec = new PBEKeySpec(password.toCharArray(), salt, ITERATIONS, 256);
        try {
            return SecretKeyFactory.getInstance("PBKDF2WithHmacSHA256").generateSecret(spec).getEncoded();
        } catch (java.security.GeneralSecurityException e) {
            throw new IllegalStateException("Password hashing unavailable", e);
        } finally {
            spec.clearPassword();
        }
    }
}
