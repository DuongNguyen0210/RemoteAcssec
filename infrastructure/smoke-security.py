"""Verify the disposable pbl4-security stack, including stored credential migration."""
import http.client
import json
import os
import ssl
import subprocess
import uuid

context = ssl.create_default_context(cafile=os.environ["REMOTE_CA_FILE"])
def request(method, route, data=None, token=None):
    connection = http.client.HTTPSConnection("127.0.0.1", 19290, context=context, timeout=15)
    try:
        headers = {"Content-Type": "application/json"}
        if token:
            headers["Authorization"] = "Bearer " + token
        connection.request(method, route, json.dumps(data) if data is not None else None, headers)
        reply = connection.getresponse()
        return reply.status, json.load(reply)
    finally:
        connection.close()

def sql(command):
    result = subprocess.run(["docker", "exec", "pbl4-security-postgres-1", "psql",
                             "-U", "admin", "-d", "remote_db", "-Atc", command],
                            capture_output=True, text=True, check=True)
    return result.stdout.strip()

def main():
    name = "secure" + uuid.uuid4().hex[:12]
    password = uuid.uuid4().hex
    credentials = {"username": name, "password": password}
    status, result = request("POST", "/api/v1/auth/register", credentials)
    assert status == 201 and result["success"]
    stored = sql(f"select password from users where username='{name}'")
    assert stored.startswith("{pbkdf2-sha256}") and stored != password
    _, result = request("POST", "/api/v1/auth/login", credentials)
    assert result["success"]
    admin = result["data"]["token"]

    _, result = request("POST", "/api/v1/child/register",
                        {"childUsername": "-child", "password": password}, admin)
    assert result["success"]
    child_name = name + "-child"
    stored = sql(f"select password from child where username='{child_name}'")
    assert stored.startswith("{pbkdf2-sha256}") and stored != password
    print("PASS: new ADMIN and CHILD passwords are hashed")

    # Seed only these disposable accounts with legacy credentials.
    sql(f"update users set password='{password}' where username='{name}'")
    sql(f"update child set password='{password}' where username='{child_name}'")
    for table, username in [("users", name), ("child", child_name)]:
        _, result = request("POST", "/api/v1/auth/login", {"username": username, "password": password})
        assert result["success"]
        assert sql(f"select password from {table} where username='{username}'").startswith("{pbkdf2-sha256}")
    print("PASS: existing plaintext credentials upgrade after login")

    new_password = uuid.uuid4().hex
    status, result = request("PUT", "/api/v1/user/password",
                             {"oldPassword": password, "newPassword": new_password}, admin)
    assert status == 200 and result["success"]
    assert sql(f"select password from users where username='{name}'").startswith("{pbkdf2-sha256}")
    _, old_login = request("POST", "/api/v1/auth/login", credentials)
    assert not old_login["success"]
    _, new_login = request("POST", "/api/v1/auth/login", {"username": name, "password": new_password})
    assert new_login["success"]
    print("PASS: password change rejects old credentials")

    _, peer = request("POST", "/api/v1/auth/register",
                      {"username": name + "peer", "password": password})
    _, peer = request("POST", "/api/v1/auth/login",
                      {"username": name + "peer", "password": password})
    _, child = request("POST", "/api/v1/auth/login", {"username": child_name, "password": password})
    child = child["data"]
    request("POST", "/api/v1/child/heartbeat", {"hostname": "Security smoke", "os": "Windows"}, child["token"])
    status, owner_devices = request("GET", "/api/v1/devices", token=new_login["data"]["token"])
    assert status == 200 and len(owner_devices["data"]) == 1
    status, peer_devices = request("GET", "/api/v1/devices", token=peer["data"]["token"])
    assert status == 200 and peer_devices["data"] == []
    status, _ = request("GET", "/api/v1/devices", token=child["token"])
    assert status == 403
    print("PASS: device owner and role isolation")
    stream = http.client.HTTPSConnection("127.0.0.1", 19290, context=context, timeout=15)
    try:
        stream.request("GET", "/api/v1/devices/events",
                       headers={"Authorization": "Bearer " + new_login["data"]["token"]})
        response = stream.getresponse()
        assert response.status == 200
        while True:
            line = response.readline()
            assert line, "SSE stream closed before snapshot"
            if line.startswith(b"data:"):
                assert len(json.loads(line[5:])) == 1
                break
        print("PASS: live device snapshot over HTTPS")
    finally:
        stream.close()

    try:
        connection = http.client.HTTPSConnection("127.0.0.1", 19290, timeout=10)
        connection.request("GET", "/health")
    except ssl.SSLCertVerificationError:
        print("PASS: an untrusted HTTPS certificate is rejected")
    else:
        raise AssertionError("Untrusted HTTPS certificate accepted")
    finally:
        connection.close()

if __name__ == "__main__":
    main()
