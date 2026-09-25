# Relay load balancing

## Run the stack

From the repository root:

```sh
cp infrastructure/.env.example infrastructure/.env
# Set RELAY_NODE_TOKEN to a random shared service token.
# Set RELAY_ADVERTISED_HOST to an IP/DNS name reachable from BOTH clients.
docker compose -f infrastructure/docker-compose.yml up --build -d
```

Management HTTP is available through Nginx on port 80 (override `MANAGEMENT_PUBLIC_PORT`).
The two public TCP endpoints are `RELAY_ADVERTISED_HOST:9091` and `:9092`.
`localhost` works only when clients run on the Docker host. PostgreSQL and Redis have
persistent volumes and no published ports. `/internal/` is blocked at Nginx; node
registration also requires `X-Relay-Token`. The API listens on internal port 9090.

Run both Qt clients with:

```sh
REMOTE_API_URL=http://localhost REMOTE_RELAY_MODE=dynamic ./clients/build/RemoteAccessApp/RemoteAccessApp
```

Local teammate mode remains the default: `REMOTE_RELAY_MODE=static`, host
`localhost`, port `8080`. Override with `REMOTE_RELAY_HOST` and `REMOTE_RELAY_PORT`.
A standalone Relay defaults to discovery disabled and uses Management API
`http://localhost:9090` for authentication. Static mode is intended for one local
Relay; the Docker relays require assigned endpoints.

## Routing and leases

1. Each process generates a new `bootId`, binds TCP, then registers its instance,
   advertised endpoint and capacity with `POST /internal/relay/nodes`.
2. `POST /internal/relay/nodes/{instanceId}/heartbeat` renews a 30-second node
   lease every 5 seconds and reports registered children / active sessions.
   A different boot cannot overwrite a live lease; a restart can take up to
   30 seconds to become allocatable. Old heartbeats cannot revive another boot.
3. CHILD calls `POST /api/v1/relay/allocations`. One Redis Lua transaction picks
   the least occupied node (max of live reservations and reported children,
   divided by capacity), reserves a slot and records session-to-node affinity.
   Existing assignments remain sticky while their node and boot are live.
4. `ChildSessionController` renews allocation every 10 seconds. Reservations
   expire after 45 seconds, capped by the login session TTL. The response's
   `expiresAt` is epoch milliseconds, capped by the node lease. Allocation
   fails with 503 when no live node has capacity.
5. ADMIN calls `GET /api/v1/devices/{agentSessionId}/relay`; ownership, database
   account existence and live presence are checked before lookup. Missing or
   expired assignments return 404. ADMIN connects directly to that endpoint.
6. REGISTER_HOST / CONNECT_REQUEST carry the existing version-2 bearer payload.
   Discovery-enabled relays also pass their instance/boot to authorization, so
   connections to the wrong node are denied. Existing connections are checked
   every 10 seconds for revoked login, ownership, presence and assignment.
7. Only an ACTIVE session's two channels may forward: SCREEN_FRAME Child →
   Admin; MOUSE_MOVE / BUTTON_DOWN / BUTTON_UP / WHEEL and KEY_PRESS / RELEASE
   Admin → Child. Header and payload bytes remain unchanged. A congested peer
   is disconnected instead of growing memory indefinitely.

Nginx only handles HTTP. TCP round-robin cannot pair clients when sessions are
process-local. On node failure, the child reconnects and obtains another endpoint
after liveness expiry. The admin must reconnect; in-flight sessions are not migrated.
Redis scripts target the standalone Redis deployed here, not Redis Cluster.

## Client integration

`RelayEndpointProvider` is asynchronous and offers `allocate` / `lookup`.
`DynamicRelayEndpointProvider` uses DeviceService with bounded HTTP timeouts;
`StaticRelayEndpointProvider` returns a configured endpoint. Child session code
owns TCP, REGISTER/ACK, SESSION_REQUEST/ACCEPT/REJECT, reconnect and lease renewal.
It emits `sessionStarted`, `sessionEnded`, `inputReceived` and `sessionFailed`.
`sendScreenPacket` validates session/type/header before sending; `pendingBytes`
allows a producer to preserve backpressure.

The additional `ScreenStreamSender(ChildSessionController*, QObject*)` constructor
connects the existing timer to session signals. AppCoordinator starts/stops the
session controller; it does not invoke the legacy sender's `start()` path. The
existing capture/encode/packetize/onTick code is unchanged and uses the controller's
transport via a compatibility accessor. The original constructor remains available.
AdminSessionController accepts an explicit host/port, exposes `sendInput`,
`screenReceived`, `endSession`, and has a bounded session-setup timeout.

## Verification

```sh
mvn -f server/pom.xml test
cmake --build clients/build -j4
QT_QPA_PLATFORM=offscreen ctest --test-dir clients/build --output-on-failure
# Against a separate disposable Redis on localhost:16389:
mvn -f server/pom.xml -pl management-api -Dtest=RelayDispatchRedisIT test
# Against a disposable Compose stack with MANAGEMENT_PUBLIC_PORT=18089:
TEST_API_URL=http://localhost:18089 python3 server/tests/relay_load_balancing.py
```

The repository's pre-existing `DeviceServiceTest.getDevices_ThrowsUnauthorized_WhenUserNotFound`
fails on the original HEAD too: the implementation returns an empty list instead
of throwing. This change does not alter that unrelated behavior or weaken its test.

Verified during implementation: both Docker images built; PostgreSQL/Redis/API
health checks passed; Nginx configuration passed `nginx -t` and denied `/internal/`;
`relay_load_balancing.py` passed against the complete two-node stack. Relay Java
unit tests, API authorization tests, Redis dispatch integration and both Qt test
executables passed. Disposable test containers and volumes were removed afterward.
