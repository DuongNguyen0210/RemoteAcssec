"""Drive real login/sharing/viewer UI against a disposable API and relay stack."""
import argparse
import json
import os
from pathlib import Path
import secrets
import shlex
import subprocess
import tempfile
import time
import urllib.parse
import urllib.request
import uuid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--api-url", default="http://127.0.0.1:18089")
parser.add_argument("--guest-api-url", default="http://10.0.2.2:18089")
parser.add_argument("--admin-binary", required=True)
parser.add_argument("--ssh-key", required=True)
parser.add_argument("--known-hosts", required=True)
parser.add_argument("--ssh-port", type=int, default=22224)
parser.add_argument("--guest-user", default="pbl4")
parser.add_argument("--log-dir", required=True)
args = parser.parse_args()
if os.environ.get("TEST_DISPOSABLE_UI_STACK") != "1":
    raise SystemExit("Requires TEST_DISPOSABLE_UI_STACK=1; creates temporary accounts")
if urllib.parse.urlparse(args.api_url).hostname not in ("localhost", "127.0.0.1", "::1"):
    raise SystemExit("API must be a disposable loopback stack")

def request(path, data, token=None):
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    req = urllib.request.Request(args.api_url + "/api/v1/" + path,
                                 data=json.dumps(data).encode(), headers=headers)
    with urllib.request.urlopen(req, timeout=10) as response:
        return json.load(response)

logs = Path(args.log_dir)
logs.mkdir(parents=True, exist_ok=True)
owner = "ui" + uuid.uuid4().hex[:12]
password = secrets.token_hex(16)
request("auth/register", {"username": owner, "password": password})
token = request("auth/login", {"username": owner, "password": password})["data"]["token"]
request("child/register", {"childUsername": "child", "password": password}, token)
ssh = ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5",
       "-o", f"UserKnownHostsFile={args.known_hosts}", "-i", args.ssh_key,
       "-p", str(args.ssh_port), f"{args.guest_user}@127.0.0.1"]
remote_config = "/tmp/pbl4-ui-" + uuid.uuid4().hex
child = None
try:
    remote = shlex.join(["env", "PBL4_DESKTOP_ROLE=child", f"PBL4_UI_USERNAME={owner}child",
                         f"PBL4_UI_PASSWORD={password}", f"REMOTE_API_URL={args.guest_api_url}",
                         "REMOTE_RELAY_MODE=dynamic", "REMOTE_RELAY_TLS=off", "REMOTE_RELAY_CA_CERT=",
                         f"XDG_CONFIG_HOME={remote_config}", "python3",
                         "/home/pbl4/source/clients/RemoteAccessApp/tests/run_x11_integration.py",
                         "--timeout-seconds", "100", "/home/pbl4/build/FullUiDesktopAcceptance"])
    with (logs / "child.log").open("w") as output:
        child = subprocess.Popen(ssh + [remote], stdout=output, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 45
        while "CHILD_UI_READY" not in (logs / "child.log").read_text():
            if child.poll() is not None:
                raise RuntimeError("CHILD UI failed; inspect child.log")
            if time.monotonic() >= deadline:
                raise TimeoutError("CHILD UI was not ready within 45 seconds")
            time.sleep(0.1)
        with tempfile.TemporaryDirectory(prefix="pbl4-ui-admin-") as config:
            env = dict(os.environ, QT_QPA_PLATFORM="offscreen", XDG_CONFIG_HOME=config,
                       PBL4_DESKTOP_ROLE="admin", PBL4_UI_USERNAME=owner, PBL4_UI_PASSWORD=password,
                       REMOTE_API_URL=args.api_url, REMOTE_RELAY_MODE="dynamic",
                       REMOTE_RELAY_TLS="off", REMOTE_RELAY_CA_CERT="")
            with (logs / "admin.log").open("w") as admin_output:
                subprocess.run([args.admin_binary, "-nocrashhandler"], env=env, stdout=admin_output,
                               stderr=subprocess.STDOUT, check=True, timeout=90)
        if child.wait(timeout=20) != 0:
            raise RuntimeError("CHILD UI did not pass")
    for filename, markers in {
        "child.log": ["CHILD_UI_READY", "CHILD_LOCAL_STOP_CONFIRMED", "CHILD_REPEAT_CONFIRMED"],
        "admin.log": ["ADMIN_UI_INPUT_CONFIRMED", "ADMIN_CHILD_STOP_CONFIRMED", "ADMIN_REPEAT_CONFIRMED"],
    }.items():
        text = (logs / filename).read_text()
        if any(marker not in text for marker in markers):
            raise RuntimeError("Missing UI acceptance confirmation in " + filename)
    print("PASS: real UI login and two distinct dynamic relay sessions with native input and CHILD stop/re-enable")
finally:
    if child is not None and child.poll() is None:
        child.terminate()
        try:
            child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait()
    subprocess.run(ssh + [shlex.join(["rm", "-rf", remote_config])], timeout=10, check=False)
