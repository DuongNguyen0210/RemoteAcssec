param(
    [Parameter(Mandatory)]
    [uri] $BaseUrl
)

$ErrorActionPreference = 'Stop'
if (!$BaseUrl.IsLoopback) { throw 'Use a disposable Compose stack bound to loopback.' }

function Invoke-SmokeApi {
    param($Method, $Route, $Body, $Token, [int] $Expected = 200)
    $request = @{ Method = $Method; Uri = [uri]::new($BaseUrl, $Route); SkipHttpErrorCheck = $true; TimeoutSec = 15 }
    if ($Token) { $request.Headers = @{ Authorization = "Bearer $Token" } }
    if ($null -ne $Body) {
        $request.ContentType = 'application/json'
        $request.Body = $Body | ConvertTo-Json -Compress
    }
    $response = Invoke-WebRequest @request
    if ($response.StatusCode -ne $Expected) {
        throw "$Method $Route returned $($response.StatusCode), expected $Expected."
    }
    if ($response.Content) { $response.Content | ConvertFrom-Json }
}

$prefix = 'smoke' + [guid]::NewGuid().ToString('N').Substring(0, 12)
$password = [guid]::NewGuid().ToString('N')
$owner = @{ username = $prefix; password = $password }
$peer = @{ username = "$prefix-peer"; password = $password }
$child = @{ childUsername = '-child'; password = $password }
$childLogin = @{ username = $prefix + $child.childUsername; password = $password }

Invoke-SmokeApi POST '/api/v1/auth/register' $owner -Expected 201 | Out-Null
Invoke-SmokeApi POST '/api/v1/auth/register' $peer -Expected 201 | Out-Null
$ownerToken = (Invoke-SmokeApi POST '/api/v1/auth/login' $owner).data.token
$peerToken = (Invoke-SmokeApi POST '/api/v1/auth/login' $peer).data.token
Invoke-SmokeApi POST '/api/v1/child/register' $child $ownerToken | Out-Null
Invoke-SmokeApi POST '/api/v1/child/register' $child $ownerToken -Expected 409 | Out-Null
$account = (Invoke-SmokeApi POST '/api/v1/auth/login' $childLogin).data
if (!$account.sessionId -or $account.role -ne 'CHILD') { throw 'CHILD login did not produce a session.' }

Invoke-SmokeApi POST '/api/v1/child/heartbeat' @{ hostname = 'Smoke device'; os = 'Windows' } $account.token | Out-Null
$devices = (Invoke-SmokeApi GET '/api/v1/devices' $null $ownerToken).data
if (@($devices).Count -ne 1 -or $devices[0].sessionId -ne $account.sessionId) {
    throw 'Owner device list did not contain the CHILD session.'
}
$peerDevices = (Invoke-SmokeApi GET '/api/v1/devices' $null $peerToken).data
if ($peerDevices) { throw 'Unrelated ADMIN received device data.' }

$allocation = (Invoke-SmokeApi POST '/api/v1/relay/allocations' $null $account.token).data
$relayRoute = "/api/v1/devices/$($account.sessionId)/relay"
$endpoint = (Invoke-SmokeApi GET $relayRoute $null $ownerToken).data
if (!$allocation.instanceId -or $endpoint.instanceId -ne $allocation.instanceId -or
    $endpoint.host -ne $allocation.host -or $endpoint.port -ne $allocation.port) {
    throw 'Owner relay lookup did not match the CHILD allocation.'
}

Invoke-SmokeApi POST '/api/v1/child/logout' $null $account.token | Out-Null
$remaining = (Invoke-SmokeApi GET '/api/v1/devices' $null $ownerToken).data
if ($remaining) { throw 'Logged-out device remained visible.' }
Write-Output 'PASS: registration, duplicate rejection, role login, heartbeat, owner isolation, relay allocation, and logout through Nginx.'
