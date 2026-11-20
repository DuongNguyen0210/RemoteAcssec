param(
    [Parameter(Mandatory = $true)] [string] $Domain,
    [Parameter(Mandatory = $true)] [string] $Executable,
    [Parameter(Mandatory = $true)] [string] $QtBin,
    [Parameter(Mandatory = $true)] [string] $MingwBin,
    [Parameter(Mandatory = $true)] [string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
$Domain = $Domain.Trim().ToLowerInvariant()
if ($Domain -notmatch '^(?=.{1,253}$)(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z]{2,63}$') {
    throw 'Domain must be a DNS name, for example myapp.duckdns.org.'
}
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) { throw "Executable not found: $Executable" }
if (-not (Test-Path -LiteralPath (Join-Path $QtBin 'windeployqt.exe') -PathType Leaf)) {
    throw "windeployqt.exe not found in: $QtBin"
}
if (-not (Test-Path -LiteralPath $MingwBin -PathType Container)) { throw "MinGW bin not found: $MingwBin" }
if (Test-Path -LiteralPath $OutputDirectory) { throw "Output already exists: $OutputDirectory" }

$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$zipPath = "$OutputDirectory.zip"
if (Test-Path -LiteralPath $zipPath) { throw "ZIP already exists: $zipPath" }

New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$targetExe = Join-Path $OutputDirectory 'RemoteAccessApp.exe'
Copy-Item -LiteralPath $Executable -Destination $targetExe

$env:PATH = "$QtBin;$MingwBin;$env:PATH"
& (Join-Path $QtBin 'windeployqt.exe') --release --compiler-runtime --no-translations $targetExe
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }

foreach ($required in @('platforms\qwindows.dll', 'tls\qschannelbackend.dll', 'libstdc++-6.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $OutputDirectory $required) -PathType Leaf)) {
        throw "Required runtime file missing: $required"
    }
}

$launcher = @"
@echo off
setlocal
cd /d "%~dp0"
set "REMOTE_API_URL=https://$Domain"
set "REMOTE_RELAY_MODE=dynamic"
set "REMOTE_RELAY_TLS=required"
set "REMOTE_ALLOW_HTTP="
set "REMOTE_API_CA_CERT="
set "REMOTE_RELAY_CA_CERT="
set "REMOTE_CA_FILE="
"%~dp0RemoteAccessApp.exe"
if errorlevel 1 pause
"@
$readme = @"
RemoteAccessApp for Windows

Double-click Open-RemoteAccess.cmd to start the app.
Server: https://$Domain
Do not put passwords or private keys in this folder.
Share the whole extracted folder, not only RemoteAccessApp.exe.
"@
$encoding = [System.Text.Encoding]::ASCII
[System.IO.File]::WriteAllText((Join-Path $OutputDirectory 'Open-RemoteAccess.cmd'), ($launcher -replace "`r?`n", "`r`n") + "`r`n", $encoding)
[System.IO.File]::WriteAllText((Join-Path $OutputDirectory 'README.txt'), ($readme -replace "`r?`n", "`r`n") + "`r`n", $encoding)

Compress-Archive -LiteralPath $OutputDirectory -DestinationPath $zipPath
Write-Output "PACKAGE=$zipPath"
