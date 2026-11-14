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
and 200%. No host/guest relay session has been accepted yet.

When testing ends, shut down only this VM gracefully, for example by running
`sudo poweroff` inside the guest. Its disk can be retained for reproducibility;
do not delete any lab VM to reclaim space.
