"""Run against disposable API/relay instances (defaults: 19090/18080), never production."""
import json
import os
import socket
import struct
import uuid
import urllib.request
import urllib.error

API = os.environ.get('TEST_API_URL', 'http://127.0.0.1:19090')
RELAY = int(os.environ.get('TEST_RELAY_PORT', '18080'))

def request(method, path, token=None, data=None, expected=200):
    headers = {'Content-Type': 'application/json'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    req = urllib.request.Request(API + '/api/v1/' + path,
        data=None if data is None else json.dumps(data).encode(), headers=headers, method=method)
    try:
        response = urllib.request.urlopen(req, timeout=10)
    except urllib.error.HTTPError as error:
        response = error
    body = response.read()
    assert response.status == expected, (path, response.status, body)
    return json.loads(body) if body else None

def login(username):
    return request('POST', 'auth/login', data={'username': username, 'password': 'test-password'})['data']

def text(value):
    raw = value.encode()
    return struct.pack('!H', len(raw)) + raw

def send(sock, kind, payload=b'', session=0):
    sock.sendall(struct.pack('!IBBHIQI', 0x52445450, 2, kind, 0, len(payload), session, 0) + payload)

def read(sock):
    def exact(size):
        result = b''
        while len(result) < size:
            part = sock.recv(size-len(result))
            assert part, 'unexpected relay disconnect'
            result += part
        return result
    magic, version, kind, flags, length, session, seq = struct.unpack('!IBBHIQI', exact(24))
    assert magic == 0x52445450 and version == 2
    return kind, session, exact(length)

owner = 'e2e' + uuid.uuid4().hex[:12]
other = owner + 'other'
for name in [owner, other]:
    request('POST', 'auth/register', data={'username': name, 'password': 'test-password'}, expected=201)
admin = login(owner)['token']
foreign = login(other)['token']
request('POST', 'child/register', admin, {'childUsername': 'child', 'password': 'test-password'})
account = request('GET', 'child', admin)['data']
assert len(account) == 1
assert not {'password', 'online', 'os', 'ipAddress'} & account[0].keys()
assert request('GET', 'devices', admin)['data'] == []
a, b = login(owner + 'child'), login(owner + 'child')
assert a['sessionId'] != b['sessionId']
assert request('GET', 'devices', admin)['data'] == []
for session, hostname in [(a, 'Laptop'), (b, 'PC')]:
    request('POST', 'child/heartbeat', session['token'], {'hostname': hostname, 'os': 'Linux'})
devices = request('GET', 'devices', admin)['data']
assert len(devices) == 2 and {d['deviceName'] for d in devices} == {'Laptop', 'PC'}
assert request('GET', 'devices', foreign)['data'] == []
request('GET', 'devices', a['token'], expected=403)
with socket.create_connection(('127.0.0.1', RELAY), timeout=5) as laptop, \
     socket.create_connection(('127.0.0.1', RELAY), timeout=5) as pc, \
     socket.create_connection(('127.0.0.1', RELAY), timeout=5) as controller, \
     socket.create_connection(('127.0.0.1', RELAY), timeout=5) as stranger:
    send(laptop, 1, text(a['token']))
    assert read(laptop) == (2, 0, b'\x01')
    send(pc, 1, text(b['token']))
    assert read(pc) == (2, 0, b'\x01')
    send(stranger, 3, text(foreign) + text(b['sessionId']))
    assert read(stranger) == (7, 0, b'\x00')
    send(controller, 3, text(admin) + text(b['sessionId']))
    kind, remote_session, body = read(pc)
    assert kind == 4 and remote_session != 0 and body == b''
    send(pc, 5, session=remote_session)
    assert read(controller) == (7, remote_session, b'\x01')
    laptop.settimeout(0.2)
    try:
        laptop.recv(1)
        raise AssertionError('Connect intended for PC reached Laptop')
    except socket.timeout:
        pass
request('POST', 'child/logout', a['token'], {})
request('POST', 'child/heartbeat', a['token'], {'hostname': 'Late', 'os': 'Linux'}, expected=401)
assert len(request('GET', 'devices', admin)['data']) == 1
request('DELETE', 'child/' + owner + 'child', admin)
assert request('GET', 'devices', admin)['data'] == []
request('POST', 'child/heartbeat', b['token'], {'hostname': 'PC', 'os': 'Linux'}, expected=401)
print('PASS: account-only login, two heartbeating machines, owner isolation, targeted relay, logout, account deletion')
