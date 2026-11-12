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
