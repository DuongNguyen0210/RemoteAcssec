"""Verify host ADMIN -> real Java relay -> X11 CHILD in a disposable SSH guest.

Uses the benchmark's loopback-only fake authorizer, not real account tokens.
The guest must contain DesktopRelayAcceptance and run_x11_integration.py.
"""
import argparse
import http.server
import ipaddress
import os
from pathlib import Path
import shlex
import subprocess
import threading
import tempfile
import time
import uuid
from relay_transport_benchmark import Authorizer, JAR, LIB, free_port, wait_for_relay

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--admin-binary", required=True)
parser.add_argument("--ssh-key", required=True)
parser.add_argument("--known-hosts", required=True)
parser.add_argument("--ssh-port", type=int, default=22224)
parser.add_argument("--guest-user", default="pbl4")
parser.add_argument("--guest-relay-host", default="10.0.2.2")
parser.add_argument("--guest-binary", default="/home/pbl4/build/DesktopRelayAcceptance")
parser.add_argument("--guest-runner", default="/home/pbl4/source/clients/RemoteAccessApp/tests/run_x11_integration.py")
parser.add_argument("--log-dir", required=True)
parser.add_argument("--duration-seconds", type=int, choices=range(0, 301), default=0, metavar="0..300")
parser.add_argument("--tls", action="store_true", help="Require verified TLS on both clients")
args = parser.parse_args()
logs = Path(args.log_dir)
logs.mkdir(parents=True, exist_ok=True)
auth = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Authorizer)
threading.Thread(target=auth.serve_forever, daemon=True).start()
processes = []
handles = []
tls_dir = tempfile.TemporaryDirectory(prefix="pbl4-relay-tls-")
remote_ca = None
ssh = ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5",
       "-o", f"UserKnownHostsFile={args.known_hosts}", "-i", args.ssh_key,
       "-p", str(args.ssh_port), f"{args.guest_user}@127.0.0.1"]
try:
    port = free_port()
    agent = str(uuid.uuid4())
    env = dict(os.environ, RELAY_PORT=str(port), RELAY_BIND_HOST="127.0.0.1",
               RELAY_DISCOVERY_ENABLED="false", RELAY_TLS_CERT="", RELAY_TLS_KEY="",
               MANAGEMENT_API_URL=f"http://127.0.0.1:{auth.server_port}")
    mode = "required" if args.tls else "off"
    ca = ""
    if args.tls:
        cert = Path(tls_dir.name) / "cert.pem"
        key = Path(tls_dir.name) / "key.pem"
        try:
            ipaddress.ip_address(args.guest_relay_host)
            guest_san = "IP:" + args.guest_relay_host
        except ValueError:
            guest_san = "DNS:" + args.guest_relay_host
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                        "-sha256", "-days", "1", "-subj", "/CN=localhost", "-addext",
                        f"subjectAltName=DNS:localhost,IP:127.0.0.1,{guest_san}",
                        "-keyout", str(key), "-out", str(cert)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=15)
        key.chmod(0o600)
        env.update(RELAY_TLS_CERT=str(cert), RELAY_TLS_KEY=str(key))
        ca = str(cert)
        remote_ca = f"/tmp/pbl4-acceptance-ca-{agent}.pem"
        subprocess.run(ssh + ["umask 077; cat > " + shlex.quote(remote_ca)],
                       input=cert.read_bytes(), check=True, timeout=10)
    relay_log = (logs / "relay.log").open("w")
    handles.append(relay_log)
    relay = subprocess.Popen(["java", "-cp", f"{JAR}{os.pathsep}{LIB}/*",
                              "com.remotecontrol.relay.server.RelayServer"],
                             env=env, stdout=relay_log, stderr=subprocess.STDOUT)
    processes.append(relay)
    wait_for_relay(relay, port)
    remote = shlex.join(["env", "PBL4_DESKTOP_ROLE=child", f"PBL4_DESKTOP_AGENT={agent}",
                         f"REMOTE_RELAY_HOST={args.guest_relay_host}", f"REMOTE_RELAY_PORT={port}",
                         f"PBL4_DESKTOP_DURATION={args.duration_seconds}",
                         f"REMOTE_RELAY_TLS={mode}", f"REMOTE_RELAY_CA_CERT={remote_ca or ''}", "python3", args.guest_runner,
                         "--timeout-seconds", str(40 + args.duration_seconds), args.guest_binary])
    child_log = (logs / "child.log").open("w")
    handles.append(child_log)
    child = subprocess.Popen(ssh + [remote],
                             stdout=child_log, stderr=subprocess.STDOUT)
    processes.append(child)
    deadline = time.monotonic() + 20
    while "CHILD_READY" not in (logs / "child.log").read_text():
        if child.poll() is not None:
            raise RuntimeError("CHILD exited before registration; inspect child.log")
        if time.monotonic() >= deadline:
            raise TimeoutError("CHILD did not register within 20 seconds")
        time.sleep(0.1)
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", PBL4_DESKTOP_ROLE="admin",
               PBL4_DESKTOP_DURATION=str(args.duration_seconds),
               PBL4_DESKTOP_AGENT=agent, REMOTE_RELAY_HOST="127.0.0.1",
               REMOTE_RELAY_PORT=str(port), REMOTE_RELAY_TLS=mode, REMOTE_RELAY_CA_CERT=ca)
    with (logs / "admin.log").open("w") as output:
        subprocess.run([args.admin_binary], env=env, stdout=output,
                       stderr=subprocess.STDOUT, timeout=40 + args.duration_seconds, check=True)
    if child.wait(timeout=10) != 0:
        raise RuntimeError("CHILD failed after ADMIN disconnected")
    admin_text = (logs / "admin.log").read_text()
    for marker in ["SCREEN_RECEIVED", "REMOTE_CLICK_CONFIRMED", "REMOTE_KEY_CONFIRMED"]:
        if marker not in admin_text:
            raise RuntimeError(f"Missing confirmation: {marker}")
    if "CHILD_DISCONNECTED" not in (logs / "child.log").read_text():
        raise RuntimeError("CHILD did not observe session disconnect")
    for line in admin_text.splitlines():
        if line.startswith("STREAM_HOLD"):
            print(line)
    print(f"PASS: host/guest screen, native mouse, native keyboard and disconnect (TLS={mode})")
finally:
    for process in reversed(processes):
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
    for handle in handles:
        handle.close()
    auth.shutdown()
    auth.server_close()
    try:
        if remote_ca:
            result = subprocess.run(ssh + [shlex.join(["rm", "-f", remote_ca])], timeout=10, check=False)
            if result.returncode:
                print(f"Guest CA cleanup failed: remove {remote_ca} when SSH is available")
    except subprocess.TimeoutExpired:
        print(f"Guest CA cleanup timed out: remove {remote_ca} when SSH is available")
    finally:
        tls_dir.cleanup()
