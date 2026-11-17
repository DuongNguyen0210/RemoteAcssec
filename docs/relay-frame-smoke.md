# Relay frame smoke test

Run `AdminScreenViewerTest loopbackRelayDeliversSyntheticFrame` against an isolated local Management API and Relay. Build the Qt client with `BUILD_TESTING=ON` first. The regular CTest suite skips this case unless explicitly enabled.

With the Qt runtime on PATH, use the Python 3 standard-library runner to provision the test accounts, select a Relay, run the case, and log out CHILD. The disposable account records persist in the test database.

```powershell
python infrastructure/smoke-relay-frame.py path/to/AdminScreenViewerTest.exe --api-port 19090
```

To run the case directly with existing disposable accounts:

1. Create a disposable ADMIN and an owned CHILD with the same disposable password. The Relay nodes must advertise loopback addresses.
2. Set `PBL4_FRAME_SMOKE=1`, `PBL4_SMOKE_ADMIN_USERNAME`, `PBL4_SMOKE_CHILD_USERNAME`, and `PBL4_SMOKE_PASSWORD` in the test process environment. Set `REMOTE_API_URL` to the loopback URL (for example, `http://127.0.0.1:19090`) and `QT_QPA_PLATFORM=offscreen`.
3. Run the test executable with `loopbackRelayDeliversSyntheticFrame` as its argument. The test sends the CHILD heartbeat, requests the Relay allocation, and logs out CHILD.

PASS requires successful CHILD and ADMIN login via the Qt form, real CHILD Relay registration, a matching nonzero remote session ID at both peers, one decoded red JPEG at the ADMIN with a 32×24 viewer resolution, and CHILD session teardown after ADMIN disconnect.

Verified on the `agent/relay-frame-smoke` branch from `3cfd918` with Qt 6.11.1 on Windows, Nginx, PostgreSQL 16, Redis 7, Management API and Netty Relay in the isolated Compose stack. The test uses a synthetic JPEG and exercises the Qt controllers, TCP transport and viewer update. OS screen capture requires a separate test.
