# Isolated PBL4 acceptance VM

The owner authorized a new test VM and explicitly excluded the existing lab
VMs (`Ubuntu`, `vm`, `vm 1`). Do not start, stop, clone or modify those lab VMs.

The dedicated VirtualBox VM is `PBL4-Acceptance`, UUID
`a5f51f2e-b2af-4059-8357-f1c78b65a338`. Its files are outside the repository in
`/home/duongnguyen/.cache/pbl4-vm/`. It uses 2 CPUs, 2 GiB RAM, a dynamically
allocated 20 GiB disk, and NAT networking. SSH is forwarded only from host
`127.0.0.1:22224` to guest port 22. The private test key stays in that cache
directory and must never be committed.

The base is the official Ubuntu 24.04 amd64 cloud VMDK from
<https://cloud-images.ubuntu.com/noble/current/>. Its SHA-256 was checked
against the downloaded official `SHA256SUMS` before attaching a separate VDI
copy. A NoCloud seed provisions a `pbl4` test account with SSH key access and
installs the Qt6 build tools, Xvfb, Openbox, XTest and xterm.
Install `qt6-l10n-tools` as well: CMake invokes `lrelease` to generate the
Vietnamese `.qm` resource in the build directory from the tracked `.ts` file.

```sh
VBoxManage startvm PBL4-Acceptance --type headless
ssh -i /home/duongnguyen/.cache/pbl4-vm/test-key -p 22224 \
  -o UserKnownHostsFile=/home/duongnguyen/.cache/pbl4-vm/known_hosts \
  pbl4@127.0.0.1
```

Tracked client source was copied to `/home/pbl4/source/clients/` in the guest.
No project `.env`, database data, Git credentials or personal SSH keys were
copied. The guest's serial boot log is `serial.log` in the VM cache directory.

This VM is preparation for host/guest acceptance. Its existence and successful
boot do not prove screen/control or relay recovery. Record actual results in
`TWO_MACHINE_ACCEPTANCE.md` after executing the application tests.

The client and test executables built successfully on Ubuntu 24.04 with Qt
6.4.2. The first build exposed the missing generated translation resource;
CMake now creates it. Six CTest executables passed initially; the remaining
screen reassembler test needed an explicit offscreen platform in CTest and
passed after that fix. Native X11 input/capture passed separately at both 100%
and 200%.

## Host/guest transport acceptance

Build `DesktopRelayAcceptance` on host and guest with the same opt-in CMake
option. While this test VM is stopped, enable host loopback access for its NAT:

```sh
VBoxManage modifyvm PBL4-Acceptance --nat-localhostreachable1 on
```

See the [VirtualBox command reference](https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/vboxmanage.html).
Build the Java relay JAR and runtime dependencies as described in
`server/tests/relay_transport_benchmark.py`. With the VM running, execute:

```sh
python3 server/tests/desktop_vm_acceptance.py \
  --admin-binary /tmp/pbl4-qt-build/DesktopRelayAcceptance \
  --ssh-key /home/duongnguyen/.cache/pbl4-vm/test-key \
  --known-hosts /home/duongnguyen/.cache/pbl4-vm/known_hosts \
  --log-dir /home/duongnguyen/.cache/pbl4-vm/acceptance-transport \
  --duration-seconds 120
```

This passed with host Qt 6.10.2 and guest Qt 6.4.2. The real Java relay forwards
JPEG frames captured by `ScreenStreamSender`; `AdminSessionController` decodes
them and sends mouse/key packets back. `ChildSessionController` delivers the
packets to `RemoteInputInjector`. A native button click changes a color marker,
and text entry changes it again; ADMIN verifies both changes in received images.
CHILD observes disconnect and both processes exit successfully. Logs contain
`SCREEN_RECEIVED`, `REMOTE_CLICK_CONFIRMED`, `REMOTE_KEY_CONFIRMED`,
`CHILD_READY` and `CHILD_DISCONNECTED`.

The sustained run held the stream for exactly 120000 ms after input
confirmation, received 596 decoded frames (4.97 FPS), and recorded a maximum
inter-frame gap of 312 ms. The acceptance checks require at least 2 FPS on
average and no gap over 5 seconds, including the gap at the end. The temporary
X server runner's timeout is extended with the requested hold duration.

The harness starts/stops its own loopback relay and fake HTTP authorizer; no
user account or project database is used. Static discovery and plaintext
transport are confined to host/guest NAT. This does not verify the full
login/sharing UI, dynamic allocation, TLS, relay failover,
Windows or independent physical-host performance. Those rows remain open.
The measured scene is static and mostly solid color; this is continuity
evidence, not a benchmark for video or many concurrent desktops.

When testing ends, shut down only this VM gracefully, for example by running
`sudo poweroff` inside the guest. Its disk can be retained for reproducibility;
do not delete any lab VM to reclaim space.
