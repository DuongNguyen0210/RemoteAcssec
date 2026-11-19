package com.remotecontrol.api.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.child.*;
import com.remotecontrol.api.dto.common.UserPrincipal;
import com.remotecontrol.api.entity.*;
import com.remotecontrol.api.repository.ChildRepository;
import org.junit.jupiter.api.Test;
import org.springframework.data.redis.connection.lettuce.LettuceConnectionFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import java.util.*;
import java.util.concurrent.*;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.Mockito.*;

/** Explicit test against a private Redis, never a production instance. */
class TelemetryRedisIT {
    private HeartbeatRequest heartbeat(long sequence, double cpu) {
        return HeartbeatRequest.builder().os("Windows").hostname("Telemetry fixture")
                .telemetry(TelemetrySample.builder().sequence(sequence).cpuPercent(cpu)
                        .memoryUsedBytes(100L).memoryTotalBytes(1000L)
                        .networkRxBytesPerSecond(1024.0).networkTxBytesPerSecond(512.0).build()).build();
    }
    @Test void latestTelemetryIsAtomicOwnedEphemeralAndCannotReviveAnOldSession() throws Exception {
        var factory = new LettuceConnectionFactory("127.0.0.1", Integer.getInteger("test.redis.port", 19593));
        factory.afterPropertiesSet();
        var redis = new StringRedisTemplate(factory);
        var leases = new LoginLeaseService(redis);
        var repo = mock(ChildRepository.class);
        var child = Child.builder().id(42L).password("private-fixture-credential").build();
        when(repo.findById(42L)).thenReturn(Optional.of(child));
        var service = new PresenceService(redis, new ObjectMapper(), repo, leases);
        long ownerId = UUID.randomUUID().getMostSignificantBits() & Long.MAX_VALUE;
        var owner = User.builder().id(ownerId).build();
        String sid = UUID.randomUUID().toString(), next = UUID.randomUUID().toString();
        var principal = UserPrincipal.builder().id("42").username("child").sessionId(sid).build();
        var pool = Executors.newFixedThreadPool(8);
        try {
            assertTrue(leases.claim("CHILD", "42", sid, child.getPassword()));
            service.openSession(42L, sid, child.getPassword());
            assertTrue(service.markDeviceOnline(owner, principal, null, heartbeat(2,20)));
            var initial = service.getDevice(ownerId, sid);
            var first = initial.getTelemetry();
            assertEquals(2, first.sequence());
            Thread.sleep(30);
            assertTrue(service.markDeviceOnline(owner, principal, null, heartbeat(1,99)));
            assertTrue(service.markDeviceOnline(owner, principal, null, heartbeat(2,99)));
            var unchanged = service.getDevice(ownerId, sid);
            assertEquals(first, unchanged.getTelemetry(), "Late/duplicate samples cannot replace values or freshness timestamp");
            assertTrue(unchanged.getTelemetryAgeMs() >= 20);
            assertEquals(initial.getLastHeartbeatAt(),unchanged.getLastHeartbeatAt(),"Replay cannot refresh presence freshness");
            List<Future<Boolean>> jobs = new ArrayList<>();
            for (long sequence=3; sequence<=34; ++sequence) {
                final long value=sequence;
                jobs.add(pool.submit(() -> service.markDeviceOnline(owner, principal, null, heartbeat(value,value))));
            }
            for (var job : jobs) assertTrue(job.get(10, TimeUnit.SECONDS));
            var latest = service.getDevice(ownerId,sid);
            assertEquals(34,latest.getTelemetry().sequence());
            assertEquals(34.0,latest.getTelemetry().cpuPercent());
            assertEquals(1,service.getDevices(ownerId).size());
            assertTrue(service.getDevices(ownerId+1).isEmpty());
            assertNull(service.getDevice(ownerId+1,sid));
            assertTrue(redis.getExpire("presence:"+ownerId+":"+sid) <= PresenceService.TTL_SECONDS);
            service.hideDevice(ownerId,sid);
            assertNull(service.getDevice(ownerId,sid));
            assertTrue(service.markDeviceOnline(owner,principal,null,heartbeat(35,50)));
            assertEquals(35,service.getDevice(ownerId,sid).getTelemetry().sequence());
            var exact=heartbeat(36,10);
            exact.getTelemetry().setMemoryTotalBytes(9007199254740991L);
            exact.getTelemetry().setMemoryUsedBytes(9007199254740990L);
            assertTrue(service.markDeviceOnline(owner,principal,null,exact));
            assertEquals(9007199254740991L,service.getDevice(ownerId,sid).getTelemetry().memoryTotalBytes());
            assertEquals(9007199254740990L,service.getDevice(ownerId,sid).getTelemetry().memoryUsedBytes());
            assertTrue(service.markDeviceOnline(owner,principal,null,HeartbeatRequest.builder().os("Legacy").build()));
            assertNull(service.getDevice(ownerId,sid).getTelemetry(), "Old clients must show unavailable, never an old reading");
            service.closeSession(ownerId,sid);
            assertFalse(service.markDeviceOnline(owner,principal,null,heartbeat(100,99)));
            assertTrue(leases.claim("CHILD","42",next,child.getPassword()));
            service.openSession(42L,next,child.getPassword());
            var replacement=UserPrincipal.builder().id("42").username("child").sessionId(next).build();
            assertTrue(service.markDeviceOnline(owner,replacement,null,heartbeat(1,10)));
            assertEquals(1,service.getDevice(ownerId,next).getTelemetry().sequence());
            assertFalse(service.markDeviceOnline(owner,principal,null,heartbeat(101,99)));
            service.closeSession(ownerId,next);
            assertTrue(service.getDevices(ownerId).isEmpty());
        } finally {
            service.closeSession(ownerId,sid);service.closeSession(ownerId,next);
            pool.shutdownNow();factory.destroy();
        }
    }
}
