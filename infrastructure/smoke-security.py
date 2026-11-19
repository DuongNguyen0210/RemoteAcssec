"""Security acceptance against ONLY a disposable localhost TLS stack.
Never prints credentials/tokens, never touches unrelated rows or Docker projects.
Reuses PR #40 security coverage and main's real viewer-frame acceptance.
"""
import argparse
import base64
import hashlib
import http.client
import json
import os
from pathlib import Path
import ssl
import subprocess
import time
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ca', type=Path, required=True)
    parser.add_argument('--api-port', type=int, default=19490)
    parser.add_argument('--project', default='pbl4-security-main')
    parser.add_argument('--viewer-test', type=Path)
    args = parser.parse_args()
    if not args.project.startswith('pbl4-security-') or not all(c.islower() or c.isdigit() or c == '-' for c in args.project):
        raise RuntimeError('Only disposable pbl4-security-* projects are allowed')
    ca = args.ca.resolve(strict=True)
    trust = ssl.create_default_context(cafile=str(ca))
    prefix = 'sec' + uuid.uuid4().hex[:12]
    password = uuid.uuid4().hex
    new_password = uuid.uuid4().hex
    passed = []
    live_sessions = {}

    def check(condition, label):
        if not condition: raise AssertionError(label)
        passed.append(label)
        print('PASS:', label, flush=True)

    def request(method, route, data=None, token=None, expected=200, _renewing=False):
        # Model desktop's periodic auth heartbeat while a long acceptance run is active.
        # Remove sessions immediately after reset/change/logout so revoked tokens are never renewed.
        if not _renewing:
            for live_token, meta in list(live_sessions.items()):
                if time.monotonic() - meta['renewed'] > 12:
                    request('POST', '/api/v1/auth/heartbeat', token=live_token, _renewing=True)
                    if meta['role'] == 'CHILD':
                        request('POST', '/api/v1/child/heartbeat', {'hostname': 'Security lab CHILD', 'os': 'Windows'}, live_token, _renewing=True)
                    meta['renewed'] = time.monotonic()
        conn = http.client.HTTPSConnection('localhost', args.api_port, context=trust, timeout=15)
        headers = {'Content-Type': 'application/json'}
        if token: headers['Authorization'] = 'Bearer ' + token
        try:
            conn.request(method, route, json.dumps(data) if data is not None else None, headers)
            response = conn.getresponse()
            body = response.read()
            if response.status != expected:
                raise AssertionError(f'{method} {route} status {response.status}; expected {expected}')
            result = json.loads(body) if body and response.getheader("Content-Type", "").startswith("application/json") else {}
            if response.status == 200:
                if route == '/api/v1/auth/login':
                    session = result['data']
                    live_sessions[session['token']] = dict(session, renewed=time.monotonic())
                if route == '/api/v1/auth/logout' or (method == 'PUT' and route == '/api/v1/user/password'):
                    live_sessions.pop(token, None)
                if method == 'PUT' and route.startswith('/api/v1/child/') and data.get('newPassword'):
                    reset_id = route.rsplit('/', 1)[1]
                    for old_token, meta in list(live_sessions.items()):
                        if meta['role'] == 'CHILD' and str(meta['userId']) == reset_id:
                            live_sessions.pop(old_token, None)
            return result
        finally: conn.close()

    def login(name, pwd=password, expected=200):
        return request('POST', '/api/v1/auth/login', {'username': name, 'password': pwd}, expected=expected)

    def db(sql):
        cid = subprocess.check_output(['docker', 'ps', '-q', '--filter', 'label=com.docker.compose.project=' + args.project,
                '--filter', 'label=com.docker.compose.service=postgres'], text=True).strip()
        if not cid or '\n' in cid: raise RuntimeError('Expected one private test database')
        run = subprocess.run(['docker', 'exec', '-i', cid, 'psql', '-U', 'admin', '-d', 'remote_db', '-At', '-v', 'ON_ERROR_STOP=1'],
                             input=sql, text=True, capture_output=True)
        if run.returncode: raise RuntimeError('Private test database operation failed')
        return run.stdout.strip()

    def stored(table, name):
        return db(f"SELECT password FROM {table} WHERE username='{name}';")

    def valid_hash(value, pwd):
        if not value.startswith('{pbkdf2-sha256}'): return False
        salt, hashed = value[len('{pbkdf2-sha256}'):].split('$')
        return hashlib.pbkdf2_hmac('sha256', pwd.encode(), base64.b64decode(salt), 600000) == base64.b64decode(hashed)

    for name in (prefix, prefix + 'peer'):
        request('POST', '/api/v1/auth/register', {'username': name, 'password': password}, expected=201)
    check(valid_hash(stored('users', prefix), password), 'new ADMIN stored as salted PBKDF2, not plaintext')
    owner = login(prefix)['data']; peer = login(prefix + 'peer')['data']
    token, peer_token = owner['token'], peer['token']
    check(login(prefix, expected=409)['errorCode'] == 'ACCOUNT_IN_USE', 'duplicate ADMIN login rejected')
    check(login(prefix, 'wrong', expected=401)['errorCode'] == 'AUTH_FAILED', 'wrong ADMIN password rejected')
    login(prefix, stored('users', prefix), expected=401)
    check(True, 'stored hash cannot be used as a password')
    jwt_parts = token.split('.')
    jwt_parts[2] = ('A' if jwt_parts[2][0] != 'A' else 'B') + jwt_parts[2][1:]
    request('GET', '/api/v1/user/profile', token='.'.join(jwt_parts), expected=401)
    request('GET', '/api/v1/user/profile', expected=401)
    check(True, 'tampered and missing JWT rejected')
    request('GET', '/internal/relays', expected=404)
    check(True, 'private Relay management path unavailable on public HTTPS listener')
    account = request('POST', '/api/v1/child/register', {'childUsername': '-child', 'password': password}, token)['data']
    child_name = prefix + '-child'
    child_id = account['id']
    check(valid_hash(stored('child', child_name), password), 'new CHILD stored as PBKDF2')
    child = login(child_name)['data']
    ct = child['token']
    request('POST', '/api/v1/child/heartbeat', {'hostname': 'Security lab CHILD', 'os': 'Windows'}, ct)
    devices = request('GET', '/api/v1/devices', token=token)['data']
    check(len(devices) == 1 and devices[0]['sessionId'] == child['sessionId'], 'HTTPS heartbeat and device list integrated')
    check(not request('GET', '/api/v1/devices', token=peer_token)['data'], 'unrelated ADMIN cannot see CHILD')
    request('POST', '/api/v1/child/register', {'childUsername': '-bad', 'password': password}, ct, expected=403)
    request('PUT', f'/api/v1/child/{child_id}', {'childUsername': '-child', 'newPassword': new_password}, peer_token, expected=403)
    check(True, 'CHILD cannot administer accounts; another ADMIN cannot reset this CHILD')
    allocation = request('POST', '/api/v1/relay/allocations', token=ct)['data']
    endpoint = request('GET', '/api/v1/devices/' + child['sessionId'] + '/relay', token=token)['data']
    check(allocation['instanceId'] == endpoint['instanceId'], 'owner-only dynamic Relay lookup integrated over HTTPS')
    conn = http.client.HTTPSConnection('localhost', args.api_port, context=trust, timeout=10)
    conn.request('GET', '/api/v1/devices/events', headers={'Authorization': 'Bearer ' + token})
    stream = conn.getresponse()
    check(stream.status == 200 and b'event:device-list' in stream.readline(), 'authenticated HTTPS SSE initial snapshot')
    conn.close()
    request('GET', '/api/v1/devices/events', expected=401)
    check(True, 'anonymous SSE rejected')

    # Seed ONLY random new legacy fixtures in the private test DB.
    legacy = prefix + 'legacy'
    db(f"INSERT INTO users(username,password,max_children) VALUES('{legacy}','{password}',5);")
    legacy_login = login(legacy)['data']
    upgraded = stored('users', legacy)
    check(valid_hash(upgraded, password), 'legacy ADMIN upgraded at successful login')
    request('GET', '/api/v1/user/profile', token=legacy_login['token'])
    login(legacy, expected=409)
    request('GET', '/api/v1/user/profile', token=legacy_login['token'])
    check(True, 'migration keeps same JWT/lease live; duplicate login cannot invalidate it')
    request('POST', '/api/v1/auth/logout', token=legacy_login['token'])
    db(f"INSERT INTO child(username,password,user_id) SELECT '{legacy}-child','{password}',id FROM users WHERE username='{legacy}';")
    lc = login(legacy + '-child')['data']
    check(valid_hash(stored('child', legacy + '-child'), password), 'legacy CHILD upgraded at successful login')
    request('POST', '/api/v1/child/heartbeat', {'hostname': 'Legacy CHILD', 'os': 'Windows'}, lc['token'])
    request('POST', '/api/v1/auth/logout', token=lc['token'])
    check(True, 'legacy CHILD presence uses upgraded hash and live session')

    # Reset/ADMIN change must revoke old tokens and preserve main's drain interval.
    request('PUT', f'/api/v1/child/{child_id}', {'childUsername': '-child', 'newPassword': new_password}, token)
    request('POST', '/api/v1/child/heartbeat', {'hostname': 'Revoked', 'os': 'Windows'}, ct, expected=401)
    request('POST', '/api/v1/auth/heartbeat', token=ct, expected=401)
    check(valid_hash(stored('child', child_name), new_password), 'CHILD reset hashes new password and revokes old session')
    request('PUT', '/api/v1/user/password', {'oldPassword': password, 'newPassword': new_password}, token)
    request('GET', '/api/v1/user/profile', token=token, expected=401)
    request('POST', '/api/v1/auth/heartbeat', token=token, expected=401)
    request('GET', '/api/v1/devices/events', token=token, expected=401)
    check(valid_hash(stored('users', prefix), new_password), 'ADMIN password change hashes value and revokes old REST/SSE/heartbeat token')
    login(prefix, password, expected=401); login(child_name, password, expected=401)
    request('POST', '/api/v1/auth/heartbeat', token=peer_token)
    live_sessions[peer_token]['renewed'] = time.monotonic()
    print('Waiting for main single-login security drain (31 seconds)...', flush=True)
    time.sleep(31)
    new_owner = login(prefix, new_password)['data']
    new_child = login(child_name, new_password)['data']
    request('GET', '/api/v1/user/profile', token=new_owner['token'])
    request('POST', '/api/v1/child/heartbeat', {'hostname': 'Reset CHILD', 'os': 'Windows'}, new_child['token'])
    check(True, 'new passwords login after drain; revoked sessions cannot revive')
    request('POST', '/api/v1/auth/logout', token=new_owner['token'])
    request('POST', '/api/v1/auth/logout', token=new_child['token'])
    request('POST', '/api/v1/auth/logout', token=peer_token)

    if args.viewer_test:
        frame_name = prefix + 'frame'
        request('POST', '/api/v1/auth/register', {'username': frame_name, 'password': password}, expected=201)
        frame_admin = login(frame_name)['data']
        request('POST', '/api/v1/child/register', {'childUsername': '-child', 'password': password}, frame_admin['token'])
        request('POST', '/api/v1/auth/logout', token=frame_admin['token'])
        env = dict(os.environ, PBL4_FRAME_SMOKE='1', PBL4_SMOKE_ADMIN_USERNAME=frame_name,
                   PBL4_SMOKE_CHILD_USERNAME=frame_name + '-child', PBL4_SMOKE_PASSWORD=password,
                   REMOTE_API_URL=f'https://localhost:{args.api_port}', REMOTE_CA_FILE=str(ca), REMOTE_RELAY_TLS='required',
                   REMOTE_API_CA_CERT=str(ca), REMOTE_RELAY_CA_CERT=str(ca), QT_QPA_PLATFORM='offscreen')
        result_path = ca.parent / 'security-frame-result.txt'
        run = subprocess.run([str(args.viewer_test.resolve()), 'loopbackRelayDeliversSyntheticFrame',
                              '-o', str(result_path) + ',txt'], env=env, timeout=90)
        result = result_path.read_text(encoding='utf-8') if result_path.exists() else ''
        print(result, end='', flush=True)
        check(run.returncode == 0 and 'PASS   : AdminScreenViewerTest::loopbackRelayDeliversSyntheticFrame()' in result,
              'real Qt ADMIN-Relay-CHILD synthetic image over trusted HTTPS/TLS')
    print(f'PASS: {len(passed)} security acceptance checks. No private data printed.', flush=True)


if __name__ == '__main__':
    main()
