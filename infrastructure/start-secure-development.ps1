# Adapted from PR #40; retains main's two APIs, two Relays and private node endpoint.
param(
    [Parameter(Mandatory)] [string] $RuntimeDirectory,
    [ValidatePattern('^pbl4-security-[a-z0-9-]+$')] [string] $ProjectName = 'pbl4-security-main',
    [string] $OpenSsl = 'openssl',
    [ValidateRange(1024,65533)] [int] $ApiPort = 19490
)
$ErrorActionPreference = 'Stop'
$runtime = [IO.Path]::GetFullPath($RuntimeDirectory)
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($runtime.StartsWith($source + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or $runtime -eq $source) {
    throw 'Choose a runtime directory outside the source checkout for private keys.'
}
$null = New-Item -ItemType Directory -Path $runtime -Force
$envFile = Join-Path $runtime 'private.env'
$certificate = Join-Path $runtime 'server.crt'
$key = Join-Path $runtime 'server.key'
if (!(Test-Path -LiteralPath $envFile)) {
    $jwt = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(48))
    $node = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $db = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $crtPath = $certificate.Replace('\', '/')
    $keyPath = $key.Replace('\', '/')
    @("JWT_SECRET=$jwt", "RELAY_NODE_TOKEN=$node", "POSTGRES_PASSWORD=$db",
      'DATABASE_JDBC_URL=jdbc:postgresql://postgres:5432/remote_db',
      'MANAGEMENT_PUBLIC_BIND=127.0.0.1', "MANAGEMENT_PUBLIC_PORT=$ApiPort",
      'RELAY_PUBLIC_BIND=127.0.0.1', "RELAY_1_PUBLIC_PORT=$($ApiPort+1)", "RELAY_2_PUBLIC_PORT=$($ApiPort+2)",
      'RELAY_1_ADVERTISED_HOST=localhost', 'RELAY_2_ADVERTISED_HOST=localhost',
      "RELAY_1_TLS_CERT=$crtPath", "RELAY_2_TLS_CERT=$crtPath", "RELAY_1_TLS_KEY=$keyPath", "RELAY_2_TLS_KEY=$keyPath",
      "REMOTE_TLS_DIR=$($runtime.Replace('\', '/'))") | Set-Content -LiteralPath $envFile -Encoding ascii
}
if (!(Test-Path -LiteralPath $certificate) -or !(Test-Path -LiteralPath $key)) {
    if ((Test-Path -LiteralPath $certificate) -or (Test-Path -LiteralPath $key)) {
        throw 'Incomplete certificate pair. Restore the matching file before starting.'
    }
    & $OpenSsl req -x509 -newkey rsa:2048 -sha256 -nodes -days 30 -subj '/CN=localhost' -addext 'subjectAltName=DNS:localhost' -keyout $key -out $certificate 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Certificate generation failed.' }
}
$nginx = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'nginx.conf.template'))
$nginx = $nginx.Replace('listen 80;', "listen 443 ssl;`n        ssl_certificate /run/tls/server.crt;`n        ssl_certificate_key /run/tls/server.key;`n        ssl_protocols TLSv1.2 TLSv1.3;")
[IO.File]::WriteAllText((Join-Path $runtime 'nginx.conf.template'), $nginx)
& docker compose --env-file $envFile -p $ProjectName -f (Join-Path $PSScriptRoot 'docker-compose.yml') -f (Join-Path $PSScriptRoot 'docker-compose.tls.yml') -f (Join-Path $PSScriptRoot 'docker-compose.secure-development.yml') up -d --build --wait
if ($LASTEXITCODE -ne 0) { throw 'Secure stack did not start.' }
Write-Output "Secure lab started. Use the ports recorded in $envFile and https://localhost (certificate matches localhost only)."
