package com.remotecontrol.api.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.child.HeartbeatRequest;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.User;
import org.junit.jupiter.api.Test;
import org.springframework.data.redis.connection.lettuce.LettuceConnectionFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;
import java.util.UUID;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.Mockito.*;

/** Run explicitly against an isolated Redis: mvn -Dtest=PresenceRedisIT test. */
class PresenceRedisIT {
    @Test void sessionsAppearOnlyAfterHeartbeatRemainIndependentAndExpire() throws Exception {
        var factory = new LettuceConnectionFactory("127.0.0.1", Integer.getInteger("test.redis.port", 16379));
        factory.afterPropertiesSet();
        var redis = new StringRedisTemplate(factory);
        var childRepo = mock(com.remotecontrol.api.repository.ChildRepository.class);
        var child = com.remotecontrol.api.entity.Child.builder().id(12L).username("ownerchild")
                .password(com.remotecontrol.api.util.ChildPasswords.encode("test-password")).build();
        when(childRepo.findById(12L)).thenReturn(java.util.Optional.of(child));
        var service = new PresenceService(redis, new ObjectMapper(), childRepo);
        long ownerId = Math.abs(UUID.randomUUID().getMostSignificantBits());
        var owner = User.builder().id(ownerId).username("owner").build();
        var a = UserPrincipal.builder().id("12").username("ownerchild").sessionId(UUID.randomUUID().toString()).build();
        var b = UserPrincipal.builder().id("12").username("ownerchild").sessionId(UUID.randomUUID().toString()).build();
        var laptop = HeartbeatRequest.builder().hostname("Laptop").os("Linux").build();
        var pc = HeartbeatRequest.builder().hostname("PC").os("Windows").build();
        try {
            service.openSession(12L, a.getSessionId(), child.getPassword());
            service.openSession(12L, b.getSessionId(), child.getPassword());
            assertTrue(service.getDevices(ownerId).isEmpty(), "Login alone must not create a device");
            assertTrue(service.markDeviceOnline(owner, a, null, laptop));
            assertTrue(service.markDeviceOnline(owner, b, null, pc));
            assertEquals(2, service.getDevices(ownerId).size());
            assertEquals("Laptop", service.getDevice(ownerId, a.getSessionId()).getDeviceName());
            assertEquals("PC", service.getDevice(ownerId, b.getSessionId()).getDeviceName());
            assertTrue(service.getDevices(ownerId + 1).isEmpty());
            assertNull(service.getDevice(ownerId + 1, a.getSessionId()));
            long ttl = redis.getExpire("presence:" + ownerId + ":" + a.getSessionId());
            assertTrue(ttl > 0 && ttl <= 20);
            service.closeSession(ownerId, a.getSessionId());
            assertFalse(service.markDeviceOnline(owner, a, null, laptop), "Late heartbeat cannot undo logout");
            assertEquals(1, service.getDevices(ownerId).size());
            var devices = mock(DeviceService.class);
            when(devices.getDevices(any())).thenAnswer(call -> service.getDevices(ownerId));
            var notifier = new SseNotifier(new ObjectMapper());
            var emitter = mock(SseEmitter.class);
            notifier.register(ownerId, emitter);
            Thread.sleep(21000); // Exercise the real production TTL, not a mocked clock.
            assertTrue(service.getDevices(ownerId).isEmpty());
            try (var publisher = new DevicePresencePublisher(devices, notifier)) {
                publisher.publish();
                verify(emitter, timeout(2000)).send(any(SseEmitter.SseEventBuilder.class));
            }
            var event = org.mockito.ArgumentCaptor.forClass(SseEmitter.SseEventBuilder.class);
            verify(emitter).send(event.capture());
            assertTrue(event.getValue().build().stream().anyMatch(part -> "[]".equals(part.getData())));
            assertTrue(service.isSessionActive("12", b.getSessionId()), "Presence expiry is not logout");
            assertTrue(service.markDeviceOnline(owner, b, null, pc));
            assertEquals(1, service.getDevices(ownerId).size());
        } finally {
            service.closeSession(ownerId, a.getSessionId());
            service.closeSession(ownerId, b.getSessionId());
            factory.destroy();
        }
    }

    @Test void passwordResetInvalidatesSessionInRealRedisAndHeartbeatRejectsIt() throws Exception {
        var factory = new LettuceConnectionFactory("127.0.0.1", Integer.getInteger("test.redis.port", 16379));
        factory.afterPropertiesSet();
        var redis = new StringRedisTemplate(factory);
        var childRepo = mock(com.remotecontrol.api.repository.ChildRepository.class);
        var child = com.remotecontrol.api.entity.Child.builder()
                .id(12L).username("ownerchild")
                .password(com.remotecontrol.api.util.ChildPasswords.encode("old-password")).build();
        when(childRepo.findById(12L)).thenReturn(java.util.Optional.of(child));

        var service = new PresenceService(redis, new ObjectMapper(), childRepo);
        long ownerId = Math.abs(UUID.randomUUID().getMostSignificantBits());
        var owner = User.builder().id(ownerId).username("owner").build();
        var session = UserPrincipal.builder().id("12").username("ownerchild").sessionId(UUID.randomUUID().toString()).build();
        var laptop = HeartbeatRequest.builder().hostname("Laptop").os("Linux").build();

        try {
            // Login binds session to old password fingerprint
            service.openSession(12L, session.getSessionId(), child.getPassword());
            assertTrue(service.isSessionActive("12", session.getSessionId()));
            assertTrue(service.markDeviceOnline(owner, session, null, laptop));
            assertEquals(1, service.getDevices(ownerId).size());

            // Admin resets child password
            String oldPass = child.getPassword();
            child.setPassword(com.remotecontrol.api.util.ChildPasswords.encode("new-password"));

            // Verification 1: Heartbeat from old session BEFORE isSessionActive is rejected atomically by Redis Lua script
            // (Proves Lua script itself blocks binding mismatch even while old key is still in Redis)
            assertFalse(service.markDeviceOnline(owner, session, null, laptop),
                    "Atomic heartbeat must reject old session after password reset even before isSessionActive purges it");

            // Verification 2: Session is immediately inactive
            assertFalse(service.isSessionActive("12", session.getSessionId()),
                    "Session must fail isSessionActive check immediately after password reset");

            // Verification 3: Stale key is purged from Redis
            assertNull(redis.opsForValue().get("auth:child:" + session.getSessionId()),
                    "Stale session key must be purged from Redis");

            // Verification 4: Stale login snapshot (openSession with old credential after reset) must fail isActive check
            var staleSnapshotSession = UserPrincipal.builder().id("12").username("ownerchild").sessionId(UUID.randomUUID().toString()).build();
            service.openSession(12L, staleSnapshotSession.getSessionId(), oldPass);
            assertFalse(service.isSessionActive("12", staleSnapshotSession.getSessionId()),
                    "Stale login snapshot must fail isSessionActive check against new DB password");
            assertNull(redis.opsForValue().get("auth:child:" + staleSnapshotSession.getSessionId()),
                    "Stale snapshot key must be purged from Redis");

            // Verification 5: New login with new password establishes working session
            var newSession = UserPrincipal.builder().id("12").username("ownerchild").sessionId(UUID.randomUUID().toString()).build();
            service.openSession(12L, newSession.getSessionId(), child.getPassword());
            assertTrue(service.isSessionActive("12", newSession.getSessionId()));
            assertTrue(service.markDeviceOnline(owner, newSession, null, laptop));
            service.closeSession(ownerId, newSession.getSessionId());
        } finally {
            service.closeSession(ownerId, session.getSessionId());
            factory.destroy();
        }
    }

    @Test void hideDeviceRemovesPresenceImmediatelyWhileSessionRemainsActiveAndHeartbeatRestores() throws Exception {
        var factory = new LettuceConnectionFactory("127.0.0.1", Integer.getInteger("test.redis.port", 16379));
        factory.afterPropertiesSet();
        var redis = new StringRedisTemplate(factory);
        var childRepo = mock(com.remotecontrol.api.repository.ChildRepository.class);
        var child = com.remotecontrol.api.entity.Child.builder()
                .id(12L).username("ownerchild")
                .password(com.remotecontrol.api.util.ChildPasswords.encode("test-password")).build();
        when(childRepo.findById(12L)).thenReturn(java.util.Optional.of(child));

        var service = new PresenceService(redis, new ObjectMapper(), childRepo);
        long ownerId = Math.abs(UUID.randomUUID().getMostSignificantBits());
        var owner = User.builder().id(ownerId).username("owner").build();
        var session = UserPrincipal.builder().id("12").username("ownerchild").sessionId(UUID.randomUUID().toString()).build();
        var laptop = HeartbeatRequest.builder().hostname("Laptop").os("Linux").build();

        try {
            service.openSession(12L, session.getSessionId(), child.getPassword());
            assertTrue(service.isSessionActive("12", session.getSessionId()));
            assertTrue(service.markDeviceOnline(owner, session, null, laptop));
            assertEquals(1, service.getDevices(ownerId).size());
            assertNotNull(service.getDevice(ownerId, session.getSessionId()));

            // Act: hideDevice (called when child clicks "Dừng chia sẻ" / unshare)
            service.hideDevice(ownerId, session.getSessionId());

            // Verification 1: presence key is removed immediately from Redis
            assertNull(service.getDevice(ownerId, session.getSessionId()),
                    "Device presence must be removed immediately after hideDevice");
            assertTrue(service.getDevices(ownerId).isEmpty(),
                    "Owner's device list must be empty after hideDevice");
            assertNull(redis.opsForValue().get("presence:" + ownerId + ":" + session.getSessionId()),
                    "presence Redis key must be deleted");

            // Verification 2: session auth key remains active (no re-login needed)
            assertTrue(service.isSessionActive("12", session.getSessionId()),
                    "auth:child session must remain active after unshare");
            assertNotNull(redis.opsForValue().get("auth:child:" + session.getSessionId()),
                    "auth:child Redis key must NOT be deleted");

            // Verification 3: re-enabling sharing (next heartbeat) restores presence without re-login
            assertTrue(service.markDeviceOnline(owner, session, null, laptop),
                    "Subsequent heartbeat must succeed and restore presence");
            assertEquals(1, service.getDevices(ownerId).size());
            assertEquals("Laptop", service.getDevice(ownerId, session.getSessionId()).getDeviceName());
        } finally {
            service.closeSession(ownerId, session.getSessionId());
            factory.destroy();
        }
    }
}
