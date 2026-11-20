"""Run only against a disposable localhost TLS Compose stack."""
import argparse
import concurrent.futures
import http.client
import json
import secrets
import ssl
import threading


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--api-port", type=int, required=True)
    parser.add_argument("--ca", required=True)
    args = parser.parse_args()
    trust = ssl.create_default_context(cafile=args.ca)
    prefix = "signup" + secrets.token_hex(6)
    password = "Test-" + secrets.token_hex(12)

    def request(method, route, data=None, token=None):
        headers = {"Content-Type": "application/json"}
        if token:
            headers["Authorization"] = "Bearer " + token
        conn = http.client.HTTPSConnection("localhost", args.api_port, context=trust, timeout=20)
        try:
            conn.request(method, "/api/v1/" + route, json.dumps(data) if data is not None else None, headers)
            reply = conn.getresponse()
            raw = reply.read()
            return reply.status, json.loads(raw) if raw and "application/json" in reply.getheader("Content-Type", "") else {}
        finally:
            conn.close()

    def expect(method, route, status, data=None, token=None):
        actual, body = request(method, route, data, token)
        if actual != status:
            raise AssertionError(f"{method} {route.split('/')[0]} expected {status}, got {actual}")
        return body

    def signup(name, **extra):
        return request("POST", "auth/register", {"username": name, "password": password, **extra})

    def login(name, role):
        data = expect("POST", "auth/login", 200, {"username": name, "password": password})["data"]
        assert data["role"] == role
        return data["token"]

    barrier = threading.Barrier(2)

    def concurrent_signup():
        barrier.wait(timeout=5)
        return signup(prefix)

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        responses = list(pool.map(lambda _: concurrent_signup(), range(2)))
    statuses = sorted(status for status, _ in responses)
    assert statuses == [201, 409], statuses
    assert next(body for status, body in responses if status == 409)["errorCode"] == "USER_ALREADY_EXISTS"
    for _, body in responses:
        assert password not in json.dumps(body)
    print("PASS: concurrent signup has one success and one friendly duplicate response", flush=True)

    expect("POST", "auth/register", 400, {"username": "bad name", "password": password})
    expect("POST", "auth/register", 400, {"username": prefix + "short", "password": "short"})
    expect("POST", "auth/register", 400, {"username": prefix + "long", "password": "x" * 256})
    expect("POST", "auth;ignored/register", 404, {"username": prefix + "alias", "password": password})
    print("PASS: invalid input and alternate registration route rejected", flush=True)

    assert signup(prefix + "peer", role="CHILD", ownerId=999, maxChildren=999)[0] == 201
    assert signup(prefix + "other", role="SUPER_ADMIN")[0] == 201
    owner = login(prefix, "ADMIN")
    peer = login(prefix + "peer", "ADMIN")
    login(prefix + "other", "ADMIN")
    expect("POST", "child/register", 401, {"childUsername": "-child", "password": password})
    print("PASS: public endpoint creates ordinary ADMIN only, CHILD creation requires login", flush=True)

    child = expect("POST", "child/register", 200, {"childUsername": "-child", "password": password}, owner)["data"]
    child_route = f"child/{child['id']}"
    assert signup(prefix + "-child")[0] == 409
    child_token = login(prefix + "-child", "CHILD")
    expect("POST", "child/register", 403, {"childUsername": "-nested", "password": password}, child_token)
    expect("PUT", child_route, 403, {"childUsername": "-foreign"}, peer)
    assert expect("DELETE", "child/" + prefix + "-child", 400, token=peer)["errorCode"] == "FORBIDDEN"
    assert not expect("GET", "child/list", 200, token=peer)["data"]
    expect("PUT", child_route, 200, {"childUsername": "-renamed"}, owner)
    assert any(item["username"] == prefix + "-renamed" for item in expect("GET", "child/list", 200, token=owner)["data"])
    expect("DELETE", "child/" + prefix + "-renamed", 200, token=owner)
    print("PASS: owner manages CHILD, peer and CHILD lack that permission, usernames stay unambiguous", flush=True)

    def race_for_shared_name(admin_signup):
        barrier.wait(timeout=5)
        if admin_signup:
            return signup(prefix + "-race")[0]
        return request("POST", "child/register", {"childUsername": "-race", "password": password}, owner)[0]

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        winner, loser = sorted(pool.map(race_for_shared_name, (True, False)))
    assert winner in (200, 201) and loser == 409
    print("PASS: concurrent ADMIN signup and CHILD creation cannot claim the same name", flush=True)

    def invalid_signup(_):
        return request("POST", "auth/register", {"username": "!", "password": password})[0]

    with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool:
        results = list(pool.map(invalid_signup, range(18)))
    assert 429 in results and set(results) <= {400, 429}, results
    print("PASS: registration flood is throttled by HTTPS proxy", flush=True)


if __name__ == "__main__":
    main()
