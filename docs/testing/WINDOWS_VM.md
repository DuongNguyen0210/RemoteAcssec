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

`payload/run-tests.cmd` copies the read-only payload into `C:\pbl4`, builds the
application/tests, runs `WindowsInputIntegrationTest` at scale 1 and 2 on the
native Windows platform, then runs CTest. It writes individual build and test
logs under `C:\pbl4-*.log` and creates `C:\pbl4-tests-passed.txt` only after all
commands succeed. The native test requires `PBL4_ISOLATED_WINDOWS=1` and an
interactive desktop session; a service/session-0 run is not valid evidence.

The native test source is shared with X11 in
`clients/RemoteAccessApp/tests/NativeInputIntegrationTest.cpp`. The existing
Linux executable name remains `X11InputIntegrationTest`; Windows builds opt in
with `ENABLE_WINDOWS_INPUT_INTEGRATION=ON`. Linux 100% and 200% runs pass after
this refactor.

Unattended installation has started on this VM and was observed at 25%.
The test account is `pbl4test`; the generated password is in the private cache's
`test-password` file with mode 0600. Auxiliary unattended files also stay in
that private directory. Guest Additions is selected for installation; the
post-install command writes `C:\pbl4-setup-ready.txt` when it reaches that stage.
To resume, inspect this VM's current state and guest properties/screenshots;
do not restart installation merely because a polling call times out.

Native Windows acceptance is pending installation/build/runtime results.
Do not treat preparing this VM or downloading its SDK as a passed Windows test.
