if redis.call('EXISTS', KEYS[2]) == 0 then return nil end
local raw = redis.call('GET', KEYS[1])
if not raw then return nil end
local assignment = cjson.decode(raw)
local nodeRaw = redis.call('GET', 'relay:node:' .. assignment.instanceId)
if not nodeRaw then return nil end
local node = cjson.decode(nodeRaw)
if node.bootId ~= assignment.bootId then return nil end
if ARGV[1] ~= '' and (node.instanceId ~= ARGV[1] or node.bootId ~= ARGV[2]) then return nil end
return cjson.encode({instanceId=node.instanceId, host=node.advertisedHost,
    port=node.advertisedPort, expiresAt=math.min(assignment.expiresAt, node.expiresAt)})
