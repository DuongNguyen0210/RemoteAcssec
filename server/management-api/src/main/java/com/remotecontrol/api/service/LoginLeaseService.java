package com.remotecontrol.api.service;

import com.remotecontrol.api.util.ChildPasswords;
import lombok.RequiredArgsConstructor;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.data.redis.core.script.DefaultRedisScript;
import org.springframework.stereotype.Service;
import java.util.List;

/** One shared, expiring login slot per account; every mutation compares the session owner. */
@Service
@RequiredArgsConstructor
public class LoginLeaseService {
    public static final int ACTIVE_SECONDS = 60;
    // Keep an expired slot briefly so existing relay authorization leases can close first.
    public static final int DRAIN_SECONDS = 30;
    private final StringRedisTemplate redis;
    private static final DefaultRedisScript<Long> CLAIM = new DefaultRedisScript<>(
            "if redis.call('SET',KEYS[1],ARGV[1],'NX','EX',ARGV[2]) then return 1 else return 0 end", Long.class);
    private static final DefaultRedisScript<Long> CHECK = new DefaultRedisScript<>(
            "if redis.call('GET',KEYS[1]) ~= ARGV[1] then return 0 end " +
            "if redis.call('PTTL',KEYS[1]) <= tonumber(ARGV[2]) then return 0 end; return 1", Long.class);
    private static final DefaultRedisScript<Long> RENEW = new DefaultRedisScript<>(
            "if redis.call('GET',KEYS[1]) ~= ARGV[1] then return 0 end " +
            "if redis.call('PTTL',KEYS[1]) <= tonumber(ARGV[2]) then return 0 end " +
            "redis.call('EXPIRE',KEYS[1],ARGV[3]); return 1", Long.class);
    private static final DefaultRedisScript<Long> RELEASE = new DefaultRedisScript<>(
            "if redis.call('GET',KEYS[1]) ~= ARGV[1] then return 0 end " +
            "return redis.call('DEL',KEYS[1])", Long.class);

    private static final DefaultRedisScript<Long> OWNS = new DefaultRedisScript<>(
            "local v=redis.call('GET',KEYS[1]); if not v or string.sub(v,1,string.len(ARGV[1])) ~= ARGV[1] then return 0 end " +
            "if redis.call('PTTL',KEYS[1]) <= tonumber(ARGV[2]) then return 0 end; return 1", Long.class);
    public boolean owns(String role, String id, String session) {
        return session != null && Long.valueOf(1).equals(redis.execute(OWNS, List.of(key(role, id)),
                session + ":", Integer.toString(DRAIN_SECONDS * 1000)));
    }

    private static final DefaultRedisScript<Long> DRAIN = new DefaultRedisScript<>(
            "local v=redis.call('GET',KEYS[1]); if not v or string.sub(v,1,string.len(ARGV[1])) ~= ARGV[1] then return 0 end " +
            "if redis.call('PTTL',KEYS[1]) > tonumber(ARGV[2]) then redis.call('PEXPIRE',KEYS[1],ARGV[2]) end; return 1", Long.class);
    public void drain(String role, String id, String session) {
        if (session != null) redis.execute(DRAIN, List.of(key(role, id)), session + ":", Integer.toString(DRAIN_SECONDS * 1000));
    }

    private static final DefaultRedisScript<Long> RELEASE_SESSION = new DefaultRedisScript<>(
            "local v=redis.call('GET',KEYS[1]); if not v or string.sub(v,1,string.len(ARGV[1])) ~= ARGV[1] then return 0 end " +
            "return redis.call('DEL',KEYS[1])", Long.class);
    public void releaseSession(String role, String id, String session) {
        if (session != null) redis.execute(RELEASE_SESSION, List.of(key(role, id)), session + ":");
    }

    private String key(String role, String id) { return "auth:login:" + role + ":" + id; }
    private String value(String session, String credential) {
        if (session == null || session.isBlank() || credential == null || credential.isBlank()) return null;
        return session + ":" + ChildPasswords.fingerprint(credential);
    }
    public boolean claim(String role, String id, String session, String credential) {
        String value = value(session, credential);
        return value != null && Long.valueOf(1).equals(redis.execute(CLAIM, List.of(key(role, id)),
                value, Integer.toString(ACTIVE_SECONDS + DRAIN_SECONDS)));
    }
    public boolean active(String role, String id, String session, String credential) {
        String value = value(session, credential);
        return value != null && Long.valueOf(1).equals(redis.execute(CHECK, List.of(key(role, id)),
                value, Integer.toString(DRAIN_SECONDS * 1000)));
    }
    // Upgrade only this login's binding, preserving its remaining active/drain deadline.
    private static final DefaultRedisScript<Long> REBIND = new DefaultRedisScript<>(
            "if redis.call('GET',KEYS[1]) ~= ARGV[1] then return 0 end " +
            "local ttl=redis.call('PTTL',KEYS[1]); if ttl <= tonumber(ARGV[3]) then return 0 end " +
            "redis.call('SET',KEYS[1],ARGV[2],'PX',ttl); return 1", Long.class);
    public boolean rebindCredential(String role, String id, String session, String previous, String replacement) {
        String oldValue = value(session, previous), newValue = value(session, replacement);
        return oldValue != null && newValue != null && Long.valueOf(1).equals(redis.execute(REBIND,
                List.of(key(role, id)), oldValue, newValue, Integer.toString(DRAIN_SECONDS * 1000)));
    }

    public boolean renew(String role, String id, String session, String credential) {
        String value = value(session, credential);
        return value != null && Long.valueOf(1).equals(redis.execute(RENEW, List.of(key(role, id)),
                value, Integer.toString(DRAIN_SECONDS * 1000), Integer.toString(ACTIVE_SECONDS + DRAIN_SECONDS)));
    }
    public void release(String role, String id, String session, String credential) {
        String value = value(session, credential);
        if (value != null) redis.execute(RELEASE, List.of(key(role, id)), value);
    }
}
