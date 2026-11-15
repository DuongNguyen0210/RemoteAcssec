# Management API smoke test

Use PowerShell 7 and a disposable Compose project with its own PostgreSQL and Redis volumes. Supply fresh values for `POSTGRES_PASSWORD`, `JWT_SECRET` (at least 32 bytes), and `RELAY_NODE_TOKEN` in the process environment. Set `DATABASE_JDBC_URL=jdbc:postgresql://postgres:5432/remote_db` to select the disposable database, and leave `REDIS_HOST=redis`. Do not load a development or production `.env`. The smoke override binds the API and both Relay nodes to loopback on ports 19090–19092. It requires Docker Compose 2.24.4 or later for `!override`.

After the services are ready, run the script from the repository root, substituting the published Management API port.

```powershell
docker compose -p pbl4-agent-smoke -f infrastructure/docker-compose.yml -f infrastructure/docker-compose.smoke.yml up -d --build --wait
./infrastructure/smoke-management.ps1 -BaseUrl 'http://127.0.0.1:19090'
```

The script creates two disposable ADMIN accounts and one CHILD account. It checks duplicate CHILD registration, login, device presence after a heartbeat, owner isolation, relay endpoint lookup, and disappearance from the device list after logout. The accounts persist in the test database.

## Verification on 2026-11-14

- Branch `agent/persistence-smoke`, base `3cfd918`. Clean main fails the Management API image build because three tests use outdated constructors. The successful run used temporary fixture updates in the build workspace and restored them afterward. PR #21 addresses the controller fixtures.
- PASS on PostgreSQL 16, Redis 7, Nginx, Management API, and one Relay node under the separate `pbl4-agent-smoke` project. PostgreSQL reported unique usernames for both account tables and the `child.user_id` foreign key. Redis returned PONG. No schema migration was needed for this tested path.
- A follow-up run used the loopback override to start both Relay nodes despite the existing stack using port 9091. PASS with both nodes running and with each node stopped in turn. After waiting 32 seconds for the stopped node's lease to expire, `-ExpectedRelay` asserted that a fresh CHILD allocation used the remaining node.
- Desktop frame delivery, recovery of an existing session and the full Maven regression run still require verification.

## Integration with the current branch

The smoke override is retained from the teammate branch. The current base Compose also requires an explicit JDBC URL and JWT secret; the prerequisites above include these. The historical results above belong to that branch and are not a claim that the PowerShell script has been rerun here. Current Linux host/VM full UI and active relay-loss evidence is in `docs/testing/FULL_UI_ACCEPTANCE.md`.
