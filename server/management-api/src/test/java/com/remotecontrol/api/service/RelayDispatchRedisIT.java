package com.remotecontrol.api.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.relay.*;
import org.junit.jupiter.api.*;
import org.springframework.data.redis.connection.lettuce.LettuceConnectionFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.web.server.ResponseStatusException;
import java.time.Duration;
import java.util.*;
import java.util.concurrent.*;
import static org.junit.jupiter.api.Assertions.*;

/** Run only against isolated Redis: -Dtest=RelayDispatchRedisIT -Dtest.redis.port=16389. */
class RelayDispatchRedisIT {
    private LettuceConnectionFactory factory;
    private StringRedisTemplate redis;
    private RelayDispatchService service;
    @BeforeEach void setup() {
        factory = new LettuceConnectionFactory("127.0.0.1", Integer.getInteger("test.redis.port", 16389));
        factory.afterPropertiesSet();
        redis = new StringRedisTemplate(factory);
        service = new RelayDispatchService(redis, new ObjectMapper());
    }
    @AfterEach void close() { factory.destroy(); }
    private String child() {
        String id = UUID.randomUUID().toString();
        redis.opsForValue().set("auth:child:" + id, "1", Duration.ofMinutes(1));
        return id;
    }
    @Test void atomicReservationsBalanceConcurrentAllocationsAndFenceRestartedNodes() throws Exception {
        String suffix = UUID.randomUUID().toString();
        String a = "a-" + suffix, b = "b-" + suffix;
        var one = new RelayNodeRegistrationRequest(a, "boot-a", "localhost", 9091, 10);
        var two = new RelayNodeRegistrationRequest(b, "boot-b", "localhost", 9092, 10);
        service.register(one);
        service.register(two);
        List<String> ids = new ArrayList<>();
        for (int i = 0; i < 20; i++) ids.add(child());
        var pool = Executors.newFixedThreadPool(8);
        try {
            var futures = new ArrayList<Future<RelayEndpointDto>>();
            for (String id : ids) futures.add(pool.submit(() -> service.allocate(id)));
            var endpoints = new ArrayList<RelayEndpointDto>();
            for (var future : futures) endpoints.add(future.get(10, TimeUnit.SECONDS));
            assertEquals(10, endpoints.stream().filter(e -> e.instanceId().equals(a)).count());
            assertEquals(10, endpoints.stream().filter(e -> e.instanceId().equals(b)).count());
            for (int i = 0; i < 20; i++) {
                assertEquals(endpoints.get(i).instanceId(), service.allocate(ids.get(i)).instanceId());
                assertEquals(endpoints.get(i).instanceId(), service.lookup(ids.get(i)).instanceId());
            }
            assertThrows(ResponseStatusException.class, () -> service.allocate(child()));
            assertThrows(ResponseStatusException.class, () -> service.heartbeat(a,
                    new RelayNodeHeartbeatRequest("stale-boot", 0, 0)));
            assertThrows(ResponseStatusException.class, () -> service.register(
                    new RelayNodeRegistrationRequest(a, "new-boot", "localhost", 9091, 10)));
            int index = 0;
            while (!endpoints.get(index).instanceId().equals(a)) index++;
            String assigned = ids.get(index);
            assertThrows(ResponseStatusException.class, () -> service.lookup(assigned, b, "boot-b"));
            redis.delete("relay:node:" + a); // Simulate expired liveness lease without sleeping.
            assertThrows(ResponseStatusException.class, () -> service.lookup(assigned));
            service.register(new RelayNodeRegistrationRequest(a, "new-boot", "localhost", 9091, 10));
            assertThrows(ResponseStatusException.class, () -> service.lookup(assigned));
            assertEquals(a, service.allocate(assigned).instanceId());
            assertNotNull(service.lookup(assigned, a, "new-boot"));
            redis.delete("auth:child:" + assigned);
            assertThrows(ResponseStatusException.class, () -> service.lookup(assigned));
            assertThrows(ResponseStatusException.class, () -> service.allocate(assigned));
        } finally {
            pool.shutdownNow();
            redis.delete(List.of("relay:node:" + a, "relay:node:" + b,
                    "relay:reservations:" + a + ":boot-a", "relay:reservations:" + a + ":new-boot",
                    "relay:reservations:" + b + ":boot-b"));
            redis.opsForZSet().remove("relay:nodes", a, b);
            for (String id : ids) redis.delete(List.of("auth:child:" + id, "relay:allocation:" + id));
        }
    }
}
