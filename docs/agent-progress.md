# RemoteAcssec engineering checkpoint

## Baseline and scope

- 2026-11-14, `agent/sse-proxy-streaming` from `origin/main` at `3cfd918` (PR #20). The original integration checkout contains two untracked Word documents and is separate from this worktree.
- `origin/main` contains the Qt ADMIN viewer and explicit disconnect. Current controller and relay code implement RDTP registration, authorized session setup, and JPEG forwarding. The protocol status document still lists some of these features as pending and needs reconciliation with the merged code.
- GitHub returned no open Issues and one open PR, #21. Its diff includes client SSE recovery, decode progress, device ownership validation, and Settings. Its review document identifies server-side presence fanout, TTL reconciliation, proxy streaming, demo-state cleanup, persistence verification, and remote input as open work. No tracked root README, standalone roadmap, or CI workflow appeared in the fetched tree.

## Milestones

| Priority | Scope and acceptance criteria |
| --- | --- |
| P0 | Proxy the device SSE endpoint. PASS when the first event arrives within two seconds, a second event arrives after 16 seconds of upstream silence, REST still forwards, and `/internal/` returns 404. FAIL if any check fails. |
| P1 | Recheck PR #21 and its ownership before work on presence delivery. PASS when silent expiry updates appear without another heartbeat, steady heartbeats avoid owner-wide fanout, concurrent owners remain isolated, and Redis integration and Java regressions pass. |
| P1 | Reconcile protocol, dashboard, and review-document claims with live capabilities. PASS when demo-only states no longer appear as live data and the Qt regression suite passes. Review PR #21 ownership before editing client code. |
| P2 | Verify persistence and end-to-end behavior against isolated services. PASS when PostgreSQL and Redis integration checks and the documented ADMIN/CHILD smoke flow pass. Add migrations only when verification demonstrates a need. |

## P0 changes

- `infrastructure/nginx.conf` routes `/api/v1/devices/events` without response buffering. Its 75-second read timeout allows Spring's 60-second emitter lifetime. The ordinary API retains its 15-second read timeout and inherits the existing proxy headers.
- PASS on Nginx 1.28.3 for Windows with an isolated loopback HTTP upstream. Both configurations passed `nginx -t`. The baseline lost the second event. The fixed configuration delivered the first in under two seconds and the second after 16 seconds, forwarded REST with the expected proxy header, and returned 404 for `/internal/`.
- Verify this branch with `git log -1` and `git show --stat`. The progress document and proxy configuration are the only milestone files.
- After review, reconcile PR #21 ownership for the next milestone before creating its branch. The full Docker Compose runtime remains unverified because Docker Desktop's Linux engine was unavailable in this session.

## Integration into `codex/pbl4-reliability-ux`

The old `infrastructure/nginx.conf` has been replaced by `nginx.conf.template`. Its exact SSE route already disables buffering/cache and uses HTTP/1.1, with a 45-second read timeout. Current server snapshots arrive every 3 seconds, so the historical 75-second timeout for a quiet stream is unnecessary here. Keep the template and its dynamic API DNS/upstreams; do not restore the retired static configuration. Full UI through this proxy has separate host/VM evidence in `docs/testing/FULL_UI_ACCEPTANCE.md`.
