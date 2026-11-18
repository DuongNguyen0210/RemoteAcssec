# Dedicated native Windows acceptance VM

The owner authorized downloading/creating separate test VMs and explicitly
excluded the three existing lab VMs. Never use or modify `Ubuntu`, `vm`, or
`vm 1` for this work.

The dedicated VM is `PBL4-Windows-Acceptance`, UUID
`a8a78994-a087-47c6-a22b-d4574b41f0ab`. Its cache and VM files are under
`/home/duongnguyen/.cache/pbl4-windows-vm/`, outside the repository. It has two
CPUs, 4 GiB RAM, a dynamic 64 GiB VDI, UEFI, TPM 2.0 and NAT. Clipboard and
drag-and-drop are disabled. A read-only `pbl4payload` share contains tracked
client sources and the test SDK; private VM credentials are outside that share.

The installation media comes from Microsoft's public
[Windows 11 Enterprise Evaluation download page](https://www.microsoft.com/en-us/evalcenter/download-windows-11-enterprise),
English (United States), Enterprise LTSC evaluation. The download link is
`https://go.microsoft.com/fwlink/?clcid=0x409&country=us&culture=en-us&linkid=2289029`.
This is an evaluation VM, not a production Windows license or a replacement
for any existing operating system.
The ISO download completed at 5112850432 bytes, matching its HTTP metadata;
its local SHA-256 is recorded in `windows11-ltsc-eval.iso.sha256`. This is a
local reproducibility record, not a comparison against a published Microsoft
hash. VirtualBox detected image 1 as Windows 11 Enterprise LTSC Evaluation
10.0.26100.1742 and confirmed unattended installation support.

The SDK uses Qt 6.10.2 MinGW desktop packages, MinGW 13.1.0 and CMake 3.30.5
from `download.qt.io/online/qtsdkrepository/windows_x86/desktop/`. The selected
archives and URLs are recorded in `sdk-downloads.json` in the private cache;
each archive was checked against its official `.sha1` download. The SDK and
source are staged under `payload/`. No personal SSH keys, project `.env`, Git
credentials or database data are included.

`payload/run-tests.cmd` copies the read-only payload into `%USERPROFILE%\pbl4`, builds the
application/tests, runs `WindowsInputIntegrationTest` at scale 1 and 2 on the
native Windows platform, then runs CTest. It writes individual build and test
logs under `%USERPROFILE%\pbl4-*.log` and creates `%USERPROFILE%\pbl4-tests-passed.txt` only after all
commands succeed. The native test requires `PBL4_ISOLATED_WINDOWS=1` and an
interactive desktop session; a service/session-0 run is not valid evidence.

The native test source is shared with X11 in
`clients/RemoteAccessApp/tests/NativeInputIntegrationTest.cpp`. The existing
Linux executable name remains `X11InputIntegrationTest`; Windows builds opt in
with `ENABLE_WINDOWS_INPUT_INTEGRATION=ON`. Linux 100% and 200% runs pass after
this refactor.

Windows installation completed and Guest Additions 7.2.22 is running after a guest restart. The Windows installation DVD was replaced with the Guest Additions ISO to prevent another unattended installation on cold boot. The desktop reports that the evaluation is valid for 90 days. The test account is `pbl4test`; generated credentials and auxiliary unattended files remain in the private cache.

The payload wrapper runs from the actual logged-in desktop, writes under the test user profile, and calls the canonical repository runner `clients/RemoteAccessApp/tests/run_windows_native.cmd`. The application and tests build successfully with Qt 6.10.2 and MinGW 13.1.0. An earlier root-directory log path was denied to the unelevated test account; moving test output to its profile resolved that setup issue.

## Native acceptance result (2026-11-15)

PASS at `QT_SCALE_FACTOR=1` and `2` on the native Windows platform, interactive session 1. Both runs assert the screen pixel ratio and capture dimensions, sample an actual colored client-area widget, click a real button using native injection, focus a real text field, type `a`, release Shift and type `c`, then verify `ac`. There are no skipped assertions. The revised fixture deliberately gives the injector a different current frame size while each click carries the viewed frame reference, and it verifies native mouse release after clearing capture geometry. All seven ordinary CTest executables also passed (33.08 seconds total after the review fixes). The wrapper wrote its PASS marker.

The initial capture check sampled `(2,2)` near the native rounded window corner before presentation and saw gray instead of magenta. The shared fixture now samples a dedicated 32-pixel client-area marker and retries capture until the compositor presents it. The capture requirement remains strict. Windows compilation also exposed three narrowing warnings in mouse flag initialization; explicit DWORD conversion resolves them.

Logs are retained outside Git at `/home/duongnguyen/.cache/pbl4-windows-vm/evidence/`: `pbl4-build-and-tests.log`, `native-100.log`, `native-200.log`, and `pbl4-tests-passed.txt`. This verifies native capture/input and desktop regression tests on Windows. The full UI API/relay/host-guest acceptance remains the Linux run documented separately; no Windows WAN or elevated-app/UAC-control result is claimed.

The VM is shut down after evidence collection. The latest seven-test run also includes the stop/resume heartbeat ordering regression. No lab VM was started or modified.
