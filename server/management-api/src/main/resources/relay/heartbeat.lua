local raw = redis.call('GET', KEYS[1])
if not raw then return 0 end
local node = cjson.decode(raw)
local heartbeat = cjson.decode(ARGV[1])
if node.bootId ~= heartbeat.bootId then return 0 end
local t = redis.call('TIME')
local now = t[1] * 1000 + math.floor(t[2] / 1000)
node.expiresAt = now + tonumber(ARGV[2])
node.registeredChildren = heartbeat.registeredChildren
node.activeSessions = heartbeat.activeSessions
redis.call('SET', KEYS[1], cjson.encode(node), 'PX', ARGV[2])
redis.call('ZADD', KEYS[2], node.expiresAt, node.instanceId)
return 1
