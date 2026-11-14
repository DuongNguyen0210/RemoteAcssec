# Relay TCP encryption

The Management API and relay are separate connections. HTTPS for the API does not
encrypt RDTP traffic, which contains login tokens, screen frames, and input.

For a relay reachable across an untrusted network, provide both `RELAY_TLS_CERT`
and `RELAY_TLS_KEY` to the relay process. They must point to PEM files readable
by that process. The relay fails at startup if only one is set or the key and
certificate cannot initialize TLS. Use a certificate whose DNS name or IP SAN
matches the `host` returned in the relay endpoint. Configure each relay node
separately; do not commit its private key.
`RELAY_BIND_HOST` defaults to `0.0.0.0`; set it to `127.0.0.1` for a relay
that must accept only local connections. The local transport benchmark uses
this loopback binding.

For the two-node Compose setup, load
`infrastructure/docker-compose.tls.yml` after the base Compose file and set
`RELAY_1_TLS_CERT`, `RELAY_1_TLS_KEY`, `RELAY_2_TLS_CERT`, and
`RELAY_2_TLS_KEY` to absolute host paths. The override mounts the files
read-only into each relay. The container runs as UID 10001, so its key file
must be readable by that user. Compose is still responsible for the usual API,
database, and Redis variables in `infrastructure/.env`.

Set `REMOTE_RELAY_TLS=required` on every Qt client. Qt verifies the server
certificate and endpoint hostname before emitting the relay-ready signal or
sending RDTP. If a private CA signed the relay certificate, set
`REMOTE_RELAY_CA_CERT` to its PEM certificate file. A certificate from a system
trusted CA needs no extra CA path. Handshake or verification failure closes the
connection; the client never ignores SSL errors. If a CA path is set while TLS
is `off`, the client refuses to connect instead of silently using plaintext.

Local plaintext tests can use `REMOTE_RELAY_TLS=off` (the current compatibility
default) with neither relay TLS variable set. Do not use that mode across an
untrusted network. For local TLS testing, a temporary certificate with
`subjectAltName=DNS:localhost` can be used, with the certificate itself passed
as `REMOTE_RELAY_CA_CERT` and relay endpoint host `localhost`. The regular
`RelaySessionTest` also accepts `TEST_TLS_RELAY_PORT` and
`TEST_TLS_RELAY_CERT` to run its positive and wrong-hostname integration cases
against a running TLS relay.

Qt's [QSslSocket documentation](https://doc.qt.io/qt-6/qsslsocket.html) describes
the `encrypted()` readiness signal and certificate verification. Netty's
[SslContextBuilder documentation](https://netty.io/4.1/api/io/netty/handler/ssl/SslContextBuilder.html)
describes the server certificate and key setup.
