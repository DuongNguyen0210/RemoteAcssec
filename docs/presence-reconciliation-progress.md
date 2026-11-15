# Device presence reconciliation

- 2026-11-14, `agent/presence-reconciliation` from `origin/main` at `3cfd918`. PR #21 handles SSE client recovery and already includes fixes for two outdated controller test constructors. PR #22 handles proxy streaming. PR #23 reconciles RDTP documentation with main.
- Management API reconciles every five seconds after the prior pass finishes. It reads one authorized device list per subscribed owner, compares it with each subscriber's last successful snapshot, and sends a new list only when visible device state changes. Heartbeats write the existing Redis lease and REST retains the stored heartbeat timestamp. Logout and silent expiry appear on the next successful reconciliation. Reconciliation cost still includes the existing Redis scan per subscribed owner.
- Production files include the Management API application, device controller, presence service and SSE notifier. Coverage includes the SSE notifier unit test and the Redis TTL integration test. The latter checks the production key format and verifies that the notifier emits an empty list after real expiry.
- JDK 17 verification with the two controller test fixtures temporarily taken from PR #21 passed the new SSE notifier test and all 26 relay tests. The full reactor still failed the existing deleted-owner test. PR #21 also contains that behavior fix. Keep this branch separate and rerun the full reactor against updated main after #21 lands.
- Targeted run PASS for both `SseNotifierTest` and `PresenceRedisIT` against Valkey 9.0.3 on isolated WSL loopback. The production 20-second TTL expired without a heartbeat and the notifier emitted an empty device list.
- Full Docker Compose and desktop runtime verification remain pending. This branch does not change the wire format or redeploy services.

## Integration with bounded publisher

Current branch already reconciles subscribed owners every 3 seconds in `DevicePresencePublisher`, with four workers, a 256-task queue, one pending task per owner and per-owner/global SSE connection limits. Keep that implementation rather than adding a second unbounded scheduler. Reconciliation sends current snapshots even without changes to support the client idle watchdog. Teammate multi-subscriber/owner-isolation and real Redis expiry-to-SSE checks were adapted to this publisher API.
