package com.remotecontrol.api.service;

import org.junit.jupiter.api.Test;
import org.springframework.data.redis.connection.lettuce.LettuceConnectionFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import java.time.Duration;
import java.util.*;
import java.util.concurrent.*;
import static org.junit.jupiter.api.Assertions.*;

/** Explicitly run against a disposable Redis with -Dtest=LoginLeaseRedisIT. */
class LoginLeaseRedisIT {
    @Test void concurrentLoginHasOneWinnerForEachRoleAndOldSessionCannotReleaseNewOne() throws Exception {
        var factory = new LettuceConnectionFactory("127.0.0.1", Integer.getInteger("test.redis.port", 18379));
        factory.afterPropertiesSet();
        var redis = new StringRedisTemplate(factory);
        var leases = new LoginLeaseService(redis);
        var pool = Executors.newFixedThreadPool(8);
        try {
            for (String role : List.of("ADMIN", "CHILD")) {
                String id = UUID.randomUUID().toString();
                String key = "auth:login:" + role + ":" + id;
                var gate = new CountDownLatch(1);
                List<Future<String>> attempts = new ArrayList<>();
                for (int i = 0; i < 16; ++i) attempts.add(pool.submit(() -> {
                    gate.await();
                    String sid = UUID.randomUUID().toString();
                    return leases.claim(role, id, sid, "credential") ? sid : null;
                }));
                gate.countDown();
                List<String> winners = new ArrayList<>();
                for (var attempt : attempts) {
                    String winner = attempt.get(10, TimeUnit.SECONDS);
                    if (winner != null) winners.add(winner);
                }
                assertEquals(1, winners.size());
                String first = winners.get(0);
                assertTrue(leases.active(role, id, first, "credential"));
                assertFalse(leases.claim(role, id, "second", "credential"));
                leases.releaseSession(role, id, "not-owner");
                assertTrue(leases.renew(role, id, first, "credential"));
                leases.releaseSession(role, id, first);
                assertTrue(leases.claim(role, id, "second", "credential"));
                leases.releaseSession(role, id, first);
                assertTrue(leases.active(role, id, "second", "credential"));
                redis.expire(key, Duration.ofMillis(100));
                assertFalse(leases.renew(role, id, "second", "credential"), "An expired/draining session cannot revive itself");
                long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(2);
                while (Boolean.TRUE.equals(redis.hasKey(key)) && System.nanoTime() < deadline) Thread.sleep(10);
                assertTrue(leases.claim(role, id, "third", "credential"));
                assertFalse(leases.renew(role, id, "second", "credential"));
                leases.releaseSession(role, id, "second");
                assertTrue(leases.active(role, id, "third", "credential"));
                leases.releaseSession(role, id, "third");
            }
        } finally {
            pool.shutdownNow();
            factory.destroy();
        }
    }

    @Test void credentialChangeAndDrainFailClosedWithoutExtendingDrainForever() {
        var factory = new LettuceConnectionFactory("127.0.0.1", Integer.getInteger("test.redis.port", 18379));
        factory.afterPropertiesSet();
        var redis = new StringRedisTemplate(factory);
        var leases = new LoginLeaseService(redis);
        String id = UUID.randomUUID().toString();
        String key = "auth:login:CHILD:" + id;
        try {
            assertTrue(leases.claim("CHILD", id, "session", "old-credential"));
            assertFalse(leases.active("CHILD", id, "session", "new-credential"));
            leases.drain("CHILD", id, "session");
            long remaining = redis.getExpire(key, TimeUnit.MILLISECONDS);
            assertTrue(remaining > 0 && remaining <= LoginLeaseService.DRAIN_SECONDS * 1000);
            assertFalse(leases.active("CHILD", id, "session", "old-credential"));
            assertFalse(leases.renew("CHILD", id, "session", "old-credential"));
            assertFalse(leases.claim("CHILD", id, "new-session", "new-credential"));
            leases.drain("CHILD", id, "session");
            assertTrue(redis.getExpire(key, TimeUnit.MILLISECONDS) <= remaining);
        } finally { redis.delete(key); factory.destroy(); }
    }
    @Test void migrationRebindPreservesDeadlineAndCannotStealOrReviveLease() {
        var factory = new LettuceConnectionFactory("127.0.0.1", Integer.getInteger("test.redis.port", 18379));
        factory.afterPropertiesSet();
        var redis = new StringRedisTemplate(factory);
        var leases = new LoginLeaseService(redis);
        String id = UUID.randomUUID().toString();
        String key = "auth:login:ADMIN:" + id;
        try {
            assertTrue(leases.claim("ADMIN", id, "owner", "legacy"));
            long before = redis.getExpire(key, TimeUnit.MILLISECONDS);
            assertFalse(leases.rebindCredential("ADMIN", id, "intruder", "legacy", "hash"));
            assertFalse(leases.rebindCredential("ADMIN", id, "owner", "wrong-credential", "hash"));
            assertTrue(leases.rebindCredential("ADMIN", id, "owner", "legacy", "hash"));
            assertTrue(redis.getExpire(key, TimeUnit.MILLISECONDS) <= before);
            assertFalse(leases.active("ADMIN", id, "owner", "legacy"));
            assertTrue(leases.active("ADMIN", id, "owner", "hash"));
            leases.drain("ADMIN", id, "owner");
            assertFalse(leases.rebindCredential("ADMIN", id, "owner", "hash", "hash2"));
            leases.releaseSession("ADMIN", id, "owner");
            assertTrue(leases.claim("ADMIN", id, "next-owner", "hash"));
            assertFalse(leases.rebindCredential("ADMIN", id, "owner", "hash", "hash2"));
            assertTrue(leases.active("ADMIN", id, "next-owner", "hash"));
        } finally { redis.delete(key); factory.destroy(); }
    }
}
