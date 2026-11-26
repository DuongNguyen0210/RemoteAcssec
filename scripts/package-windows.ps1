[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $QtBin,
    [Parameter(Mandatory = $true)] [string] $MingwBin,
    [string] $BuildDirectory,
    [ValidateRange(1, 64)] [int] $Jobs = 2
)

$ErrorActionPreference = 'Stop'

function Invoke-Checked([string] $Program, [string[]] $Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE." }
}

$repo = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repo 'clients\RemoteAccessApp'
$dist = Join-Path $repo 'dist'
if (!$BuildDirectory) { $BuildDirectory = Join-Path $repo 'build-release\windows' }
$QtBin = (Resolve-Path -LiteralPath $QtBin).Path
$MingwBin = (Resolve-Path -LiteralPath $MingwBin).Path
$deployQt = Join-Path $QtBin 'windeployqt.exe'
$qmake = Join-Path $QtBin 'qmake.exe'
$compiler = Join-Path $MingwBin 'g++.exe'
$make = Join-Path $MingwBin 'mingw32-make.exe'
foreach ($tool in @($deployQt, $qmake, $compiler, $make)) {
    if (!(Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Missing build tool $tool." }
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
$temporary = Join-Path $dist ('.windows-package-' + [IO.Path]::GetRandomFileName())
$stage = Join-Path $temporary 'RemoteAccessWin'
New-Item -ItemType Directory -Path $stage -Force | Out-Null
$originalPath = $env:PATH
try {
    $env:PATH = "$QtBin;$MingwBin;$originalPath"
    $cmake = (Get-Command cmake.exe -CommandType Application).Source
    $tar = (Get-Command tar.exe -CommandType Application).Source
    $qtVersion = & $qmake -query QT_VERSION
    if ($LASTEXITCODE -ne 0 -or $qtVersion -notmatch '^6\.') { throw 'The Windows release requires Qt 6 MinGW.' }

    Invoke-Checked $cmake @(
        "-DCLIENT_CONFIG=$source\.env", "-DPACKAGE_DIRECTORY=$stage",
        '-P', (Join-Path $PSScriptRoot 'stage-client-config.cmake')
    )
    Invoke-Checked $cmake @(
        '-S', $source, '-B', $BuildDirectory, '-G', 'MinGW Makefiles',
        '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF', '-DREMOTEACCESS_REQUIRE_TLS=ON',
        "-DQt6_DIR=$(Join-Path (Split-Path -Parent $QtBin) 'lib\cmake\Qt6')",
        "-DCMAKE_CXX_COMPILER=$compiler", "-DCMAKE_MAKE_PROGRAM=$make"
    )
    Invoke-Checked $cmake @('--build', $BuildDirectory, '--target', 'RemoteAccessApp', '--parallel', "$Jobs")

    $executable = Join-Path $stage 'RemoteAccessApp.exe'
    Copy-Item -LiteralPath (Join-Path $BuildDirectory 'RemoteAccessApp.exe') -Destination $executable
    Invoke-Checked $deployQt @('--release', '--compiler-runtime', '--no-translations', $executable)
    # Use the Windows TLS backend and the Windows trust store, without requiring OpenSSL.
    $unusedOpenSslBackend = Join-Path $stage 'tls\qopensslbackend.dll'
    if (Test-Path -LiteralPath $unusedOpenSslBackend) { Remove-Item -LiteralPath $unusedOpenSslBackend }
    foreach ($relativePath in @(
        'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Network.dll', 'Qt6Widgets.dll', 'Qt6Svg.dll',
        'platforms\qwindows.dll', 'tls\qschannelbackend.dll', 'imageformats\qjpeg.dll',
        'libstdc++-6.dll', 'libgcc_s_seh-1.dll', 'libwinpthread-1.dll'
    )) {
        if (!(Test-Path -LiteralPath (Join-Path $stage $relativePath) -PathType Leaf)) {
            throw "windeployqt did not deploy $relativePath."
        }
    }

    # windeployqt places plugin directories next to the executable.
    @('[Paths]', 'Plugins=.') | Set-Content -LiteralPath (Join-Path $stage 'qt.conf') -Encoding Ascii
    @(
        'Extract the whole ZIP and open RemoteAccessApp.exe.'
        'Edit .env next to RemoteAccessApp.exe, then restart the app to change the server.'
        'Select Enable sharing on the CHILD before starting remote control.'
        ''
        "Build platform: Windows x64, Qt $qtVersion"
    ) | Set-Content -LiteralPath (Join-Path $stage 'README.txt') -Encoding Ascii
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'client-third-party.txt') -Destination (Join-Path $stage 'THIRD-PARTY.txt')
    Add-Content -LiteralPath (Join-Path $stage 'THIRD-PARTY.txt') -Value @('', "Qt runtime version: $qtVersion")
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'licenses') -Destination $stage -Recurse

    $archive = Join-Path $temporary 'RemoteAccessWin.zip'
    Invoke-Checked $tar @('-a', '-c', '-f', $archive, '-C', $temporary, 'RemoteAccessWin')
    $contents = & $tar -tf $archive
    if ($LASTEXITCODE -ne 0 -or $contents -notcontains 'RemoteAccessWin/.env') {
        throw 'The release ZIP is invalid or missing .env.'
    }
    $output = Join-Path $dist 'RemoteAccessWin.zip'
    Move-Item -LiteralPath $archive -Destination $output -Force
    Write-Output "Created $output"
}
finally {
    $env:PATH = $originalPath
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
