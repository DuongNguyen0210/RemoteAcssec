# PBL4 completion audit

This audit covers the original improvement request. The goal is not yet fully
accepted: real desktop interaction still needs the two-machine acceptance gate.
See `PBL4_IMPROVEMENT.md` for the implementation and measurement history.

| Requested outcome | Current evidence | Acceptance status |
|---|---|---|
| Identify missing and excessive features | Capability audit in `PBL4_IMPROVEMENT.md`; demo metrics/actions and unused relay JPEG path removed; chat/file transfer/clipboard remain outside this PBL4 scope | Code audit complete; real screen/input acceptance pending |
| Simplify code and preserve upgrade paths | CHILD session ownership consolidated in `ChildSessionController`; GUI/network separated by signals; relay forwards chunks without duplicate JPEG reassembly | Implemented and covered by module tests |
| Bound client/server traffic and keep data current | Bounded SSE parsing/retries, coalesced server snapshots every 3 seconds, bounded publisher workers, CHILD relay backoff, whole-frame 1 MiB encoding and 2 MiB send budget | Local automated checks and synthetic relay measurements available; two-host quality/latency pending |
| Improve UX | Explicit unattended sharing switch and local stop; real empty/error states; functional device search; removed nonfunctional session controls | Automated desktop checks available; real desktop interaction pending |
| Preserve desktop theme | Diff against `origin/main` changes no existing QSS rule or color; only adds CHILD window background using existing `#f7f9fb` | Verified by source diff |
| Test and update database if needed | Idempotent `child.user_id` index migration tested on isolated PostgreSQL and applied without row-count changes; isolated Redis presence TTL test passed | Recorded in `PBL4_IMPROVEMENT.md`; no further schema change evidenced |
| Check teammate Git work | Remote refs fetched; `git branch -r --no-merged HEAD` returns no branches; local deployment edits reviewed with owner confirmation | All fetched remote heads included; teammate deployment work integrated |
| Use CodeGraph | Index synchronized after changes; symbol/caller/impact queries used for streaming and controller work | Done |

## Integration review of shared deployment work

The shared worktree contained deployment/JWT changes when this review began.
The owner confirmed that the teammate finished them
and authorized integration. Compose validation with disposable placeholder secrets passes. The
current server test reports contain 20 Management API and 29 relay tests, with
zero failures, errors or skips.
The desktop was rebuilt from scratch and all seven CTest executables passed.
Both the base Compose configuration and its TLS overlay validate; the local
launcher passes `bash -n`. These checks do not prove a VPS deployment is live.

Integration findings:

1. Corrected `docs/deployment/VPS_CLOUD.md` to document optional strict relay TLS; use
   `docs/security/RELAY_TLS.md` and `infrastructure/docker-compose.tls.yml`.
2. Updated `docs/architecture/RELAY_LOAD_BALANCING.md` with the whole-frame
   send budget and encoded-size pointer mapping.
3. The two API instances still start with Hibernate `ddl-auto: update`. The
   deployment document recognizes the migration requirement; concurrent schema
   updates have not been accepted as a production migration process.
4. Keep the shared JWT secret identical on both API instances and require
   configured secrets. The integrated JWT test verifies matching keys work,
   different keys reject tokens, and short keys fail at startup.

## Remaining external evidence

Native Linux X11 capture and injection now pass on a private Xvfb desktop via
`X11InputIntegrationTest`: a real button click, keyboard entry, modifier release
and screen color marker are verified at both 100% and 200%, with assertions on
the actual pixel ratio and capture dimensions. This test reproduced and guarded a crash
in XTest library teardown. It does not exercise relay transport or Windows.

- The owner excluded all three existing lab VMs and authorized a new test VM.
  `PBL4-Acceptance` now runs Ubuntu 24.04 separately; clean client builds and
  native X11 tests at 100%/200% pass there. See `docs/testing/ISOLATED_VM.md`.
  Host/guest relay screen, native click, keyboard and disconnect now pass via
  `DesktopRelayAcceptance` using a real relay and fake authorizer. Full UI,
  dynamic allocation/failover and Windows remain unverified. A static-scene
  host/guest hold received 596 decoded frames in 120000 ms (4.97 FPS), with a
  maximum frame gap of 312 ms, then disconnected cleanly.
  `docs/testing/TWO_MACHINE_ACCEPTANCE.md` records the required observations.

The two remaining isolated validation containers later became stopped. Both
were removed, together with `pbl4-validation_postgres-data`,
`pbl4-validation_redis-data` and `pbl4-validation_default`. No validation
containers, volumes or networks remain. The user's development resources were
not selected for removal.

Do not interpret local unit tests, loopback benchmarks, or the existence of the
manual checklist as proof that these remaining gates passed.
