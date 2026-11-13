"""Disconnect one relay from an isolated Compose network and verify reassignment.

TEST_API_URL=http://127.0.0.1:18089 TEST_COMPOSE_PROJECT=pbl4-validation \
TEST_COMPOSE_ENV=/tmp/pbl4-validation.env python3 server/tests/relay_failover.py
"""
import os
import subprocess
import sys
import time
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from relay_load_balancing import request

project = os.environ.get("TEST_COMPOSE_PROJECT", "")
env_file = os.environ.get("TEST_COMPOSE_ENV", "")
if not project.startswith("pbl4-validation") or not Path(env_file).is_file():
    raise SystemExit("Use an isolated pbl4-validation Compose project and TEST_COMPOSE_ENV")

owner = "fail" + uuid.uuid4().hex[:12]
password = "test-password"
request("POST", "auth/register", data={"username": owner, "password": password}, expected=201)
admin = request("POST", "auth/login", data={"username": owner, "password": password})["data"]["token"]
request("POST", "child/register", admin, {"childUsername": "child", "password": password})
child = request("POST", "auth/login", data={"username": owner + "child", "password": password})["data"]
request("POST", "child/heartbeat", child["token"], {"hostname": "failover-test", "os": "Linux"})
first = request("POST", "relay/allocations", child["token"], {})["data"]["instanceId"]
if first not in {"relay-proxy-1", "relay-proxy-2"}:
    raise AssertionError(first)
print("Initially assigned:", first, flush=True)
container = f"{project}-{first}-1"
network = f"{project}_default"
subprocess.run(["docker", "network", "disconnect", network, container], check=True)
try:
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        time.sleep(2)
        request("POST", "child/heartbeat", child["token"], {"hostname": "failover-test", "os": "Linux"})
        try:
            current = request("POST", "relay/allocations", child["token"], {})["data"]["instanceId"]
        except (AssertionError, KeyError, TypeError):
            continue
        print("Allocation after network loss:", current, flush=True)
        if current != first:
            print("PASS: reassigned from", first, "to", current)
            break
    else:
        raise AssertionError("CHILD was not reassigned within 45 seconds")
finally:
    subprocess.run(["docker", "network", "connect", network, container], check=True)
