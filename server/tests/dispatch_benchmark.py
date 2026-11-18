"""Measure Redis relay allocation with one and two nodes on disposable Redis.

TEST_DISPOSABLE_REDIS=1 python3 server/tests/dispatch_benchmark.py --host 172.x.x.x
Uses Redis DB 15 only when empty, and clears it on exit. This measures the
dispatch Lua script, not screen throughput or relay CPU usage.
"""

import argparse
import json
import math
import os
import socket
import statistics
import time
from pathlib import Path


SCRIPTS = Path(__file__).resolve().parents[1] / "management-api/src/main/resources/relay"


class Redis:
    def __init__(self, host, port):
        self.socket = socket.create_connection((host, port), timeout=5)
        self.socket.settimeout(5)
        self.stream = self.socket.makefile("rb")

    def close(self):
        self.stream.close()
        self.socket.close()

    def call(self, *args):
        command = b"*" + str(len(args)).encode() + b"\r\n"
        for arg in args:
            value = arg if isinstance(arg, bytes) else str(arg).encode()
            command += b"$" + str(len(value)).encode() + b"\r\n" + value + b"\r\n"
        self.socket.sendall(command)
        return self.read()

    def read(self):
        prefix = self.stream.read(1)
        line = self.stream.readline().rstrip(b"\r\n")
        if prefix == b"+":
            return line.decode()
        if prefix == b"-":
            raise RuntimeError(line.decode())
        if prefix == b":":
            return int(line)
        if prefix == b"$":
            size = int(line)
            return None if size < 0 else self.stream.read(size + 2)[:-2]
        raise RuntimeError("Invalid Redis response")


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)]


def run_case(redis, register_sha, allocate_sha, nodes, count, round_number):
    redis.call("FLUSHDB")
    for index in range(nodes):
        node = {"instanceId": f"bench-{index}", "bootId": f"round-{round_number}",
                "advertisedHost": "127.0.0.1", "advertisedPort": 19091 + index,
                "capacity": max(1000, count)}
        assert redis.call("EVALSHA", register_sha, 1, "relay:nodes", json.dumps(node), 30000) == 1
    ids = [f"benchmark-{round_number}-{index}" for index in range(count)]
    for child_id in ids:
        redis.call("SET", "auth:child:" + child_id, "test", "PX", 60000)
    times = []
    distribution = {f"bench-{index}": 0 for index in range(nodes)}
    for child_id in ids:
        started = time.perf_counter_ns()
        response = redis.call("EVALSHA", allocate_sha, 3, "relay:nodes",
                              "relay:allocation:" + child_id, "auth:child:" + child_id,
                              child_id, 45000)
        times.append((time.perf_counter_ns() - started) / 1_000_000)
        distribution[json.loads(response)["instanceId"]] += 1
    assert sum(distribution.values()) == count
    return times, distribution


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=6379)
    parser.add_argument("--count", type=int, default=300)
    parser.add_argument("--rounds", type=int, default=3)
    args = parser.parse_args()
    if os.environ.get("TEST_DISPOSABLE_REDIS") != "1":
        parser.error("Set TEST_DISPOSABLE_REDIS=1 only for a disposable Redis instance")
    if args.count < 2 or args.rounds < 1:
        parser.error("--count must be >= 2 and --rounds must be >= 1")
    redis = Redis(args.host, args.port)
    clean = False
    try:
        redis.call("SELECT", 15)
        if redis.call("DBSIZE") != 0:
            raise RuntimeError("Refusing to benchmark: Redis DB 15 is not empty")
        clean = True
        register_sha = redis.call("SCRIPT", "LOAD", (SCRIPTS / "register.lua").read_bytes())
        allocate_sha = redis.call("SCRIPT", "LOAD", (SCRIPTS / "allocate.lua").read_bytes())
        results = {1: ([], []), 2: ([], [])}
        for round_number in range(args.rounds):
            for nodes in ((1, 2) if round_number % 2 == 0 else (2, 1)):
                times, distribution = run_case(redis, register_sha, allocate_sha,
                                               nodes, args.count, round_number)
                results[nodes][0].extend(times)
                results[nodes][1].append(distribution)
        for nodes, (samples, distributions) in results.items():
            print(json.dumps({"nodes": nodes, "allocations": len(samples),
                              "p50_ms": round(statistics.median(samples), 3),
                              "p95_ms": round(percentile(samples, 0.95), 3),
                              "mean_ms": round(statistics.mean(samples), 3),
                              "distribution_each_round": distributions}))
    finally:
        if clean:
            redis.call("FLUSHDB")
        redis.close()


if __name__ == "__main__":
    main()
