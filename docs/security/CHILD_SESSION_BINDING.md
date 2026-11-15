# CHILD credential-bound sessions

A CHILD login stores `auth:child:<sessionId>` as `<childId>:<SHA-256 fingerprint of the stored credential>`, with the existing ten-hour TTL. `AuthService` passes the exact encoded credential snapshot that passed the password check. No credential or fingerprint is put in the JWT or API response. Password verification continues using the existing salted password encoding; the fingerprint is a session binding, not a replacement password hashing scheme.

Every CHILD session validation compares that binding with the account's current encoded credential from the database, using the indexed primary key. Missing accounts, missing credentials, missing session keys, mismatches and old session values containing only a child ID all fail closed. A mismatch removes the obsolete auth key. The heartbeat's Redis Lua script also requires an exact binding match before it can publish presence. This avoids scanning all Redis sessions or introducing a new database column. A login still using an old credential snapshot cannot create a valid post-reset session even if its Redis write occurs after the reset.

## Upgrade behavior

After upgrading the Management API, existing CHILD sessions without a credential binding must sign in again. ADMIN tokens and database schema are unchanged. Password reset makes the old CHILD token invalid on its next authenticated request; the existing relay authorization lease detects it on renewal. The ten-second lease interval is not a strict end-to-end revocation SLA: scheduling and request timeouts also apply. Already published presence can remain until its twenty-second TTL expires, but an invalid session cannot be authorized as a target or renew heartbeat presence.

## Sharing is separate from login

`POST /api/v1/child/unshare` requires a valid CHILD token and derives the owner from the child account. It removes only that session's presence key. The login remains valid, so the user can enable sharing again without logging in. `POST /api/v1/child/logout` removes both auth and presence. The desktop serializes any in-flight heartbeat, unshare and a later resume heartbeat, preventing a normal rapid stop/re-enable sequence from publishing out of order. If the app exits abruptly or the network fails, presence TTL remains the fallback.

## Verification

Unit and real-Redis integration tests cover legacy-session rejection, password reset, session-key cleanup, heartbeat rejection and unshare/resume. The full UI harness additionally checks stop/re-enable across the real API/relay path and attempts repeated requests with an old token after a database password update, followed by a new successful login. Runtime results are recorded in `docs/testing/FULL_UI_ACCEPTANCE.md` after the run.
