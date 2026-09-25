local t = redis.call('TIME')
local now = t[1] * 1000 + math.floor(t[2] / 1000)
local ttl = redis.call('PTTL', KEYS[3])
if ttl <= 0 then return nil end
local expires = now + math.min(ttl, tonumber(ARGV[2]))
local node = nil
local previous = redis.call('GET', KEYS[2])
if previous then
    local assignment = cjson.decode(previous)
    local raw = redis.call('GET', 'relay:node:' .. assignment.instanceId)
    if raw then
        local candidate = cjson.decode(raw)
        if candidate.bootId == assignment.bootId then node = candidate end
    end
end
if not node then
    redis.call('ZREMRANGEBYSCORE', KEYS[1], '-inf', now)
    local ids = redis.call('ZRANGE', KEYS[1], 0, -1)
    local best = math.huge
    for _, id in ipairs(ids) do
        local raw = redis.call('GET', 'relay:node:' .. id)
        if raw then
            local candidate = cjson.decode(raw)
            local reservations = 'relay:reservations:' .. id .. ':' .. candidate.bootId
            redis.call('ZREMRANGEBYSCORE', reservations, '-inf', now)
            local count = math.max(redis.call('ZCARD', reservations), candidate.registeredChildren or 0)
            local load = count / candidate.capacity
            if count < candidate.capacity and load < best then
                node = candidate
                best = load
            end
        end
    end
end
if not node then return nil end
local reservations = 'relay:reservations:' .. node.instanceId .. ':' .. node.bootId
redis.call('ZADD', reservations, expires, ARGV[1])
redis.call('PEXPIRE', reservations, ARGV[2])
local assignment = {instanceId=node.instanceId, bootId=node.bootId, expiresAt=expires}
redis.call('SET', KEYS[2], cjson.encode(assignment), 'PX', expires - now)
return cjson.encode({instanceId=node.instanceId, host=node.advertisedHost,
    port=node.advertisedPort, expiresAt=math.min(expires, node.expiresAt)})
