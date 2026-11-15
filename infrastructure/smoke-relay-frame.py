"""Run the Qt Relay frame smoke test against a disposable loopback stack."""

import argparse
import http.client
import json
import os
from pathlib import Path
import subprocess
import tempfile
import uuid


def post(port, route, data, *, token=None, expected=200):
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    try:
        connection.request("POST", route, json.dumps(data), headers)
        response = connection.getresponse()
        if response.status != expected:
            raise RuntimeError(f"POST {route} returned {response.status}, expected {expected}")
        return json.load(response).get("data")
    finally:
        connection.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path, help="Path to AdminScreenViewerTest")
    parser.add_argument("--api-port", type=int, default=19090)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    port = args.api_port
    name = "jpeg" + uuid.uuid4().hex[:12]
    password = uuid.uuid4().hex
    admin_credentials = {"username": name, "password": password}
    post(port, "/api/v1/auth/register", admin_credentials, expected=201)
    admin = post(port, "/api/v1/auth/login", admin_credentials)
    post(port, "/api/v1/child/register", {"childUsername": "-child", "password": password},
         token=admin["token"])
    env = dict(os.environ, PBL4_FRAME_SMOKE="1",
               PBL4_SMOKE_ADMIN_USERNAME=name, PBL4_SMOKE_CHILD_USERNAME=name + "-child",
               PBL4_SMOKE_PASSWORD=password, REMOTE_API_URL=f"http://127.0.0.1:{port}",
               QT_QPA_PLATFORM="offscreen")
    with tempfile.TemporaryDirectory(prefix="pbl4-frame-") as directory:
        results = Path(directory) / "test.txt"
        run = subprocess.run([str(executable), "loopbackRelayDeliversSyntheticFrame",
                              "-o", f"{results},txt"], env=env, timeout=75)
        if results.exists():
            print(results.read_text(encoding="utf-8"), end="")
        return run.returncode


if __name__ == "__main__":
    raise SystemExit(main())
