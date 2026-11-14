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
- [ ] Audit core/missing/out-of-scope functions with concrete source evidence.
- [ ] Realtime: SSE uses configured API URL; safe cancellation; retry transient failures with capped jitter; idle watchdog and bounded input. Server avoids full-list fanout per heartbeat, periodically reconciles expiry, cleans failed emitters. Proxy streams without buffering.
- [ ] Streaming: review backpressure and redundant session handling; retain session authorization/consent.
- [ ] Desktop UX: remove demo records and misleading controls, expose reliable connection state; preserve all existing palette values.
- [ ] Persistence: run isolated Redis and PostgreSQL checks, introduce only evidenced schema changes.
- [ ] Build/test both stacks and record limits and remaining feature gaps.

## Commands and conventions
- Server: `mvn -f server/pom.xml test`
- Client: `cmake -S clients -B /tmp/remoteaccess-build -DBUILD_TESTING=ON`, `cmake --build /tmp/remoteaccess-build -j 4`, `ctest --test-dir /tmp/remoteaccess-build --output-on-failure`
- JUnit/Mockito tests in server modules; QtTest in clients/RemoteAccessApp/tests. Add regression tests before behavior changes.
- Keep Qt signal/slot boundary between GUI/controllers/network; Java constructor injection; small classes with one responsibility.
- Example: `if (m_loading) return;` prevents overlapping requests; do not introduce global polling.

## Initial evidence
PresenceService scans Redis and serializes owner snapshots on each heartbeat. TTL expiration has no notification. SseClient ignores saved API URL and can dereference a null reply during abort's synchronous finished signal. Nginx buffers SSE and has a 15-second read timeout. Logs/Sessions include demo data. RemoteScreenWidget only renders frames; actual mouse/keyboard capture and injection are absent.

## Git reconciliation (2026-11-13)
Fetched origin. Local feature head 1679900 includes settings and test fixes beyond origin/main 5939507. Teammate integration head 734ab5f includes ee8c42a (screen viewer) and bc167fa (graceful disconnect). Combined both on codex/pbl4-reliability-ux, resolving MainWindow conflicts by retaining injected SettingsPage and live SessionsPage. No push or shared-branch rewrite.
Initial server suite: 15 management tests, one existing failure: nonexistent owner returns empty devices instead of 401. Relay integration: 26 tests passed. Initial merged Qt run exposed absent startup refresh, corrected in AppCoordinator.
Database read-only inspection: child.user_id has FK but no lookup index; users/child username uniqueness exists. No database mutation performed.

## Verified increments
- Integrated teammate branch, preserving local Settings; all seven Qt test executables pass (startup regression re-run after fix); 26 relay tests pass.
- Restored 401 for deleted owners instead of misleading empty device list. Full Maven reactor passes (17 management + 26 relay tests).
- Added regression for decode progress with multiple queued frames: reproduced failure (only newest delivered), then removed newest-arrival equality guard. One decoding task plus one latest pending JPEG remains bounded; session generation still rejects images from ended sessions.

## Remaining work
SSE retry/address/idle behavior and bounded fanout; presence expiry and Redis lookup cost; misleading logs/settings and streaming duplication; database index/migration verification; runtime UX evidence and final review. Actual input capture/injection is missing despite protocol message types and relay forwarding; record this honestly as a core capability gap in the final feature audit.

### SSE client recovery
Uses ApiClient's configured base URL (regression reproduced with local HTTP server before fix). Retries transient HTTP failures with exponential 3–30 second delay plus 0–999 ms jitter; stops on 401/403, including empty bodies. A 45-second transfer inactivity timeout recovers silent streams. Network read buffer and incomplete event buffer are bounded to 1 MiB; overflow disconnects and retries. Complete SSE blocks reset backoff. Verified local HTTP tests for configured address, synchronous cancellation, 503 reconnect followed by empty 401, and oversized incomplete event; account/device flow also passes. Server-side periodic snapshots/keepalives remain necessary and pending.

### Database check (2026-11-13)
The live PostgreSQL database had 1 users row and 2 child rows. `child.user_id` lacked an index, while `ChildRepository.findByOwner` is used by account listing and periodic presence snapshots. Added `idx_child_user_id` in the JPA mapping and an idempotent SQL migration. Tested the migration twice in a disposable PostgreSQL 16 container, then applied it to the live local `remote_db`; row counts remained 1 and 2. No table data was changed. Redis integration tests were not run in this increment; the existing 20-second presence TTL still requires an isolated Redis run for final verification.

### Redis integration and streaming cleanup
Ran `PresenceRedisIT` against a disposable Redis 7 container on port 16379. The first run exposed a stale test assertion using `presence:v2:` while the current implementation writes `presence:`. Corrected the assertion; the integration test then passed, including the real 20-second TTL expiry, independent sessions, and logout behavior. Removed the unused `ScreenStreamSender` relay registration/session state path (187 lines); `ChildSessionController` remains the single owner of that state. Qt build and all 7 CTest executables pass after the removal.

### Desktop navigation cleanup
The sidebar's “Kết nối mới” action now opens Devices, where an actual device can be selected. Removed disabled notification/help/profile icons from the top bar; they had no backing behavior. QSS was not modified. Built the Qt client and ran the account/device and screen viewer test executables after the edit.

### Remaining acceptance evidence
A full two-machine demonstration is still outstanding: real input injection and DPI mapping, relay node failure/reallocation, and one-vs-two relay load measurements. The CHILD auto-accepts sessions, so an explicit consent/unattended-access policy is still needed. Relay TCP has no TLS; do not expose its token-bearing traffic on an untrusted network. See CORE_IDEA_REVIEW.md for the capability audit. These are product/architecture gaps, not proven complete by unit tests.

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
