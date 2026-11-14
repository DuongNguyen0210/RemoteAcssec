# Host/VM full UI acceptance

The opt-in `FullUiDesktopAcceptance` executable drives the real widgets and
`AppCoordinator`. Unlike `DesktopRelayAcceptance`, it logs in through the real
API and uses dynamic relay allocation. Build it on host and the dedicated
`PBL4-Acceptance` guest with `ENABLE_X11_INPUT_INTEGRATION=ON`.

## Disposable backend

Start `infrastructure/docker-compose.yml` under a separate Compose project such
as `pbl4-ui-acceptance`, with independent PostgreSQL/Redis volumes and fresh
JWT/database/relay service secrets. Use a private environment file outside Git.
Never point this test at the development or production database: it creates
temporary owner/CHILD accounts. Both relays' advertised host must be reachable
from host and guest, and published ports must match the advertised ports.

The verified stack used API `127.0.0.1:18089` on host and `10.0.2.2:18089` in
the guest, two API instances behind Nginx, and relay ports 19091/19092 on the
host's LAN interface. Test containers had restart disabled. The VM's NAT
loopback access is configured as described in `ISOLATED_VM.md`.

```sh
TEST_DISPOSABLE_UI_STACK=1 python3 server/tests/full_ui_vm_acceptance.py \
  --admin-binary /tmp/pbl4-qt-build/FullUiDesktopAcceptance \
  --ssh-key /home/duongnguyen/.cache/pbl4-vm/test-key \
  --known-hosts /home/duongnguyen/.cache/pbl4-vm/known_hosts \
  --log-dir /home/duongnguyen/.cache/pbl4-vm/acceptance-full-ui
```

The harness uses separate temporary settings directories, generated disposable
credentials and the VM's private Xvfb display. No account token is injected into
the Qt clients: both clients fill and submit the actual login form.

## Verified behavior

- CHILD login opens its status window with sharing initially off.
- Selecting “Bật chia sẻ” starts heartbeat and dynamic relay registration.
- ADMIN login loads the online device list through the real API/SSE path.
- Selecting the device looks up its assigned relay and opens the real viewer.
- Viewer mouse/keyboard events change a native button and text field in the VM;
  received images confirm both changes.
- CHILD selects “Dừng chia sẻ”; its relay connection closes, ADMIN closes the
  viewer, and the main window stays usable with an inline session-ended notice.
  Session termination does not mark otherwise current device data as stale.
- On the same login, CHILD re-enables sharing and ADMIN opens a second session.
  Both clients observe a new session ID. Native input and CHILD local stop
  pass again, without a per-session consent dialog. Each client observes exactly
  two session starts/ends; old frame state is cleared between them.

The first run exposed a modal session-ended error in `DevicesController` that
also reused the data-refresh error state. It is now an inline session notice,
cleared on the next connection attempt. The revised full UI run passed on host
Qt 6.10.2 and guest Qt 6.4.2. The subsequent two-round run also passed and logs
`CHILD_REPEAT_CONFIRMED` and `ADMIN_REPEAT_CONFIRMED`. This test does not cover
Windows or every keyboard/mouse variant.

## Active relay failure

Add `--failover-project pbl4-ui-acceptance` to the harness command when using
that disposable Compose project. After confirmed native input in the first
session, the harness reads the actual relay assignment from the API, verifies
the container's Compose project label, and stops only that assigned relay.
CHILD keeps sharing enabled. ADMIN must observe session end; CHILD must
register again, and the API must assign a different relay before ADMIN opens
the next session. A marker file coordinates that readiness with the UI test.
The second session must again pass native mouse/keyboard and CHILD local stop.

The real host/VM run passed: `relay-proxy-2` stopped, CHILD registered on
`relay-proxy-1` after 48.46 seconds measured from the start of the stop command,
and ADMIN opened a working new session. This timing includes lease expiry and
the client's capped retry backoff; it is one measurement, not a recovery SLA.
The relay processes shared a host in separate containers. This proves node
process loss and new-session recovery, not physical-host independence or live
migration of an active session. Session IDs are local to each relay, so the
failover run verifies the new instance rather than requiring unequal numeric IDs.

After the run, remove only this disposable Compose project with `down --volumes`
using the same environment/config files, then shut down only `PBL4-Acceptance`.
Keep the user's three lab VMs and development databases untouched.
