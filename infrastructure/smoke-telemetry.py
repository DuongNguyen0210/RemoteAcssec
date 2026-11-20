"""Telemetry acceptance on a disposable localhost HTTPS stack; no production data.
Optional Qt acceptance starts separate headless CHILD/ADMIN processes, never screen capture or native input.
"""
import argparse
import http.client
import json
import os
from pathlib import Path
import ssl
import subprocess
import time
import uuid


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ca',type=Path,required=True)
    parser.add_argument('--api-port',type=int,default=19590)
    parser.add_argument('--qt-test',type=Path)
    args=parser.parse_args()
    ca=args.ca.resolve(strict=True)
    context=ssl.create_default_context(cafile=str(ca))
    connection=http.client.HTTPSConnection('localhost',args.api_port,context=context,timeout=15)
    prefix='met'+uuid.uuid4().hex[:12]
    password=uuid.uuid4().hex
    checks=[]
    def check(condition,label):
        if not condition:raise AssertionError(label)
        checks.append(label);print('PASS:',label,flush=True)
    def req(method,path,data=None,token=None,expected=200):
        headers={'Content-Type':'application/json'}
        if token:headers['Authorization']='Bearer '+token
        connection.request(method,'/api/v1/'+path,json.dumps(data) if data is not None else None,headers)
        response=connection.getresponse();body=response.read()
        if response.status!=expected:raise AssertionError(f'{method} {path}: status {response.status}, expected {expected}')
        return json.loads(body) if body else {}
    def login(name,pwd=password):return req('POST','auth/login',{'username':name,'password':pwd})['data']
    def sample(sequence,cpu=37.5):
        return {'sequence':sequence,'cpuPercent':cpu,'memoryUsedBytes':4294967296,'memoryTotalBytes':17179869184,
                'networkRxBytesPerSecond':2048.0,'networkTxBytesPerSecond':512.0}
    def heartbeat(value,token):return req('POST','child/heartbeat',{'hostname':'Telemetry fixture','os':'Windows','telemetry':value},token)
    def devices(token):return req('GET','devices',token=token)['data']
    def stream(token):
        conn=http.client.HTTPSConnection('localhost',args.api_port,context=context,timeout=12)
        conn.request('GET','/api/v1/devices/events',headers={'Authorization':'Bearer '+token})
        response=conn.getresponse()
        if response.status!=200:raise AssertionError('Authenticated SSE failed')
        return conn,response
    def until(response,predicate,timeout=12):
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            line=response.readline()
            if not line:raise AssertionError('SSE ended before expected snapshot')
            if line.startswith(b'data:'):
                value=json.loads(line[5:].strip())
                if predicate(value):return value
        raise AssertionError('SSE did not reach expected state')

    for name in (prefix,prefix+'peer'):
        req('POST','auth/register',{'username':name,'password':password},expected=201)
    owner=login(prefix);peer=login(prefix+'peer');token=owner['token'];peer_token=peer['token']
    req('POST','child/register',{'childUsername':'-child','password':password},token)
    child_name=prefix+'-child';child=login(child_name);ct=child['token']
    heartbeat(sample(1),ct)
    view=devices(token)
    check(len(view)==1 and view[0]['telemetry']['cpuPercent']==37.5,'CPU/RAM/RX/TX arrive in authorized ADMIN REST snapshot')
    check(view[0]['telemetry']['memoryUsedBytes']==4294967296 and view[0]['telemetry']['memoryTotalBytes']==17179869184,
          'RAM byte counts remain exact through JSON and Redis')
    check(view[0]['telemetry']['networkRxBytesPerSecond']==2048 and view[0]['telemetryAgeMs']>=0,
          'network uses bytes/second and age comes from the server')
    check(not devices(peer_token),'other ADMIN sees no telemetry')
    req('GET','devices',token=ct,expected=403);req('GET','devices/events',token=ct,expected=403)
    req('GET','devices',expected=401)
    check(True,'CHILD and anonymous users cannot subscribe to ADMIN telemetry')
    peer_conn,peer_sse=stream(peer_token)
    try:check(until(peer_sse,lambda x:not x)==[],'other ADMIN SSE contains no foreign telemetry')
    finally:peer_conn.close()
    conn,sse=stream(token)
    try:
        until(sse,lambda x:len(x)==1 and x[0]['telemetry']['sequence']==1)
        heartbeat(sample(2,50),ct)
        live=until(sse,lambda x:len(x)==1 and x[0]['telemetry']['sequence']==2)
        check(live[0]['telemetry']['cpuPercent']==50,'real SSE updates CPU without an ADMIN refresh')
        accepted=devices(token)[0]
        heartbeat(sample(1,99),ct);heartbeat(sample(2,99),ct)
        replay=devices(token)[0]
        check(replay['telemetry']==accepted['telemetry'] and replay['lastHeartbeatAt']==accepted['lastHeartbeatAt'],
              'late and duplicate samples cannot overwrite data or refresh freshness')
        for field,value in [('cpuPercent',101),('memoryUsedBytes',-1),('memoryTotalBytes',1),
                            ('networkRxBytesPerSecond',-1),('sequence',0)]:
            invalid=sample(3);invalid[field]=value
            req('POST','child/heartbeat',{'os':'Windows','telemetry':invalid},ct,expected=400)
        check(devices(token)[0]['telemetry']==accepted['telemetry'],'invalid ranges rejected without changing current telemetry')
        # No publisher heartbeat now: observe actual production20s presence TTL, not a shortened test TTL.
        until(sse,lambda x:not x,timeout=28)
        check(not devices(token),'lost publisher expires telemetry automatically with real presence TTL')
    finally:conn.close()
    warm={'sequence':3,'memoryUsedBytes':0,'memoryTotalBytes':17179869184}
    heartbeat(warm,ct)
    restored=devices(token)[0]['telemetry']
    check(restored['cpuPercent'] is None and restored['networkRxBytesPerSecond'] is None and restored['memoryUsedBytes']==0,
          'recovery warmup is unknown rather than invented zero CPU/network')
    zero=sample(4,0);zero['networkRxBytesPerSecond']=zero['networkTxBytesPerSecond']=0
    heartbeat(zero,ct)
    check(devices(token)[0]['telemetry']['networkRxBytesPerSecond']==0,'measured zero traffic stays distinguishable from unknown')
    exact=sample(5);exact['memoryTotalBytes']=9007199254740991;exact['memoryUsedBytes']=9007199254740990
    heartbeat(exact,ct)
    check(devices(token)[0]['telemetry']['memoryTotalBytes']==9007199254740991,'large valid integer counters retain exact JSON precision')
    req('POST','child/unshare',{},ct)
    check(not devices(token),'stop sharing removes telemetry immediately')
    heartbeat(sample(6),ct)
    check(devices(token)[0]['telemetry']['sequence']==6,'resume sharing restores a fresh sample')
    req('POST','child/heartbeat',{'os':'Legacy'},ct)
    check(devices(token)[0]['telemetry'] is None,'legacy heartbeat remains compatible and clears stale measurements')
    req('POST','auth/logout',{},ct)
    req('POST','child/heartbeat',{'os':'Windows','telemetry':sample(7)},ct,expected=401)
    check(not devices(token),'logged-out CHILD cannot revive old telemetry')
    child2=login(child_name);heartbeat(sample(1),child2['token'])
    current=devices(token)[0]
    check(current['sessionId']!=child['sessionId'] and current['telemetry']['sequence']==1,
          'new CHILD login starts a separate sample generation')
    for session in (child2,owner,peer):req('POST','auth/logout',{},session['token'])
    connection.close()

    if args.qt_test:
        # New accounts avoid sharing API-fixture sessions with real Qt processes.
        qt_name=prefix+'qt'
        connection=http.client.HTTPSConnection('localhost',args.api_port,context=context,timeout=15)
        req('POST','auth/register',{'username':qt_name,'password':password},expected=201)
        admin=login(qt_name)
        req('POST','child/register',{'childUsername':'-child','password':password},admin['token'])
        req('POST','auth/logout',{},admin['token']);connection.close()
        result_dir=ca.parent
        env=dict(os.environ,REMOTE_API_URL=f'https://localhost:{args.api_port}',REMOTE_CA_FILE=str(ca),
                 REMOTE_API_CA_CERT=str(ca),REMOTE_RELAY_CA_CERT=str(ca),REMOTE_RELAY_TLS='required',
                 PBL4_TELEMETRY_LIVE='1',PBL4_TELEMETRY_ADMIN_USER=qt_name,PBL4_TELEMETRY_CHILD_USER=qt_name+'-child',
                 PBL4_TELEMETRY_PASSWORD=password,PBL4_TELEMETRY_RESULT_DIR=str(result_dir),
                 TEST_SCREENSHOT_DIR=str(result_dir/'screenshots'),QT_QPA_PLATFORM='offscreen')
        result=result_dir/'qt-telemetry-live.txt'
        run=subprocess.run([str(args.qt_test.resolve()),'liveEndToEnd','-o',str(result)+',txt'],env=env,timeout=110)
        output=result.read_text(encoding='utf-8') if result.exists() else ''
        print(output,end='',flush=True)
        check(run.returncode==0 and 'PASS   : TelemetryFlowTest::liveEndToEnd()' in output,
              'separate real Qt CHILD collector -> HTTPS/Redis/SSE -> ADMIN store and visible metric labels; stop/relogin verified')
    print(f'PASS: {len(checks)} telemetry acceptance checks. No credentials printed.',flush=True)


if __name__=='__main__':main()
