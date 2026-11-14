# Management API smoke test

Use PowerShell 7 and a disposable Compose project with its own PostgreSQL and Redis volumes. Supply fresh values for `POSTGRES_PASSWORD` and `RELAY_NODE_TOKEN` in the process environment. Set `MANAGEMENT_PUBLIC_PORT` to an unused port. Use a local Compose override to bind published ports to loopback and to select free ports for both relay instances if 9091 or 9092 are in use.

After the services are ready, run the script from the repository root, substituting the published Management API port.

```powershell
./infrastructure/smoke-management.ps1 -BaseUrl 'http://127.0.0.1:19090'
```

The script creates two disposable ADMIN accounts and one CHILD account. It checks duplicate CHILD registration, login, device presence after a heartbeat, owner isolation, relay endpoint lookup, and disappearance from the device list after logout. The accounts persist in the test database.

## Verification on 2026-11-14

- Branch `agent/persistence-smoke`, base `3cfd918`. Clean main fails the Management API image build because three tests use outdated constructors. The successful run used temporary fixture updates in the build workspace and restored them afterward. PR #21 addresses the controller fixtures.
- PASS on PostgreSQL 16, Redis 7, Nginx, Management API, and one Relay node under the separate `pbl4-agent-smoke` project. PostgreSQL reported unique usernames for both account tables and the `child.user_id` foreign key. Redis returned PONG. No schema migration was needed for this tested path.
- The second Relay could not start because the existing development stack already publishes port 9091. The test exercised REST and relay allocation through Nginx. Desktop frame delivery, relay failover, and the full Maven regression run still require verification.
