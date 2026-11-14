# Run Management API directly on the host

Management API started from an IDE runs **outside** Docker. It cannot reach
PostgreSQL or Redis inside `infrastructure/docker-compose.yml` because that
stack intentionally does not publish their ports. The local development stack
in `server/docker-compose.yml` publishes both services on `127.0.0.1`.

From the repository root, run:

```sh
./server/run-management-api-local.sh
```

The script starts local PostgreSQL and Redis, waits for them, reads
`JWT_SECRET` from the ignored `infrastructure/.env` (or accepts one already in
the environment), and launches the API on port 9090. It does not change the
full relay stack's database. A successful startup responds to
`curl http://localhost:9090/health` with `ok`.

To run from an IDE instead, start dependencies with
`docker compose -f server/docker-compose.yml up -d --wait`. Spring Boot reads
`JWT_SECRET` from `infrastructure/.env` when the IDE working directory is the
repository root, `server` (IntelliJ's default when opening `server/pom.xml`), or
`server/management-api`. An environment variable set in the
IDE run configuration takes precedence. The defaults in `application.yml`
point to the local development database and Redis. The local database credentials are
`admin` / `password123`; use this stack only for development.

If you want to test the **full Docker stack**, start
`infrastructure/docker-compose.yml` instead. It runs its own API instances and
keeps PostgreSQL and Redis private to Docker. Do not start an additional API
from the IDE for that workflow.
