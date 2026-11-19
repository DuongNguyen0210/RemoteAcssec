# Real-time CHILD CPU/RAM/Network

## User-visible behavior

When CHILD enables sharing, its authorized owner sees live CPU%, physical RAM
used/total (GiB and%), network receive/send rates (B/s, KiB/s, MiB/s) on each
card in the ADMIN Devices page. No remote viewer needs to be opened. CHILD
starts with sharing off as before. Stopping sharing/logout removes telemetry;
loss of publisher heartbeat removes the entire presence after20seconds.

A fresh sample is collected every2seconds while sharing is enabled. Existing
SSE publishes the current owned device snapshot every3seconds, so normal update
latency is2–5seconds plus network latency. Cards clear measured values and show
“Mất cập nhật số liệu” after10seconds without a newer sample. An unavailable
measurement shows an em dash; it is never fabricated as zero. CPU/network need
two OS readings: the first/warmup sample can contain only RAM. Counters going
backward or network interfaces changing reset the affected delta for one sample.

Network is actual aggregate machine traffic, not an Internet speed test or
per-process/RemoteAccess-only bandwidth. Prefer active physical adapters to
avoid counting packets again at VPN/virtual layers; on a VM with no active
physical adapter, use active virtual adapters. Loopback and tunnel interfaces
are excluded on Windows. Memory is total physical minus currently available,
not committed/process memory. Task Manager counters can differ because sampling
intervals and definitions differ; compare trends and the corresponding adapter.

## Integration and security

Built from origin/main ce82f54 plus user-approved security commit d92abc6 (PR42).
PR42 remains unmerged; no main merge was performed. Remote origin branches and
open PRs were checked: only a reserved TELEMETRY0x50 enum existed, no collector
or CPU/RAM/network pipeline to reuse. This implementation extends the existing
CHILD heartbeat -> Redis presence -> authorized REST/SSE -> DeviceService/store
-> ADMIN card flow. It does not create a second session-scoped transport or a
new analytics/history service. The reserved RDTP enum remains unused.

Heartbeat telemetry is optional for older clients. No PostgreSQL migration,
JWT/session/RDTP version changes or permission bypass. Owner/role checks, private
Redis keys and20second TTL are reused. One latest sample per session, no history.
CHILD cannot choose owner, reception timestamp or another session via payload.
CPU must be finite0..100; byte/rate/sequence ranges and field pairs are validated
before persistence. Missing OS counters remain null. Values are untrusted client
measurements, not tamper-proof host attestation.

The Redis atomic heartbeat rejects stale/duplicate telemetry sequences without
replacing values, reception timestamp or extending presence TTL. New valid JSON
is stored verbatim, avoiding Lua cjson numeric re-encoding and lost byte precision.
A new login gets a new session/generation; stop/resume keeps increasing sequence.
Older-client heartbeats without telemetry clear previous metrics, preserving
compatibility. Server supplies sample age; UI adds local monotonic elapsed time,
so CHILD/ADMIN wall-clock offsets do not make stale metrics appear fresh.

Native collection: Windows GetSystemTimes, GlobalMemoryStatusEx and GetIfTable2;
Linux /proc/stat, /proc/meminfo and /proc/net/dev with sysfs adapter IDs. Other
platforms report unavailable. No privileged shell commands, passwords, input
injection, blocking sleeps or screen capture in the collector. Windows64+CPU
processor-group behavior follows GetSystemTimes; this change is not a multi-group
high-core-count performance profiler. Linux fallback needs platform acceptance
if the test evidence only records Windows execution.

Windows API references:
- [GetSystemTimes (kernel time includes idle)](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes)
- [Physical memory definitions](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-memorystatusex)
- [Network counters and interface flags](https://learn.microsoft.com/en-us/windows/win32/api/netioapi/ns-netioapi-mib_if_row2)

## Build and run after pull

JDK17/Maven, Qt6 (Core/Gui/Widgets/Network/Concurrent/Svg/Test/Linguist), compatible
C++17 compiler, CMake/Ninja, Docker Desktop and OpenSSL. Windows IP Helper is an
OS library linked automatically; there is no new third-party runtime dependency.

~~~powershell
mvn -f server/pom.xml test
cmake -S clients -B build -G Ninja -DBUILD_TESTING=ON -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --timeout 120
$runtime = 'C:\remoteacssec-private\telemetry'
.\infrastructure\start-secure-development.ps1 -RuntimeDirectory $runtime -ProjectName pbl4-security-telemetry -ApiPort 19590 -OpenSsl 'C:\Program Files\Git\usr\bin\openssl.exe'
$env:REMOTE_API_URL = 'https://localhost:19590'
$env:REMOTE_API_CA_CERT = Join-Path $runtime 'server.crt'
$env:REMOTE_RELAY_CA_CERT = $env:REMOTE_API_CA_CERT
$env:REMOTE_RELAY_TLS = 'required'
$env:REMOTE_RELAY_MODE = 'dynamic'
.\build\RemoteAccessApp\RemoteAccessApp.exe
~~~

Use your actual Qt/compiler paths. Development certificate islocalhost-only,
30days; no OS trust-store import needed. Private runtime must stay outside source.
Use a real hostname/CA/advertised Relay hosts for deployment on another machine.
Do not point destructive/integration fixtures at a real database/Redis.

## Automated acceptance

- TelemetryFlowTest: counter deltas, first/reset/zero/missing samples, strict JSON,
  actual native counters, UI units/aging, heartbeat payload/start/stop/resume.
- TelemetryValidationTest: optional/warmup/zero and non-finite/out-of-range pairs.
- TelemetryRedisIT on private19593: concurrent samples keep highest sequence,
  delayed replay cannot refresh data, exact integer byte counts, ownership,
  stop/logout/new generation/old-client compatibility; existing RedisIT regression.
- smoke-telemetry.py on a private TLS stack: real API validation/Redis/REST/SSE,
  owner/role separation, real20second expiry, recovery, stop/resume and relogin.
- Optional real Qt live test uses separate child and ADMIN processes. CHILD uses
  actual OS counters via production HeartbeatReporter; ADMIN uses production
  DeviceService/DevicesController/store/UI labels, with automatic SSE updates.
  Runs two CHILD logins to verify removal and new generation; no native input or
  screen capture is started. Opt-in cases skip during normal unit-only CTest.
- Existing full Java/Qt, HTTPS/TLS and real Relay synthetic-frame security regression.

~~~powershell
python infrastructure/smoke-telemetry.py --ca "$runtime/server.crt" --api-port 19590 --qt-test build/RemoteAccessApp/TelemetryFlowTest.exe
~~~

Real two-Windows-PC CPU/load/network changes and Internet/NAT behavior remain
manual acceptance; two processes on one host do not prove a two-machine deployment.
