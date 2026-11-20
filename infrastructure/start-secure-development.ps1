param(
    [Parameter(Mandatory)] [string] $RuntimeDirectory,
    [string] $OpenSsl = 'openssl'
)
$ErrorActionPreference = 'Stop'
$runtime = [IO.Path]::GetFullPath($RuntimeDirectory)
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($runtime.StartsWith($source, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Choose a runtime directory outside the source checkout for private keys.'
}
$null = New-Item -ItemType Directory -Path $runtime -Force
$envFile = Join-Path $runtime 'private.env'
$certificate = Join-Path $runtime 'server.crt'
$key = Join-Path $runtime 'server.key'
if (!(Test-Path -LiteralPath $envFile)) {
    $jwt = [Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $node = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $db = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    @("JWT_SIGNING_KEY=$jwt", "RELAY_NODE_TOKEN=$node", "POSTGRES_PASSWORD=$db",
      'RELAY_ADVERTISED_HOST=localhost') | Set-Content -LiteralPath $envFile -Encoding ascii
}
if (!(Test-Path -LiteralPath $certificate) -or !(Test-Path -LiteralPath $key)) {
    if ((Test-Path -LiteralPath $certificate) -or (Test-Path -LiteralPath $key)) {
        throw 'Incomplete certificate pair. Restore the matching file before starting.'
    }
    & $OpenSsl req -x509 -newkey rsa:2048 -sha256 -nodes -days 30 -subj '/CN=localhost' -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1' -keyout $key -out $certificate 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Certificate generation failed.' }
}
$nginx = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'nginx.conf'))
$nginx = $nginx.Replace('listen 80;', "listen 443 ssl;" + [Environment]::NewLine + "        ssl_certificate /run/tls/server.crt; ssl_certificate_key /run/tls/server.key; ssl_protocols TLSv1.2 TLSv1.3;")
[IO.File]::WriteAllText((Join-Path $runtime 'nginx.conf'), $nginx)
$env:REMOTE_TLS_DIR = $runtime.Replace('\', '/')
docker compose --env-file $envFile -p pbl4-security -f (Join-Path $PSScriptRoot 'docker-compose.yml') -f (Join-Path $PSScriptRoot 'docker-compose.tls.yml') up -d --build --wait
if ($LASTEXITCODE -ne 0) { throw 'Secure stack did not start.' }
Write-Output 'Secure development API is available at https://127.0.0.1:19290.'
