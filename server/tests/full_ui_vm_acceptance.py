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
import urllib.error
import uuid
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--api-url", default="http://127.0.0.1:18089")
parser.add_argument("--guest-api-url", default="http://10.0.2.2:18089")
parser.add_argument("--admin-binary", required=True)
parser.add_argument("--ssh-key", required=True)
parser.add_argument("--known-hosts", required=True)
parser.add_argument("--ssh-port", type=int, default=22224)
parser.add_argument("--guest-user", default="pbl4")
parser.add_argument("--log-dir", required=True)
parser.add_argument("--failover-project", help="Stop the assigned relay in this disposable pbl4-* Compose project")
args = parser.parse_args()
if os.environ.get("TEST_DISPOSABLE_UI_STACK") != "1":
    raise SystemExit("Requires TEST_DISPOSABLE_UI_STACK=1; creates temporary accounts")
if urllib.parse.urlparse(args.api_url).hostname not in ("localhost", "127.0.0.1", "::1"):
    raise SystemExit("API must be a disposable loopback stack")
if args.failover_project and not re.fullmatch(r"pbl4-[a-z0-9-]+", args.failover_project):
    raise SystemExit("Failover project must be a disposable pbl4-* project")

def request(path, data, token=None, method=None):
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    req = urllib.request.Request(args.api_url + "/api/v1/" + path,
                                 data=json.dumps(data).encode() if data is not None else None, headers=headers, method=method)
    with urllib.request.urlopen(req, timeout=10) as response:
        return json.load(response)

logs = Path(args.log_dir)
logs.mkdir(parents=True, exist_ok=True)
owner = "ui" + uuid.uuid4().hex[:12]
password = secrets.token_hex(16)
request("auth/register", {"username": owner, "password": password})
token = request("auth/login", {"username": owner, "password": password})["data"]["token"]
request("child/register", {"childUsername": "child", "password": password}, token)
# Release setup login before the real ADMIN UI claims this same account.
request("auth/logout", {}, token)
ssh = ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5",
       "-o", f"UserKnownHostsFile={args.known_hosts}", "-i", args.ssh_key,
       "-p", str(args.ssh_port), f"{args.guest_user}@127.0.0.1"]
remote_config = "/tmp/pbl4-ui-" + uuid.uuid4().hex
child = None
try:
    remote = shlex.join(["env", "PBL4_DESKTOP_ROLE=child", f"PBL4_UI_USERNAME={owner}child",
                         "PBL4_UI_FAILOVER=" + ("1" if args.failover_project else "0"),
                         f"PBL4_UI_PASSWORD={password}", f"REMOTE_API_URL={args.guest_api_url}",
                         "REMOTE_RELAY_MODE=dynamic", "REMOTE_RELAY_TLS=off", "REMOTE_RELAY_CA_CERT=",
                         f"XDG_CONFIG_HOME={remote_config}", "python3",
                         "/home/pbl4/source/clients/RemoteAccessApp/tests/run_x11_integration.py",
                         "--timeout-seconds", "260", "/home/pbl4/build/FullUiDesktopAcceptance"])
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
            recovery_marker = Path(config) / "relay-recovered"
            env = dict(os.environ, QT_QPA_PLATFORM="offscreen", XDG_CONFIG_HOME=config,
                       PBL4_DESKTOP_ROLE="admin", PBL4_UI_USERNAME=owner, PBL4_UI_PASSWORD=password,
                       REMOTE_API_URL=args.api_url, REMOTE_RELAY_MODE="dynamic",
                       REMOTE_RELAY_TLS="off", REMOTE_RELAY_CA_CERT="",
                       PBL4_UI_FAILOVER="1" if args.failover_project else "0",
                       PBL4_UI_RECOVERY_MARKER=str(recovery_marker),
                       PBL4_UI_TOKEN_FILE=str(Path(config) / "ui-session"))
            with (logs / "admin.log").open("w") as admin_output:
                admin = subprocess.Popen([args.admin_binary, "-nocrashhandler"], env=env,
                                         stdout=admin_output, stderr=subprocess.STDOUT)
                try:
                    deadline = time.monotonic() + 240
                    failed_node = None
                    recovery_start = None
                    while admin.poll() is None:
                        if time.monotonic() >= deadline:
                            raise TimeoutError("ADMIN UI did not finish within 240 seconds")
                        admin_text = (logs / "admin.log").read_text()
                        child_text = (logs / "child.log").read_text()
                        if args.failover_project and failed_node is None and "ADMIN_UI_INPUT_CONFIRMED" in admin_text:
                            token = (Path(config) / "ui-session").read_text()
                            devices = request("devices", None, token)["data"]
                            sid = next(d["sessionId"] for d in devices if d["username"] == owner + "child")
                            assigned = request(f"devices/{sid}/relay", None, token)["data"]
                            failed_node = assigned["instanceId"]
                            if failed_node not in ("relay-proxy-1", "relay-proxy-2"):
                                raise RuntimeError("Unexpected assigned relay instance")
                            container = f"{args.failover_project}-{failed_node}-1"
                            project = subprocess.check_output(["docker", "inspect", "--format",
                                '{{index .Config.Labels "com.docker.compose.project"}}', container], text=True).strip()
                            if project != args.failover_project:
                                raise RuntimeError("Refusing to stop a relay outside the test project")
                            recovery_start = time.monotonic()
                            subprocess.run(["docker", "stop", container], check=True, timeout=20,
                                           stdout=subprocess.DEVNULL)
                        if failed_node and not recovery_marker.exists() and "CHILD_RECOVERED" in child_text:
                            assigned = request(f"devices/{sid}/relay", None, token)["data"]
                            if assigned["instanceId"] == failed_node:
                                raise RuntimeError("CHILD recovered on the stopped node")
                            recovery_marker.touch()
                            print(f"RELAY_RECOVERED {failed_node} -> {assigned['instanceId']} in {time.monotonic()-recovery_start:.2f}s", flush=True)
                        time.sleep(0.1)
                    if admin.returncode:
                        raise RuntimeError("ADMIN UI failed; inspect admin.log")
                    if args.failover_project and (not failed_node or not recovery_marker.exists()):
                        raise RuntimeError("UI finished without verified relay reassignment")
                finally:
                    if admin.poll() is None:
                        admin.terminate()
                        try:
                            admin.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            admin.kill(); admin.wait()
        if child.wait(timeout=20) != 0:
            raise RuntimeError("CHILD UI did not pass")
    for filename, markers in {
        "child.log": ["CHILD_UI_READY", "CHILD_LOCAL_STOP_CONFIRMED", "CHILD_REPEAT_CONFIRMED"],
        "admin.log": ["ADMIN_UI_INPUT_CONFIRMED", "ADMIN_CHILD_STOP_CONFIRMED", "ADMIN_REPEAT_CONFIRMED"],
    }.items():
        text = (logs / filename).read_text()
        if any(marker not in text for marker in markers):
            raise RuntimeError("Missing UI acceptance confirmation in " + filename)
    # The UI released its login on normal shutdown; use a new cleanup session.
    token = request("auth/login", {"username": owner, "password": password})["data"]["token"]
    if request("devices", None, token)["data"]:
        raise RuntimeError("Stopped CHILD remains present after confirmed unshare")
    print("PRESENCE_UNSHARE_CONFIRMED", flush=True)
    old_session = request("auth/login", {"username": owner + "child", "password": password})["data"]
    request("child/heartbeat", {"hostname": "Credential reset check", "os": "Test"}, old_session["token"])
    account = next(c for c in request("child", None, token)["data"] if c["username"] == owner + "child")
    new_password = secrets.token_hex(16)
    request(f"child/{account['id']}", {"childUsername": "child", "newPassword": new_password}, token, method="PUT")
    for _ in range(4):
        try:
            request("child/heartbeat", {"hostname": "Revoked session"}, old_session["token"])
        except urllib.error.HTTPError as error:
            if error.code != 401:
                raise RuntimeError("Revoked CHILD session returned an unexpected status") from error
        else:
            raise RuntimeError("Password reset did not revoke an existing CHILD session")
    # Allow revoked relay authorization leases to drain before replacement.
    time.sleep(31)
    fresh_session = request("auth/login", {"username": owner + "child", "password": new_password})["data"]
    request("child/heartbeat", {"hostname": "Fresh session", "os": "Test"}, fresh_session["token"])
    request("child/logout", {}, fresh_session["token"])
    request("auth/logout", {}, token)
    print("PASSWORD_RESET_SESSION_REVOCATION_CONFIRMED", flush=True)
    if args.failover_project:
        print("PASS: real UI relay loss, reassignment to another node, native input in a new session and CHILD stop")
    else:
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
