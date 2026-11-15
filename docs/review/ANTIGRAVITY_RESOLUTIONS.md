# Antigravity findings: implementation and verification

The independent report records findings at `7d9d127`; it is not a claim that those findings remain in the final implementation. Codex reviews and verifies each resulting change before accepting it.

| Finding | Resolution | Current verification |
|---|---|---|
| RC-01, delayed input after dynamic JPEG downscale | Mouse packets carry the viewed frame dimensions; CHILD maps against that reference rather than the newest capture. Relay bounds both legacy and extended payloads, including fragmented TCP decoding. | Mapping/viewer and relay TCP fragmentation tests pass. Linux and Windows native 100%/200% and real host/VM UI/relay failover pass. |
| RC-02, button release after geometry disappears | Release of a tracked native button can proceed at the current pointer location when geometry is no longer valid. | Native fixture presses a real button, clears geometry, releases all and checks the button is no longer down; Linux and Windows 100%/200% pass. |
| RC-03, X11 connection/library per input | Retain current bounded synchronous lifetime. Connections and handles are closed after each event; the report does not demonstrate a leak or measure a bottleneck. A persistent X11 backend is deferred until profiling justifies that complexity. | Existing native and host/VM checks pass; no high-rate input performance claim. |
| SR-01, stale presence after stop | Client serializes in-flight heartbeat, unshare, then a new heartbeat if re-enabled. Backend removes presence while preserving the login session. Abrupt process/network loss retains the 20-second TTL fallback. | Client ordering, backend role authorization and real Redis checks pass; full UI confirms the device list is empty after stop. |
| CA-01, password reset retains sessions | Redis sessions bind the exact credential snapshot used at login; validation compares against the current account credential. | Unit/Redis checks pass. Real PostgreSQL/API password reset rejects the old token on four requests; fresh login/heartbeat/logout pass. |
| SR-02 and CA-02 | Preserve bounded workers/connections, owner isolation, explicit sharing opt-in, unattended acceptance and local stop. | Existing tests/runtime evidence; regression suite rerun after fixes. |

## Deployment compatibility

Deploy the updated relay before the updated desktop: older relays reject extended mouse payloads. The new relay/CHILD accept legacy mouse packets, though legacy packets cannot guarantee coordinates across resolution changes. Keyboard wire format is unchanged. The new Management API rejects legacy CHILD sessions without a credential binding; those clients must sign in again. No database schema migration is needed for this change. See `docs/security/CHILD_SESSION_BINDING.md`.
