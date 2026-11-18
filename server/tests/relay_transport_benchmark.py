"""Benchmark real relay forwarding with one and two local Java processes.

Build first: mvn -q -f server/pom.xml -pl relay-proxy -am package -DskipTests
Then: mvn -q -f server/relay-proxy/pom.xml dependency:copy-dependencies -DincludeScope=runtime
No database is used. A loopback test authorizer accepts only generated tokens.
Results describe this host and workload, not Internet or two-host performance.
"""

import argparse
import concurrent.futures
import http.server
import json
import math
import os
import socket
import statistics
import struct
import subprocess
import threading
import time
import uuid
from contextlib import ExitStack
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
JAR = ROOT / "relay-proxy/target/relay-proxy-1.0.0-SNAPSHOT.jar"
LIB = ROOT / "relay-proxy/target/dependency"


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


class Authorizer(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        token = self.headers.get("Authorization", "").removeprefix("Bearer ")
        if self.path == "/api/v1/relay/agent" and token.startswith("bench-child:"):
            agent = token.removeprefix("bench-child:")
        elif self.path.startswith("/api/v1/relay/targets/") and token == "bench-admin":
            agent = self.path.rsplit("/", 1)[-1]
        else:
            agent = ""
        try:
            agent = str(uuid.UUID(agent))
        except ValueError:
            self.send_error(403)
            return
        body = agent.encode()
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_):
        pass


def field(value):
    encoded = value.encode()
    return struct.pack("!H", len(encoded)) + encoded


def packet(kind, session=0, payload=b"", sequence=0):
    return struct.pack("!IBBHIQI", 0x52445450, 2, kind, 0, len(payload), session, sequence) + payload


def receive(sock):
    def exact(size):
        result = bytearray()
        while len(result) < size:
            chunk = sock.recv(size - len(result))
            if not chunk:
                raise ConnectionError("Relay closed before forwarding completed")
            result.extend(chunk)
        return bytes(result)
    header = exact(24)
    length = struct.unpack("!IBBHIQI", header)[4]
    return header + exact(length)


def connect_session(stack, port):
    child_id = str(uuid.uuid4())
    child = stack.enter_context(socket.create_connection(("127.0.0.1", port), timeout=10))
    admin = stack.enter_context(socket.create_connection(("127.0.0.1", port), timeout=10))
    child.settimeout(10)
    admin.settimeout(10)
    child.sendall(packet(1, payload=field("bench-child:" + child_id)))
    assert receive(child) == packet(2, payload=b"\1")
    admin.sendall(packet(3, payload=field("bench-admin") + field(child_id)))
    invitation = receive(child)
    _, _, kind, _, _, session, _ = struct.unpack("!IBBHIQI", invitation[:24])
    assert kind == 4 and session > 0
    child.sendall(packet(5, session))
    assert receive(admin) == packet(7, session, b"\1")
    return child, admin, session


def wait_for_relay(process, port):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("Relay exited before listening")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise TimeoutError("Relay did not listen within 10 seconds")


def linux_usage(process):
    stat_path = Path(f"/proc/{process.pid}/stat")
    status_path = Path(f"/proc/{process.pid}/status")
    if not stat_path.is_file() or not status_path.is_file():
        return None
    fields = stat_path.read_text().rsplit(") ", 1)[1].split()
    cpu_seconds = (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")
    rss_kib = next(int(line.split()[1]) for line in status_path.read_text().splitlines()
                   if line.startswith("VmRSS:"))
    return cpu_seconds, rss_kib / 1024


def worker(child, admin, session, payload, frames, interval_ms, barrier):
    barrier.wait()
    latencies = []
    next_frame = time.monotonic()
    for index in range(frames):
        if interval_ms:
            time.sleep(max(0, next_frame - time.monotonic()))
            next_frame += interval_ms / 1000
        data = packet(0x10, session, payload, index)
        started = time.perf_counter_ns()
        child.sendall(data)
        assert receive(admin) == data
        latencies.append((time.perf_counter_ns() - started) / 1_000_000)
    return latencies


def benchmark(ports, processes, sessions, frames, payload, interval_ms):
    with ExitStack() as stack:
        pairs = [connect_session(stack, ports[index % len(ports)]) for index in range(sessions)]
        before = [linux_usage(process) for process in processes]
        barrier = threading.Barrier(sessions + 1)
        with concurrent.futures.ThreadPoolExecutor(max_workers=sessions) as pool:
            futures = [pool.submit(worker, child, admin, session, payload, frames, interval_ms, barrier)
                       for child, admin, session in pairs]
            barrier.wait()
            started = time.perf_counter()
            samples = [latency for future in futures for latency in future.result()]
            elapsed = time.perf_counter() - started
        after = [linux_usage(process) for process in processes]
    ordered = sorted(samples)
    result = {"relays": len(ports), "sessions": sessions, "frames": sessions * frames,
            "payload_bytes": len(payload), "interval_ms": interval_ms,
            "seconds": round(elapsed, 3),
            "throughput_mib_s": round(sessions * frames * len(payload) / elapsed / 1048576, 2),
            "frame_p50_ms": round(statistics.median(samples), 3),
            "frame_p95_ms": round(ordered[math.ceil(0.95 * len(ordered)) - 1], 3)}
    if all(usage is not None for usage in before + after):
        result["relay_cpu_seconds"] = round(sum(end[0] - start[0]
                                                 for start, end in zip(before, after)), 3)
        result["relay_rss_mib_after"] = round(sum(usage[1] for usage in after), 1)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sessions", type=int, default=12)
    parser.add_argument("--frames", type=int, default=30)
    parser.add_argument("--payload-bytes", type=int, default=65536)
    parser.add_argument("--interval-ms", type=int, default=0,
                        help="Delay between frames for a paced stream (for example 200 for 5 FPS)")
    args = parser.parse_args()
    if args.sessions < 2 or args.frames < 1 or not 1 <= args.payload_bytes <= 524288 or args.interval_ms < 0:
        parser.error("Use at least two sessions, one frame, and a payload of 1..524288 bytes")
    if not JAR.is_file() or not LIB.is_dir():
        parser.error("Build the relay jar and copy its runtime dependencies first (see module docstring)")
    auth = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Authorizer)
    auth_thread = threading.Thread(target=auth.serve_forever, daemon=True)
    auth_thread.start()
    processes = []
    try:
        ports = []
        for index in range(2):
            port = free_port()
            env = dict(os.environ, RELAY_PORT=str(port), RELAY_BIND_HOST="127.0.0.1",
                       RELAY_INSTANCE_ID=f"bench-{index}",
                       RELAY_DISCOVERY_ENABLED="false", RELAY_TLS_CERT="", RELAY_TLS_KEY="",
                       MANAGEMENT_API_URL=f"http://127.0.0.1:{auth.server_port}")
            process = subprocess.Popen(["java", "-cp", f"{JAR}{os.pathsep}{LIB}/*",
                                        "com.remotecontrol.relay.server.RelayServer"], env=env,
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            processes.append(process)
            wait_for_relay(process, port)
            ports.append(port)
        payload = b"X" * args.payload_bytes
        for scenario in (ports[:1], ports):
            print(json.dumps(benchmark(scenario, processes[:len(scenario)],
                                       args.sessions, args.frames, payload,
                                       args.interval_ms)), flush=True)
    finally:
        for process in processes:
            process.terminate()
        for process in processes:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        auth.shutdown()
        auth.server_close()


if __name__ == "__main__":
    main()
