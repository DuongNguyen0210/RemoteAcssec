# Two-machine acceptance for the PBL4 remote-control core

This is the remaining manual acceptance gate. Use two physical computers or
separate desktop VMs: one ADMIN viewer and one CHILD being controlled. Record
OS, display server, resolution, scale factor, API address, relay hosts, and
Git commit before starting. The current CHILD input backend supports Windows
and Linux X11 (`xcb` with `libXtst.so.6`); native Wayland input is unsupported.

## Local X11 backend check

Before using two desktops, native capture/input can be checked on a private
Xvfb server. Install Xvfb and the XTest runtime, then run:

```sh
cmake -S clients/RemoteAccessApp -B /tmp/pbl4-qt-build -DBUILD_TESTING=ON -DENABLE_X11_INPUT_INTEGRATION=ON
cmake --build /tmp/pbl4-qt-build -j 4 --target X11InputIntegrationTest
python3 clients/RemoteAccessApp/tests/run_x11_integration.py /tmp/pbl4-qt-build/X11InputIntegrationTest
python3 clients/RemoteAccessApp/tests/run_x11_integration.py /tmp/pbl4-qt-build/X11InputIntegrationTest --scale 2
```

The runner allocates and stops its own X server and never uses the existing
desktop display. It verifies a captured color marker, native button click,
text input and releasing a held modifier at 100% and 200%. The test verifies
the actual screen pixel ratio and capture dimensions before injecting input.
This reproduced a CHILD crash caused
by unloading XTest before X11 extension cleanup; closing the display first
fixed it and the test passes. This is local Linux backend evidence, not a pass
for the two-machine, Windows, network or high-DPI acceptance rows below.

## Prepare

1. Start the Management API, PostgreSQL, Redis and two relay nodes. Their
   advertised host and port must be reachable from both desktops. Check that
   the API returns distinct relay assignments for two CHILD accounts using
   `server/tests/relay_load_balancing.py` against a disposable stack first.
2. Use HTTPS for the API and enable relay TLS on an untrusted network as
   described in `docs/security/RELAY_TLS.md`. The relay certificate's SAN must
   match the advertised host. Set `REMOTE_RELAY_TLS=required` and, for a
   private CA, `REMOTE_RELAY_CA_CERT` on both desktops.
3. Point both apps at the same API in Settings or `REMOTE_API_URL`. An existing
   saved Settings URL takes precedence over the environment variable. Set
   `REMOTE_RELAY_MODE=dynamic` on both desktops before launch.
4. Log in as CHILD and confirm that sharing is initially off. Click **Bật
   chia sẻ** and wait for **Sẵn sàng nhận phiên điều khiển không giám sát**.
   Log in as ADMIN, select that CHILD and open the remote viewer.

## Record pass or failure

| Check | Expected observation | Result and evidence |
|---|---|---|
| Screen | Viewer shows current CHILD desktop and keeps updating for 2 minutes | Host/guest fixture: 596 decoded frames in 120000 ms, max gap 312 ms; full login/sharing GUI remains pending |
| Pointer | Move and click all four screen corners at 100% and a high-DPI scale such as 200%; no offset or clipped area | |
| Keyboard | Type letters, digits, Backspace, Enter and modifiers into a test editor; releasing viewer focus releases held keys | |
| Local stop | CHILD clicks **Dừng chia sẻ**; viewer ends and further input no longer affects CHILD | |
| Unattended policy | CHILD re-enables sharing; a new valid ADMIN session starts without a per-session prompt | |
| Relay failure | Stop the assigned relay; after its lease expires, CHILD registers on the other relay and ADMIN opens a new session | |
| Load | Repeat with two CHILD machines on different relays; record FPS, p50/p95 input delay, CPU/RAM and bytes/s for each node | |

For each failed row, save a timestamped screenshot or short recording, client
and relay logs without tokens, and the exact OS/display/scale settings. An
active session is expected to end when its relay disappears; this version
reallocates and starts a new session rather than migrating it live. The
single-host synthetic measurements in `docs/review/PBL4_IMPROVEMENT.md` are
supporting evidence, not a pass for this gate.
