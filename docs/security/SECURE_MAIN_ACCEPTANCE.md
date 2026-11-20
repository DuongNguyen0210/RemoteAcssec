# Account and connection security on current main

## Integration scope

PR #42 ported the useful security pieces of PR #40 (6197387) onto main ce82f54
and was merged as bfac974. The chained PR #37–#40 were not merged into main as
full branches; PR #38–#40 were merged only into their parent agent branches.
Main already contains the required opt-in sharing/session framework. The
previously accepted viewer device-name fix from #37 is adapted in PR #43.
Other UI/input work outside main remains in its frozen worktree.

Retained from main: mandatory private JWT_SECRET with sessionId claims,
credential-bound single-login leases, CHILD presence binding, account quota lock,
node token authorization, two API processes, Redis dispatch, two TLS Relays,
TLS 1.2/1.3 server restrictions, and private Nginx port9090 for node RPCs.

Reused/adapted from #40: PBKDF2 ADMIN registration/password change, conditional
legacy ADMIN/CHILD upgrades and custom certificate trust in Qt REST/Relay.
SSE shares the same ApiClient request factory and therefore the same TLS policy.
Old #40 JWT and one-API Compose code must not replace main's implementations.

## Credential guarantees

- Existing PBKDF2-SHA256 verifier uses 600000 iterations, 16-byte random salt,
  32-byte derived hash. ADMIN and CHILD use the same existing implementation.
- Legacy migration first claims the login slot, then conditionally replaces the
  exact old DB credential. A rejected duplicate login performs no migration.
- Atomic Redis rebind compares owner and old fingerprint, preserves remaining
  TTL and refuses an expired/draining lease. Conditional DB writes do not
  overwrite a concurrent password reset. Failure releases only its own UUID.
- Fresh credential reread before issuing JWT protects stale login snapshots.
  Conditional JPQL updates clear persistence context to avoid OSIV stale reads.
- Password change/reset revokes old credential-bound REST/SSE/heartbeat sessions;
  main's 30-second drain prevents reuse before Relay authorization leases close.
  This is bounded revocation, not instantaneous termination of all Relay sockets.
- JWT/node/DB secrets are generated outside source. They are not bundled in clients.

## Trust and development compatibility

REMOTE_API_CA_CERT and REMOTE_RELAY_CA_CERT override REMOTE_CA_FILE per transport.
Explicit custom CA files replace default roots; missing/unreadable/empty trust
fails closed. Certificate chain and hostname are verified. No ignoreSslErrors.
Both Qt paths set TLS1.2OrLater. A configured CA prevents HTTP/plain Relay fallback,
including loopback. Main's explicit plaintext lab override remains available only
without a configured CA; do not use it for a real remote deployment.

## Disposable local stack

Run PowerShell7, Docker Desktop and an OpenSSL executable:

~~~powershell
.\infrastructure\start-secure-development.ps1 -RuntimeDirectory 'C:\private\remoteacssec-security' -OpenSsl 'C:\Program Files\Git\usr\bin\openssl.exe'
~~~

Default project pbl4-security-main, loopback API19490 HTTPS, Relays19491/19492 TLS.
DB/Redis volumes are unique to the project. Existing runtimes/data are preserved.
The development certificate intentionally matches localhost only and expires in
30days; use a real CA certificate and externally reachable advertised hosts for
another machine. No certificates are installed in the OS trust store.
The secure overlay replaces only public Nginx port80 with HTTPS443; private
node port9090 stays inside the Compose network. The script retains both APIs.

## Automated acceptance

- Maven reactor test: unit/security tests for registration, hashes, migration,
  compare-and-set races, single-login cleanup, JWT, authorization and Relay.
- Explicit LoginLeaseRedisIT, PresenceRedisIT, RelayDispatchRedisIT against a
  separate disposable Redis; never point these tests at a real database.
- Qt CTest: all existing suites plus security_transport.
- SecurityTransportTest liveHttpsCertificateAndEventStream with TEST_HTTPS_API_URL
  and TEST_HTTPS_CA_CERT: trusted/untrusted/wrong-host REST and SSE tests.
- RelaySessionTest tlsConnectsOnlyToTrustedName with TEST_TLS_RELAY_PORT and
  TEST_TLS_RELAY_CERT: encrypted handshake and hostname rejection.
- python infrastructure/smoke-security.py --ca <runtime>/server.crt
  --viewer-test <build>/RemoteAccessApp/AdminScreenViewerTest.exe
  checks actual Postgres hashes, legacy fixtures, REST/SSE, ownership, revocation,
  reset login and the existing real Qt synthetic-frame smoke over HTTPS/TLS.
  Only pbl4-security-* Compose projects accepted. Fixtures have random names.

Native mouse/keyboard injection into the user's desktop and a real two-PC Internet
session are manual checks; synthetic images do not prove those scenarios.
