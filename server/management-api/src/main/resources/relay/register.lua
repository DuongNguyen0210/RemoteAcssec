local t = redis.call('TIME')
local now = t[1] * 1000 + math.floor(t[2] / 1000)
local node = cjson.decode(ARGV[1])
local key = 'relay:node:' .. node.instanceId
local previous = redis.call('GET', key)
if previous then
    local old = cjson.decode(previous)
    if old.bootId ~= node.bootId then return 0 end
    if old.advertisedHost ~= node.advertisedHost or old.advertisedPort ~= node.advertisedPort
        or old.capacity ~= node.capacity then return 0 end
    node.registeredChildren = old.registeredChildren
    node.activeSessions = old.activeSessions
else
    node.registeredChildren = 0
    node.activeSessions = 0
end
node.expiresAt = now + tonumber(ARGV[2])
redis.call('SET', key, cjson.encode(node), 'PX', ARGV[2])
redis.call('ZADD', KEYS[1], node.expiresAt, node.instanceId)
return 1
