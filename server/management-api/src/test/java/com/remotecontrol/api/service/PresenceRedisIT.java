package com.remotecontrol.api.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.child.HeartbeatRequest;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.User;
import org.junit.jupiter.api.Test;
import org.springframework.data.redis.connection.lettuce.LettuceConnectionFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import java.util.UUID;
import static org.junit.jupiter.api.Assertions.*;

/** Run explicitly against an isolated Redis: mvn -Dtest=PresenceRedisIT test. */
class PresenceRedisIT {
    @Test void sessionsAppearOnlyAfterHeartbeatRemainIndependentAndExpire() throws Exception {
        var factory = new LettuceConnectionFactory("127.0.0.1", Integer.getInteger("test.redis.port", 16379));
        factory.afterPropertiesSet();
        var redis = new StringRedisTemplate(factory);
        var service = new PresenceService(redis, new ObjectMapper());
        long ownerId = Math.abs(UUID.randomUUID().getMostSignificantBits());
        var owner = User.builder().id(ownerId).username("owner").build();
        var a = UserPrincipal.builder().id("12").username("ownerchild").sessionId(UUID.randomUUID().toString()).build();
        var b = UserPrincipal.builder().id("12").username("ownerchild").sessionId(UUID.randomUUID().toString()).build();
        var laptop = HeartbeatRequest.builder().hostname("Laptop").os("Linux").build();
        var pc = HeartbeatRequest.builder().hostname("PC").os("Windows").build();
        try {
            service.openSession(12L, a.getSessionId());
            service.openSession(12L, b.getSessionId());
            assertTrue(service.getDevices(ownerId).isEmpty(), "Login alone must not create a device");
            assertTrue(service.markDeviceOnline(owner, a, null, laptop));
            assertTrue(service.markDeviceOnline(owner, b, null, pc));
            assertEquals(2, service.getDevices(ownerId).size());
            assertEquals("Laptop", service.getDevice(ownerId, a.getSessionId()).getDeviceName());
            assertEquals("PC", service.getDevice(ownerId, b.getSessionId()).getDeviceName());
            assertTrue(service.getDevices(ownerId + 1).isEmpty());
            assertNull(service.getDevice(ownerId + 1, a.getSessionId()));
            long ttl = redis.getExpire("presence:v2:" + ownerId + ":" + a.getSessionId());
            assertTrue(ttl > 0 && ttl <= 20);
            service.closeSession(ownerId, a.getSessionId());
            assertFalse(service.markDeviceOnline(owner, a, null, laptop), "Late heartbeat cannot undo logout");
            assertEquals(1, service.getDevices(ownerId).size());
            Thread.sleep(21000); // Exercise the real production TTL, not a mocked clock.
            assertTrue(service.getDevices(ownerId).isEmpty());
            assertTrue(service.isSessionActive("12", b.getSessionId()), "Presence expiry is not logout");
            assertTrue(service.markDeviceOnline(owner, b, null, pc));
            assertEquals(1, service.getDevices(ownerId).size());
        } finally {
            service.closeSession(ownerId, a.getSessionId());
            service.closeSession(ownerId, b.getSessionId());
            factory.destroy();
        }
    }
}
