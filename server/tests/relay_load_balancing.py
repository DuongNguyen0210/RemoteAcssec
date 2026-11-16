"""End-to-end test against a disposable two-relay Docker stack, never production.

TEST_API_URL=http://127.0.0.1:18089 python3 server/tests/relay_load_balancing.py
Creates a temporary admin and child account; use a disposable database.
"""
import json
import os
import socket
import struct
import uuid
import urllib.error
import urllib.request
import urllib.parse
from contextlib import ExitStack

def require_test_target(url):
    host = urllib.parse.urlparse(url).hostname
    if host not in ("127.0.0.1", "localhost", "::1") and os.environ.get("ALLOW_REMOTE_TEST_TARGET") != "1":
        raise SystemExit("Refusing to create test accounts on a remote API. Set ALLOW_REMOTE_TEST_TARGET=1 for an isolated remote stack.")

API = os.environ.get("TEST_API_URL", "http://127.0.0.1:18089")
require_test_target(API)


def request(method, path, token=None, data=None, expected=200):
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    req = urllib.request.Request(API + "/api/v1/" + path, headers=headers, method=method,
            data=json.dumps(data).encode() if data is not None else None)
    try:
        response = urllib.request.urlopen(req, timeout=10)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        body = response.read()
        assert response.status == expected, (path, response.status, body)
        return json.loads(body) if body else None


def credentials(token, target=None):
    def field(value):
        raw = value.encode()
        return struct.pack("!H", len(raw)) + raw
    return field(token) + (field(target) if target is not None else b"")


def packet(kind, session=0, payload=b"", flags=0, sequence=0):
    return struct.pack("!IBBHIQI", 0x52445450, 2, kind, flags, len(payload), session, sequence) + payload


def read(sock):
    def exact(size):
        data = b""
        while len(data) < size:
            part = sock.recv(size - len(data))
            assert part, "Unexpected relay disconnect"
            data += part
        return data
    header = exact(24)
    length = struct.unpack("!IBBHIQI", header)[4]
    return header + exact(length)


def main():
    owner = "lb" + uuid.uuid4().hex[:12]
    password = "test-password"
    request("POST", "auth/register", data={"username": owner, "password": password}, expected=201)
    admin = request("POST", "auth/login", data={"username": owner, "password": password})["data"]["token"]
    children = []
    for index in range(2):
        suffix = "child" + str(index)
        request("POST", "child/register", admin, {"childUsername": suffix, "password": password})
        children.append(request("POST", "auth/login", data={"username": owner + suffix, "password": password})["data"])
    endpoints = []
    for index, child in enumerate(children):
        request("POST", "child/heartbeat", child["token"], {"hostname": "test-" + str(index), "os": "Linux"})
        endpoint = request("POST", "relay/allocations", child["token"], {})["data"]
        endpoints.append(endpoint)
        assert request("POST", "relay/allocations", child["token"], {})["data"]["instanceId"] == endpoint["instanceId"]
        assert request("GET", "devices/" + child["sessionId"] + "/relay", admin)["data"]["port"] == endpoint["port"]
    assert endpoints[0]["instanceId"] != endpoints[1]["instanceId"], endpoints
    request("POST", "relay/allocations", admin, {}, expected=403)
    with ExitStack() as stack:
        connect = lambda endpoint: stack.enter_context(socket.create_connection((endpoint["host"], endpoint["port"]), timeout=5))
        wrong = connect(endpoints[1])
        wrong.sendall(packet(1, payload=credentials(children[0]["token"])))
        assert read(wrong) == packet(2, payload=b"\0"), "Child registered on an unassigned node"
        for child, endpoint in zip(children, endpoints):
            agent = connect(endpoint)
            controller = connect(endpoint)
            agent.sendall(packet(1, payload=credentials(child["token"])))
            assert read(agent) == packet(2, payload=b"\1")
            controller.sendall(packet(3, payload=credentials(admin, child["sessionId"])))
            invitation = read(agent)
            _, _, kind, _, _, session, _ = struct.unpack("!IBBHIQI", invitation[:24])
            assert kind == 4 and session > 0
            agent.sendall(packet(5, session))
            assert read(controller) == packet(7, session, b"\1")
            screen = packet(0x10, session, b"\x00\xffscreen\x00", flags=3, sequence=123)
            agent.sendall(screen)
            assert read(controller) == screen
            for kind in [0x20, 0x21, 0x22, 0x23, 0x30, 0x31]:
                payload = (struct.pack("!II", 0, 0) if kind == 0x20 else
                           struct.pack("!IIB", 0, 0, 1) if kind in (0x21, 0x22) else
                           struct.pack("!IIi", 0, 0, 120) if kind == 0x23 else
                           struct.pack("!I", 65))
                event = packet(kind, session, payload, sequence=456)
                controller.sendall(event)
                assert read(agent) == event
    print("PASS: two-node allocation, affinity, authorization, screen and all mouse/key packet types")


if __name__ == "__main__":
    main()
