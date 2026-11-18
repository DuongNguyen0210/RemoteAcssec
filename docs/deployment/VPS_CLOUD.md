# Deploy on a VPS or cloud network

For the Vietnamese walkthrough explaining local versus VPS addresses, `.env`, desktop configuration and a complete TLS deployment command, see [LOCAL_TO_VPS.md](LOCAL_TO_VPS.md).

The repository's Compose stack runs two Management API instances, two TCP
relay instances, Nginx, PostgreSQL and Redis. The API instances share the same
database, Redis and `JWT_SECRET`. Relay allocation and presence state are in
Redis; Nginx distributes HTTP requests between the APIs. Nginx expands
`infrastructure/nginx.conf.template` at container startup using `API_*`
environment variables. The default upstreams are Docker service names.

## Single VPS

1. Copy `infrastructure/.env.vps.example` to `infrastructure/.env`.
2. Generate **three independent secrets** with `openssl rand -hex 32` and set
   `JWT_SECRET`, `RELAY_NODE_TOKEN` and `POSTGRES_PASSWORD`. Never commit `.env`.
3. Set `RELAY_1_ADVERTISED_HOST` and `RELAY_2_ADVERTISED_HOST` to a DNS name or
   public IP reachable by both Qt clients. Set each public relay port to match
   its published Docker port. Open those TCP ports in the VPS firewall.
4. Set `MANAGEMENT_PUBLIC_BIND=127.0.0.1` and a free
   `MANAGEMENT_PUBLIC_PORT` when a host HTTPS proxy terminates TLS. If a cloud
   ingress connects directly, bind to the appropriate private interface and
   restrict access at the firewall. Point Qt `REMOTE_API_URL` at the HTTPS URL
   and set `REMOTE_RELAY_MODE=dynamic`.
5. Configure both relay certificate/key pairs in the environment file. Validate and start the TLS overlay:

   ```sh
   docker compose -f infrastructure/docker-compose.yml -f infrastructure/docker-compose.tls.yml --env-file infrastructure/.env config --quiet
   docker compose -f infrastructure/docker-compose.yml -f infrastructure/docker-compose.tls.yml --env-file infrastructure/.env up --build -d
   docker compose -f infrastructure/docker-compose.yml -f infrastructure/docker-compose.tls.yml --env-file infrastructure/.env ps
   ```

The sample binds Management HTTP to loopback on port 8080. HTTPS termination
and certificates belong in the host reverse proxy or cloud ingress; never
expose this HTTP port directly to the Internet. Enable relay TLS with
`infrastructure/docker-compose.tls.yml`, configure certificates on both relay
nodes, and set `REMOTE_RELAY_TLS=required` on both Qt clients before public
Internet use. See [relay TLS configuration](../security/RELAY_TLS.md) for the
certificate paths, private CA option and hostname verification requirements.

## Cloud services and separate hosts

- Set `DATABASE_JDBC_URL` to a PostgreSQL JDBC URL, and set `POSTGRES_USER` and
  `POSTGRES_PASSWORD` to that database's credentials. For managed PostgreSQL,
  use its required TLS parameters (for example `?sslmode=require`).
- Set `REDIS_HOST` and `REDIS_PORT` to a private Redis endpoint. The current
  client configuration does not include Redis password or TLS settings; add
  those before using a managed Redis service that requires them.
- `API_UPSTREAM_1` and `API_UPSTREAM_2` are `host:port` values used by the
  Nginx template. Set `API_DNS_RESOLVER` to a DNS resolver reachable from the
  Nginx container when these are cloud DNS names. Keep both upstreams on a
  private network; do not publish the API instances' port 9090.
- Relay nodes use `RELAY_MANAGEMENT_URL` to reach the private Nginx listener
  on port 9090. When moving relays to another host, route this URL over the
  private network and keep the public `/internal/` path blocked.

The provided Compose file remains an all-in-one deployment: it starts local
PostgreSQL, Redis and both API instances even when external service endpoints
are configured. For separate cloud hosts, deploy the same images and Nginx
template with your orchestrator, supply the variables above, and replace the
Compose `depends_on` rules with platform health checks. PostgreSQL and Redis
remain single-instance dependencies in the all-in-one stack. Run controlled
schema migrations before deploying multiple API instances; the current
application uses Hibernate `ddl-auto: update`.

Changing `JWT_SECRET` invalidates tokens issued with the previous key. Use the
same value on every API instance and rotate it as a planned logout event.
