package com.remotecontrol.api.service;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.remotecontrol.api.dto.relay.*;
import lombok.RequiredArgsConstructor;
import org.springframework.core.io.ClassPathResource;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.data.redis.core.script.DefaultRedisScript;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Service;
import org.springframework.web.server.ResponseStatusException;
import java.util.List;

/** Atomic dispatch for standalone Redis: live-node leases plus renewable child reservations. */
@Service
@RequiredArgsConstructor
public class RelayDispatchService {
    public static final long NODE_TTL_MS = 30000;
    public static final long ALLOCATION_TTL_MS = 45000;
    private static final String NODES = "relay:nodes";
    private static final DefaultRedisScript<Long> REGISTER = script("register", Long.class);
    private static final DefaultRedisScript<Long> HEARTBEAT = script("heartbeat", Long.class);
    private static final DefaultRedisScript<String> ALLOCATE = script("allocate", String.class);
    private static final DefaultRedisScript<String> LOOKUP = script("lookup", String.class);
    private final StringRedisTemplate redis;
    private final ObjectMapper mapper;

    public void register(RelayNodeRegistrationRequest request) {
        Long result = redis.execute(REGISTER, List.of(NODES), json(request), Long.toString(NODE_TTL_MS));
        if (!Long.valueOf(1).equals(result))
            throw new ResponseStatusException(HttpStatus.CONFLICT, "Instance already leased by another boot");
    }

    public void heartbeat(String instanceId, RelayNodeHeartbeatRequest request) {
        Long result = redis.execute(HEARTBEAT, List.of("relay:node:" + instanceId, NODES),
                json(request), Long.toString(NODE_TTL_MS));
        if (!Long.valueOf(1).equals(result))
            throw new ResponseStatusException(HttpStatus.CONFLICT, "Node must register again");
    }

    public RelayEndpointDto allocate(String agentSessionId) {
        String result = redis.execute(ALLOCATE,
                List.of(NODES, allocationKey(agentSessionId), "auth:child:" + agentSessionId),
                agentSessionId, Long.toString(ALLOCATION_TTL_MS));
        if (result == null) throw new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "No relay available");
        return endpoint(result);
    }

    public RelayEndpointDto lookup(String agentSessionId) { return lookup(agentSessionId, "", ""); }

    public RelayEndpointDto lookup(String agentSessionId, String instanceId, String bootId) {
        String result = redis.execute(LOOKUP,
                List.of(allocationKey(agentSessionId), "auth:child:" + agentSessionId), instanceId, bootId);
        if (result == null) throw new ResponseStatusException(HttpStatus.NOT_FOUND, "No live relay assignment");
        return endpoint(result);
    }

    private static String allocationKey(String id) { return "relay:allocation:" + id; }
    private String json(Object value) {
        try { return mapper.writeValueAsString(value); }
        catch (JsonProcessingException e) { throw new IllegalStateException("Cannot encode relay state", e); }
    }
    private RelayEndpointDto endpoint(String json) {
        try { return mapper.readValue(json, RelayEndpointDto.class); }
        catch (JsonProcessingException e) { throw new IllegalStateException("Invalid relay state", e); }
    }
    private static <T> DefaultRedisScript<T> script(String name, Class<T> type) {
        var script = new DefaultRedisScript<T>();
        script.setLocation(new ClassPathResource("relay/" + name + ".lua"));
        script.setResultType(type);
        return script;
    }
}
