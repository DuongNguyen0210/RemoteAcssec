# PBL4 completion audit

This audit covers the original improvement request and the later Git/CodeGraph/Antigravity additions. Implementation and acceptance are complete within the student PBL4 scope, including the additional reviewed session, input and presence fixes. See `PBL4_IMPROVEMENT.md` for the change history, `docs/testing/FULL_UI_ACCEPTANCE.md` for Linux host/VM transport/UI evidence and `docs/testing/WINDOWS_VM.md` for native Windows 100%/200% and CTest results. Physical-host WAN performance and production deployment are outside the evidence collected here. Antigravity's independent review and the verified fixes are recorded separately below.

| Requested outcome | Current evidence | Acceptance status |
|---|---|---|
| Identify missing and excessive features | Capability audit in `PBL4_IMPROVEMENT.md`; demo metrics/actions and unused relay JPEG path removed; chat/file transfer/clipboard remain outside this PBL4 scope | Code audit and Linux host/VM acceptance complete; Windows native 100%/200% passed |
| Simplify code and preserve upgrade paths | CHILD session ownership consolidated in `ChildSessionController`; GUI/network separated by signals; relay forwards chunks without duplicate JPEG reassembly | Implemented and covered by module tests |
| Bound client/server traffic and keep data current | Bounded SSE parsing/retries, coalesced server snapshots every 3 seconds, bounded publisher workers, CHILD relay backoff, whole-frame 1 MiB encoding and 2 MiB send budget | Automated checks plus Linux host/VM two-minute delivery, TLS, UI and failover evidence; physical-host/WAN quality remains unmeasured |
| Improve UX | Explicit unattended sharing switch and local stop; real empty/error states; functional device search; removed nonfunctional session controls | Full UI Linux host/VM login, two sessions, input, local stop and failover pass; Windows native and seven CTests passed |
| Preserve desktop theme | Diff against `origin/main` changes no existing QSS rule or color; only adds CHILD window background using existing `#f7f9fb` | Verified by source diff |
| Test and update database if needed | Idempotent `child.user_id` index migration tested on isolated PostgreSQL and applied without row-count changes; isolated Redis presence TTL test passed | Recorded in `PBL4_IMPROVEMENT.md`; no further schema change evidenced |
| Check teammate Git work | Remote refs fetched; `git branch -r --no-merged HEAD` returns no branches; local deployment edits reviewed with owner confirmation | All fetched remote heads included; teammate deployment work integrated |
| Use CodeGraph | Index synchronized after changes; symbol/caller/impact queries used for streaming and controller work | Done |
| Connect local Antigravity for collaboration | Installed agent API connection, independent source review, backend fixes and tests; Codex verified the combined UI/runtime result | Done; report and resolutions linked below |

## Antigravity collaboration

At the owner’s request, Codex connected to the running local Antigravity app using its installed `agentapi`. The app accepted a new review conversation (`0169db97-9abb-41d5-8ab9-5b7b47623a51`) and the reviewer created `ANTIGRAVITY_REVIEW.md`. The assignment is read-only source review of remote input, SSE/relay bounds and sharing authorization, with that report as its only writable file. No credentials or personal VM contents were supplied. The independent review completed. Antigravity then implemented the credential-bound session and unshare backend changes; Codex reviewed its dependency/error handling and verified the combined result through both native platforms, real Redis, PostgreSQL/API password reset, ordinary UI stop/re-enable and active relay failover. See `ANTIGRAVITY_RESOLUTIONS.md` and `docs/security/CHILD_SESSION_BINDING.md`.

## Integration review of shared deployment work

The shared worktree contained deployment/JWT changes when this review began.
The owner confirmed that the teammate finished them
and authorized integration. Compose validation with disposable placeholder secrets passes. The
latest server test reports contain 23 Management API and 31 relay tests, with
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

## Runtime evidence and limits

Native Linux X11 capture and injection now pass on a private Xvfb desktop via
`X11InputIntegrationTest`: a real button click, keyboard entry, modifier release
and screen color marker are verified at both 100% and 200%, with assertions on
the actual pixel ratio and capture dimensions. This test reproduced and guarded a crash
in XTest library teardown. It does not exercise relay transport or Windows.

- The owner excluded all three existing lab VMs and authorized a new test VM.
  `PBL4-Acceptance` now runs Ubuntu 24.04 separately; clean client builds and
  native X11 tests at 100%/200% pass there. See `docs/testing/ISOLATED_VM.md`.
  Host/guest relay screen, native click, keyboard and disconnect now pass via
  `DesktopRelayAcceptance` using a real relay and fake authorizer. A static-scene
  host/guest hold received 596 decoded frames in 120000 ms (4.97 FPS), with a
  maximum frame gap of 312 ms, then disconnected cleanly.
  A separate required-TLS host/guest run passed the same input/disconnect
  checks with 49 frames in 10 seconds and a maximum gap of 207 ms. The real UI
  login, sharing switch, dynamic allocation/lookup, native input and CHILD local
  stop passed against a disposable two-API/two-relay stack; see
  `docs/testing/FULL_UI_ACCEPTANCE.md`. A subsequent two-round run also passed
  stop/re-enable, distinct new session IDs, native input and local stop again
  on the same login. Active relay loss also passed: the harness stopped the
  assigned relay-proxy-2, observed reassignment to relay-proxy-1 after 48.46 s,
  and opened a new session with working native input and local stop. Both
  relays ran in separate containers on one host. Windows native capture/input at 100%/200% and all seven CTests subsequently passed; see `docs/testing/WINDOWS_VM.md`.
  `docs/testing/TWO_MACHINE_ACCEPTANCE.md` records the required observations.

The two remaining isolated validation containers later became stopped. Both
were removed, together with `pbl4-validation_postgres-data`,
`pbl4-validation_redis-data` and `pbl4-validation_default`. No validation
containers, volumes or networks remain. The user's development resources were
not selected for removal.

Do not interpret local unit tests, loopback benchmarks, or the existence of the
manual checklist as proof that unmeasured physical-host/WAN or production deployment scenarios passed.

## Newly fetched teammate branches (2026-11-15)

Fetched and integrated all five `origin/agent/*` branches: presence reconciliation, SSE proxy streaming, log empty state, persistence smoke, and RDTP documentation. The current bounded 3-second publisher already covers expiry without heartbeat, and the dynamic Nginx template already streams SSE. Their implementations were retained to avoid adding an unbounded second scheduler or restoring retired static Nginx configuration. Real bounded session logs were retained instead of replacing working activity data with an unavailable page. Teammate owner-isolation and Redis-expiry tests were adapted to the current publisher API; smoke prerequisites and protocol status were reconciled.

After integration, Management API and relay tests pass (23 + 31), all seven desktop CTest executables pass, and an explicit three-case Redis integration run passed against a disposable Redis container. That container was then removed. The smoke Compose overlay validates with disposable placeholder values; its PowerShell runtime was not rerun here. All fetched remote branches are ancestors of HEAD.

## Final review-fix acceptance

The current source passes 23 Management API unit tests, 31 relay tests and three explicit real-Redis integration cases (no failures/errors/skips). All seven desktop CTest executables pass on host Linux (34.05 seconds) and the isolated Windows VM (33.08 seconds). Native capture, real click/key input, frame-reference mapping despite changed sender geometry, and button release after geometry disappears pass on Linux and Windows at 100% and 200%.

Both real host/VM UI runs passed after the fixes: the ordinary run covers two distinct sessions with stop/re-enable on the same login; the failover run covers loss of relay-proxy-2 and a working new session on relay-proxy-1 (48.08 seconds in this sample). Each run also confirms immediate API disappearance after successful unshare and old-token rejection after an actual PostgreSQL password update, followed by a working fresh login. Logs are in `acceptance-review-repeat/` and `acceptance-review-fixes/` under the dedicated VM cache. See `ANTIGRAVITY_RESOLUTIONS.md` for the individual findings. The unprofiled X11 library-lifetime suggestion is explicitly deferred; no measured defect or leak was demonstrated.

The disposable stack's containers, PostgreSQL/Redis volumes and network were removed, and its generated environment secrets deleted. Both dedicated acceptance VMs are powered off. All three lab VMs and development databases remain outside this cleanup. VM images, SDK and test evidence remain in the private cache for reproducibility.

Upgrade behavior: deploy the new relay before the new desktop so it accepts mouse frame-reference metadata. Existing CHILD sessions must sign in again after the Management API credential-binding update. No additional database schema change is required for these last fixes. Physical-host/WAN performance, Wayland control and live migration of an active relay session are not claimed.

## Single-login follow-up — 2026-11-16

ADMIN and CHILD now share an atomic per-account Redis login lease. The chosen policy rejects the second login while preserving the first. Independent desktop auth heartbeats keep an unshared CHILD signed in; normal shutdown releases its owned lease, and crash recovery is bounded to about 90 seconds. Old tokens cannot renew or remove replacement sessions. No PostgreSQL migration or existing account deletion is required. See [policy, limits and reproducible checks](../security/SINGLE_LOGIN.md).

Verified with two actual API processes sharing disposable PostgreSQL/Redis, 16-way login races for both roles, real 60+30-second expiry, real Qt login/Enter/renew/logout for both roles, all eight desktop CTest groups, API and relay Maven tests, Redis integration tests, account CRUD/password-reset/quota acceptance, and API-backed Java relay targeting. Full two-desktop VM streaming/failover was not rerun for this follow-up; its harness was adapted to release setup login and share the authenticated UI observer session through a private temporary test-only file. Existing user VMs/services were not changed.
