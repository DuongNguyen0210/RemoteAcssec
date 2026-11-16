"""Exercise single-login policy across two API instances on a disposable loopback stack.

TEST_DISPOSABLE_ACCOUNT_STACK=1 ACCOUNT_TEST_API_URLS=http://127.0.0.1:18149,http://127.0.0.1:18150 python3 server/tests/single_login_acceptance.py
No credentials or bearer tokens are printed. Takes ~95 seconds to verify real expiry.
"""
import concurrent.futures
import json
import os
import secrets
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid

if os.environ.get('TEST_DISPOSABLE_ACCOUNT_STACK') != '1':
    raise SystemExit('Requires TEST_DISPOSABLE_ACCOUNT_STACK=1; creates disposable accounts')
bases = os.environ.get('ACCOUNT_TEST_API_URLS', 'http://127.0.0.1:18149,http://127.0.0.1:18150').split(',')
if len(bases) != 2 or any(urllib.parse.urlparse(b).hostname not in ('localhost', '127.0.0.1', '::1') for b in bases):
    raise SystemExit('Use two disposable loopback APIs sharing PostgreSQL and Redis')

def request(path, body=None, token=None, expected=200, node=0):
    headers = {'Content-Type': 'application/json'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    req = urllib.request.Request(bases[node] + '/api/v1/' + path,
        data=json.dumps(body).encode() if body is not None else None, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=15) as response:
            status, result = response.status, json.load(response)
    except urllib.error.HTTPError as error:
        status, result = error.code, json.load(error)
    if status != expected:
        raise AssertionError(f'{path}: expected HTTP {expected}, received {status}')
    return result

password = secrets.token_hex(20)
owner = 'lease' + uuid.uuid4().hex[:10]
request('auth/register', {'username': owner, 'password': password}, expected=201)

def race(username):
    barrier = threading.Barrier(16)
    def login(index):
        barrier.wait()
        headers = {'Content-Type': 'application/json'}
        req = urllib.request.Request(bases[index % 2] + '/api/v1/auth/login',
            data=json.dumps({'username': username, 'password': password}).encode(), headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=20) as response:
                return response.status, json.load(response)
        except urllib.error.HTTPError as error:
            return error.code, json.load(error)
    with concurrent.futures.ThreadPoolExecutor(max_workers=16) as pool:
        results = list(pool.map(login, range(16)))
    assert sum(s == 200 for s, _ in results) == 1
    assert sum(s == 409 and r['errorCode'] == 'ACCOUNT_IN_USE' for s, r in results) == 15
    data = next(r['data'] for s, r in results if s == 200)
    assert str(uuid.UUID(data['sessionId'])) == data['sessionId']
    request('auth/heartbeat', {}, data['token'], node=1)
    request('auth/login', {'username': username, 'password': 'wrong'}, expected=401)
    return data

admin = race(owner)
token = admin['token']
request('child/register', {'childUsername': 'child', 'password': password}, token)
child = race(owner + 'child')
ct = child['token']
request('child/heartbeat', {'hostname': 'machine-A', 'os': 'test'}, ct)
devices = request('devices', token=token, node=1)['data']
assert len(devices) == 1 and devices[0]['sessionId'] == child['sessionId']
request('child/unshare', {}, ct)
assert request('devices', token=token)['data'] == []
request('auth/login', {'username': owner + 'child', 'password': password}, expected=409, node=1)
request('auth/heartbeat', {}, ct, node=1)
request('auth/logout', {}, ct)
fresh = request('auth/login', {'username': owner + 'child', 'password': password}, node=1)['data']
request('auth/heartbeat', {}, ct, expected=401)
request('auth/logout', {}, ct, expected=401)
request('child/heartbeat', {'hostname': 'late-old-machine'}, ct, expected=401)
request('auth/heartbeat', {}, fresh['token'])
request('child/heartbeat', {'hostname': 'machine-B', 'os': 'test'}, fresh['token'])
devices = request('devices', token=token)['data']
assert len(devices) == 1 and devices[0]['sessionId'] == fresh['sessionId']
request('auth/logout', {}, fresh['token'])
request('auth/logout', {}, token)
new_admin = request('auth/login', {'username': owner, 'password': password}, node=1)['data']
request('auth/logout', {}, token, expected=401)
request('devices', token=token, expected=401)
request('auth/heartbeat', {}, new_admin['token'])
request('auth/logout', {}, new_admin['token'])
print('PASS: ADMIN/CHILD 16-way race across two APIs, sharing-independent lease, logout, stale-token fencing, one presence card', flush=True)

# No heartbeat simulates abrupt process death. Verify the 60s active + 30s drain
# using real clocks, without deleting keys or bypassing authorization.
crashed = request('auth/login', {'username': owner + 'child', 'password': password})['data']
started = time.monotonic()
time.sleep(61)
request('auth/heartbeat', {}, crashed['token'], expected=401, node=1)
request('auth/login', {'username': owner + 'child', 'password': password}, expected=409)
time.sleep(max(0, 91 - (time.monotonic() - started)))
recovered = request('auth/login', {'username': owner + 'child', 'password': password}, node=1)['data']
request('auth/heartbeat', {}, crashed['token'], expected=401)
request('auth/heartbeat', {}, recovered['token'])
request('auth/logout', {}, recovered['token'])
print('PASS: real 60s expiry, 30s relay drain, crash recovery, expired token cannot revive or affect replacement')
