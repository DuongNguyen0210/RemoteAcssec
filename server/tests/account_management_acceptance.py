"""Check account CRUD and concurrent limits against an isolated loopback API."""
import concurrent.futures
import json
import os
import secrets
import urllib.error
import urllib.parse
import urllib.request
import uuid

if os.environ.get('TEST_DISPOSABLE_ACCOUNT_STACK') != '1':
    raise SystemExit('Requires TEST_DISPOSABLE_ACCOUNT_STACK=1; creates disposable accounts')
base = os.environ.get('ACCOUNT_TEST_API_URL', 'http://127.0.0.1:18139')
if urllib.parse.urlparse(base).hostname not in ('localhost', '127.0.0.1', '::1'):
    raise SystemExit('Use a disposable loopback API')

def request(path, body=None, token=None, method=None, expected=200):
    headers = {'Content-Type': 'application/json'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    req = urllib.request.Request(base + '/api/v1/' + path,
        data=json.dumps(body).encode() if body is not None else None, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=20) as response:
            status, result = response.status, json.load(response)
    except urllib.error.HTTPError as error:
        status, result = error.code, json.load(error)
    if status != expected:
        raise AssertionError(f'{path}: expected HTTP {expected}, received {status}')
    return result

owner = 'ux' + uuid.uuid4().hex[:10]
password = secrets.token_hex(16)
for name in (owner, owner + 'peer'):
    request('auth/register', {'username': name, 'password': password}, expected=201)
token = request('auth/login', {'username': owner, 'password': password})['data']['token']
peer = request('auth/login', {'username': owner + 'peer', 'password': password})['data']['token']
child = request('child/register', {'childUsername': ' first ', 'password': password}, token)['data']
assert child['username'] == owner + 'first'
assert not any('password' in k.lower() for k in child)
request('child/register', {'childUsername': 'first', 'password': password}, token, expected=409)
request('child/register', {'childUsername': 'x' * 50, 'password': password}, token, expected=400)
second = request('child/register', {'childUsername': 'second', 'password': password}, token)['data']
request(f"child/{child['id']}", {'childUsername': 'second'}, token, method='PUT', expected=409)
request(f"child/{child['id']}", {'childUsername': 'stolen'}, peer, method='PUT', expected=403)
request(f"child/{child['id']}", {'childUsername': 'renamed'}, token, method='PUT')
login = request('auth/login', {'username': owner + 'renamed', 'password': password})['data']
request('child/heartbeat', {'hostname': 'Account UX acceptance', 'os': 'Test'}, login['token'])
replacement = secrets.token_hex(16)
request(f"child/{child['id']}", {'childUsername': 'renamed', 'newPassword': replacement}, token, method='PUT')
request('child/heartbeat', {'hostname': 'Revoked login'}, login['token'], expected=401)
fresh = request('auth/login', {'username': owner + 'renamed', 'password': replacement})['data']
assert fresh['role'] == 'CHILD'
request('child/logout', {}, fresh['token'])
request('child/' + second['username'], token=token, method='DELETE')

def add(index):
    # With one existing child and default limit five, exactly four requests may succeed.
    headers = {'Content-Type': 'application/json', 'Authorization': 'Bearer ' + token}
    req = urllib.request.Request(base + '/api/v1/child/register',
        data=json.dumps({'childUsername': 'parallel' + str(index), 'password': password}).encode(), headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=30) as response:
            return response.status
    except urllib.error.HTTPError as error:
        return error.code
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    statuses = list(pool.map(add, range(8)))
assert statuses.count(200) == 4 and statuses.count(409) == 4, statuses
accounts = request('child', token=token)['data']
assert len(accounts) == 5
for account in accounts:
    request('child/' + account['username'], token=token, method='DELETE')
assert request('child', token=token)['data'] == []
print('PASS: create, trimmed login, duplicate/length validation, rename, preserved password, reset/revocation, owner isolation, delete, concurrent account limit')
