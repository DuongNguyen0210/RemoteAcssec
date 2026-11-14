# PBL4 remote control improvement contract

## Objective and boundaries
Keep the existing Qt Widgets/QSS palette and Spring/Netty architecture. Improve the working remote desktop core, remove misleading demo content, and document missing capabilities. Avoid enterprise features and new infrastructure. Never delete user data; database changes must be additive and tested against an isolated database.

## Capability map and order
| Module | Responsibility | Dependencies |
|---|---|---|
| realtime | Reliable bounded presence delivery and reconnect | Existing REST/Redis |
| streaming | Session ownership, flow control, remove duplicate legacy sender | Existing RDTP |
| desktop-ux | Accurate state, useful empty/error states, preserve QSS | realtime, streaming |
| persistence | Validate schema/contracts and migration need | Existing JPA/PostgreSQL |

## Acceptance and tasks
- [x] Audit core/missing/out-of-scope functions with concrete source evidence.
- [x] Realtime client and server: SSE uses the configured API URL, safe cancellation, capped retry jitter, an idle watchdog and bounded input. Server coalesces presence updates and periodically reconciles expiry. Proxy configuration is being reconciled with the teammate's deployment work.
- [x] Streaming: bounded send queues, relay input limits and single ownership of CHILD relay registration. CHILD enables unattended sharing explicitly and can stop it locally.
- [x] Desktop UX: remove demo records and misleading controls, expose connection state and preserve the existing palette.
- [x] Persistence: run isolated Redis and PostgreSQL checks, introduce only evidenced schema changes.
- [x] Build/test both stacks and record limits and remaining feature gaps.

## Current feature audit
| Core capability | Current evidence | Remaining gap |
|---|---|---|
| Screen and control | Qt captures/encodes JPEG, relay forwards RDTP, ADMIN viewer decodes and sends input; CHILD injects on Windows/X11 (`ScreenStreamSender`, `AdminSessionController`, `RemoteInputInjector`). CHILD now maps frame pixels to Qt desktop coordinates for high-DPI screens; the local 100%/200% mapping test passes. | Verify real two-machine input at both scales, FPS and Wayland behavior |
| Relay and dispatch | Redis Lua allocates by reserved/registered load; real two-node API/relay integration and failure reallocation passed (`allocate.lua`, `server/tests/relay_load_balancing.py`, `relay_failover.py`) | Active sessions end on relay loss; multi-host recovery and user reconnect need field testing |
| Transport and overload | SSE bounded/retried, server presence coalesced, screen send skips frames under backpressure, relay has write watermarks, CHILD retries back off; TLS can be required (`SseClient`, `DevicePresencePublisher`, `ScreenStreamSender`, `RelayServer`) | Enable TLS in deployment; measure full Qt capture/network load across two hosts |
| User control | CHILD explicitly enables unattended sharing and can stop locally; ADMIN can end the viewer session (`AppCoordinator`, `RemoteDesktopWindow`) | Test visibility and stop behavior on actual desktops |
| Persistence | Isolated PostgreSQL migration and Redis TTL tests passed; `child.user_id` index added | No further schema change is evidenced |

Chat, file transfer, clipboard sync, AI features and Kubernetes are outside the core PBL4 remote-control goal. Disabled demo actions and unused relay registration code were removed; the remaining account/settings pages support administering and connecting devices.

## Commands and conventions
- Server: `mvn -f server/pom.xml test`
- Client: `cmake -S clients -B /tmp/remoteaccess-build -DBUILD_TESTING=ON`, `cmake --build /tmp/remoteaccess-build -j 4`, `ctest --test-dir /tmp/remoteaccess-build --output-on-failure`
- JUnit/Mockito tests in server modules; QtTest in clients/RemoteAccessApp/tests. Add regression tests before behavior changes.
- Keep Qt signal/slot boundary between GUI/controllers/network; Java constructor injection; small classes with one responsibility.
- Example: `if (m_loading) return;` prevents overlapping requests; do not introduce global polling.

## Initial evidence before fixes
PresenceService scans Redis and serializes owner snapshots on each heartbeat. TTL expiration has no notification. SseClient ignores saved API URL and can dereference a null reply during abort's synchronous finished signal. Nginx buffers SSE and has a 15-second read timeout. Logs/Sessions include demo data. RemoteScreenWidget only renders frames; actual mouse/keyboard capture and injection are absent.

## Git reconciliation (2026-11-13)
Fetched origin. Local feature head 1679900 includes settings and test fixes beyond origin/main 5939507. Teammate integration head 734ab5f includes ee8c42a (screen viewer) and bc167fa (graceful disconnect). Combined both on codex/pbl4-reliability-ux, resolving MainWindow conflicts by retaining injected SettingsPage and live SessionsPage. No push or shared-branch rewrite.
Initial server suite: 15 management tests, one existing failure: nonexistent owner returns empty devices instead of 401. Relay integration: 26 tests passed. Initial merged Qt run exposed absent startup refresh, corrected in AppCoordinator.
Database read-only inspection: child.user_id has FK but no lookup index; users/child username uniqueness exists. No database mutation performed.

## Verified increments
- Integrated teammate branch, preserving local Settings; all seven Qt test executables pass (startup regression re-run after fix); 26 relay tests pass.
- Restored 401 for deleted owners instead of misleading empty device list. Full Maven reactor passes (17 management + 26 relay tests).
- Added regression for decode progress with multiple queued frames: reproduced failure (only newest delivered), then removed newest-arrival equality guard. One decoding task plus one latest pending JPEG remains bounded; session generation still rejects images from ended sessions.

## Outstanding work now
The code and local integration tests do not prove actual two-computer interaction. Test screen/input on Windows and X11 with different DPI, record Qt capture FPS and end-to-end latency, and verify recovery after a relay fails on a separate host. Enable TLS in any untrusted network deployment. Finish and reconcile the teammate's uncommitted deployment configuration before claiming a deployment-ready stack. The isolated validation PostgreSQL and Redis containers still require removal when Docker permits it.

### SSE client recovery
Uses ApiClient's configured base URL (regression reproduced with local HTTP server before fix). Retries transient HTTP failures with exponential 3–30 second delay plus 0–999 ms jitter; stops on 401/403, including empty bodies. A 45-second transfer inactivity timeout recovers silent streams. Network read buffer and incomplete event buffer are bounded to 1 MiB; overflow disconnects and retries. Complete SSE blocks reset backoff. Verified local HTTP tests for configured address, synchronous cancellation, 503 reconnect followed by empty 401, and oversized incomplete event; account/device flow also passes. The server now coalesces heartbeat-driven snapshots per owner and periodically reconciles expiry with bounded workers and connections (`DevicePresencePublisher`, `SseNotifier`).

### Database check (2026-11-13)
The live PostgreSQL database had 1 users row and 2 child rows. `child.user_id` lacked an index, while `ChildRepository.findByOwner` is used by account listing and periodic presence snapshots. Added `idx_child_user_id` in the JPA mapping and an idempotent SQL migration. Tested the migration twice in a disposable PostgreSQL 16 container, then applied it to the live local `remote_db`; row counts remained 1 and 2. No table data was changed. Redis integration tests were not run in this increment; the existing 20-second presence TTL still requires an isolated Redis run for final verification.

### Redis integration and streaming cleanup
Ran `PresenceRedisIT` against a disposable Redis 7 container on port 16379. The first run exposed a stale test assertion using `presence:v2:` while the current implementation writes `presence:`. Corrected the assertion; the integration test then passed, including the real 20-second TTL expiry, independent sessions, and logout behavior. Removed the unused `ScreenStreamSender` relay registration/session state path (187 lines); `ChildSessionController` remains the single owner of that state. Qt build and all 7 CTest executables pass after the removal.

### Desktop navigation cleanup
The sidebar's “Kết nối mới” action now opens Devices, where an actual device can be selected. Removed disabled notification/help/profile icons from the top bar; they had no backing behavior. QSS was not modified. Built the Qt client and ran the account/device and screen viewer test executables after the edit.

The Sessions page no longer shows three metric cards with no data or a disabled “Xem màn hình” action. It keeps the real viewer, current-session indicator and working disconnect control. With no active session it now prompts the user to choose a device instead of implying that a screen stream is pending. The existing QSS palette was not changed; the screen viewer test passes after the cleanup.

The top bar's previously disabled search field now filters the locally held online device list by machine name, account, OS or IP. Typing from another page opens Devices once, so subsequent keystrokes do not trigger repeated API refreshes. A no-match state can clear the query. No QSS color values were changed. The navigation test checks filtering, the one-time page switch and clearing from the no-match state.

### Remaining acceptance evidence
A full two-machine demonstration is still outstanding: real input injection and DPI mapping, measured Qt capture/stream quality, and a failure/reconnect demonstration with relay nodes on separate hosts. Local two-relay allocation, reassignment and synthetic transport benchmarks are recorded below. Unattended access is explicitly enabled on CHILD, and relay TLS must be configured before using an untrusted network. Unit and single-host integration tests do not prove multi-host desktop behavior.

### Two-relay integration (2026-11-13)
Built an isolated Compose stack with two API instances, two relays, disposable PostgreSQL/Redis and separate loopback ports (18089, 19091, 19092). `server/tests/relay_load_balancing.py` passed: two CHILD sessions allocated to different relays, affinity/lookup and wrong-node authorization held, and screen/input packets crossed the active relay. `server/tests/relay_failover.py` passed after isolating the assigned relay from the Compose network: its Redis lease expired and a subsequent allocation returned the other relay. Docker `stop` was denied by the local daemon, and process `SIGSTOP` did not pause its heartbeat here; network disconnection was the verified failure injection. This does not prove seamless migration of an active remote-control session. Build initially hit a truncated Maven Central download and succeeded on retry. Cleanup was attempted after testing, but this Docker daemon returned `permission denied` for stop/kill/remove on the validation containers. The five application containers were disconnected from the validation network; PostgreSQL and Redis remain connected to each other. `pbl4-validation-*` containers and two volumes still need removal when Docker permissions recover. The temporary environment file was deleted.

### CHILD session visibility and local stop
CHILD now opens a small status window after login with sharing initially off. The person at the CHILD machine explicitly selects “Bật chia sẻ” to start the heartbeat and relay; while enabled, valid sessions are accepted automatically as unattended access, per the owner's decision. The window reports an active session, relay failure, and expired authentication. “Dừng chia sẻ” stops both connections, and closing the window exits. This preserves the main app QSS palette. `AccountDeviceFlowTest` checks that sharing is off until the button is clicked and can be stopped locally. Real desktop behavior and screen/input interaction on two machines remain to be tested.

### Relay TLS transport
Added optional TLS to the Netty relay and Qt relay client. Relay certificate/key must be configured together; when `REMOTE_RELAY_TLS=required`, Qt waits for the encrypted handshake and verifies the hostname before allowing RDTP writes. A CA path cannot silently fall back to plaintext. The relay module tests and Qt relay tests pass. A local TLS relay with a temporary `localhost` certificate passed a positive Qt handshake and rejected a connection to `127.0.0.1` for hostname mismatch. See `docs/security/RELAY_TLS.md` for configuration. Plaintext remains the local compatibility default, so deployment configuration must explicitly enable TLS on both sides.

### Relay reconnect pacing
Repeated CHILD allocation/transport failures now use an exponential delay from 1.5 seconds up to 30 seconds with up to 499 ms jitter. A successful relay registration or local stop resets the delay. A regression test first reproduced the old fixed 1.5-second retry interval, then verified that the third failed allocation is deferred. All seven Qt test executables pass after the change. This reduces synchronized reconnect bursts when a relay node or Management API is unavailable.

### Validation container cleanup update
After the Docker daemon restarted, the five validation application containers could be removed. Docker still denies killing the validation PostgreSQL and Redis containers. Their restart policies are disabled, and both were disconnected from the validation network. Only `pbl4-validation-postgres-1`, `pbl4-validation-redis-1`, and their two disposable volumes remain; remove these exact resources when the daemon permits it. Other project and user containers were not touched.

### Redis dispatch microbenchmark
Added `server/tests/dispatch_benchmark.py` to measure the actual `register.lua` and `allocate.lua` scripts against a disposable Redis DB 15. It refuses a nonempty DB and requires `TEST_DISPOSABLE_REDIS=1`; the script clears the test DB on exit. On the isolated validation Redis, three alternating rounds of 300 new child allocations each gave one node 300/300 per round and two nodes 150/150 per round. Across 900 allocations per case, the sequential local-bridge p50/p95 were 0.040/0.120 ms (one node) and 0.046/0.090 ms (two nodes). Redis DB 15 was verified empty afterward and disconnected from the validation network. These timings exclude HTTP, authorization, real relay traffic, network contention, and screen capture; they demonstrate even distribution and dispatch cost only. A full one-vs-two relay load test remains outstanding.

### Local relay data-plane benchmark
Added `server/tests/relay_transport_benchmark.py`, which starts one or two real Java relay processes and a loopback-only test authorizer, then establishes CHILD/ADMIN RDTP sessions and measures forwarded frames. No database or user account is involved; all spawned processes exit after the run. On this single workstation, 24 sessions sending 40 frames each at 5 FPS and 64 KiB/frame produced 7.69 MiB/s with one relay and 7.68 MiB/s with two. Frame p50/p95 was 3.550/5.777 ms (one) and 3.771/5.946 ms (two). Relay process CPU time during the measured interval was 1.38 s (one) and 1.58 s combined (two); RSS after the interval was 244.6 MiB and 465.3 MiB combined. The paced source caps offered traffic at about 7.7 MiB/s, so this result does not show a throughput gain from a second relay on the same host. A separate unpaced burst reached roughly 1 GiB/s aggregate in both layouts, also on loopback. This benchmark excludes capture/JPEG, the Qt client, TLS, Internet latency, and true multi-host availability. It is reproducible evidence for relay forwarding, not a substitute for a two-machine user test.

### Input payload boundary
The relay previously forwarded any payload length for mouse/key message types (up to the generic 512 KiB RDTP limit), even though CHILD discards input that does not match the 4/8/9/12-byte protocol shapes. A regression test reproduced a 256 KiB `MOUSE_MOVE` reaching CHILD. The relay now checks exact input lengths, closes a malformed sender, and rejects oversized input from the RDTP header before allocating its payload. Other non-screen messages are capped at 8 KiB, above the maximum 4200-byte authorization payload. Focused and full Maven tests pass; the two-relay E2E script now sends valid payloads for each input type. This bounds malformed-control allocation per connection but is not a global connection-rate limit.

### Whole-frame send budget
The CHILD sender previously checked its socket queue before capture, then could enqueue an entire large JPEG frame in chunks beyond that threshold. It now keeps each encoded frame within 1 MiB by reducing image dimensions only when needed, and admits the whole packetized frame only if the resulting queue stays within 2 MiB. Frames are skipped when the receiver cannot keep up, so queued old images do not grow without bound. The actual encoded image dimensions are passed to pointer mapping. A noisy-image regression verifies that an oversized source yields a decodable bounded JPEG instead of disappearing. Real quality and frame pacing on two machines still require the acceptance test.

The Java relay also contained `ScreenFrameHandler` and a second frame reassembler that were never installed in its Netty pipeline or used by tests. They only logged complete JPEG frames and could allocate from untrusted chunk counts if activated. Both were removed; the relay remains a bounded chunk forwarder and ADMIN remains responsible for reassembly.

### Local API default
A fresh Qt client previously fell back to a fixed ngrok hostname when no Settings value or `REMOTE_API_URL` existed. That could direct login credentials to an unrelated endpoint after the demo tunnel changes ownership. The fallback is now `http://localhost:9090`, matching the Management API's local default. Remote deployments must set their own HTTPS API URL. The Qt app builds and the account/device flow test passes after the change.
