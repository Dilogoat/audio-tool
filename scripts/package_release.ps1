param(
    [string]$Version = "0.3.0",
    [string]$Preset = "default",
    [switch]$SkipBuild,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not $SkipBuild) {
    cmake --preset $Preset
    cmake --build --preset $Preset
}

if (-not $SkipTests) {
    ctest --preset $Preset --output-on-failure
}

$dist = Join-Path $root "dist"
$build = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $dist | Out-Null

Copy-Item (Join-Path $build "audio_tool.exe") $dist -Force
Copy-Item (Join-Path $build "audio_tool_cli.exe") $dist -Force
if (Test-Path (Join-Path $dist "presets")) { Remove-Item (Join-Path $dist "presets") -Recurse -Force }
Copy-Item (Join-Path $root "presets") (Join-Path $dist "presets") -Recurse -Force

$requiredDlls = @(
    "FLAC.dll", "fftw3.dll", "glfw3.dll", "libmp3lame.DLL", "mpg123.dll", "ogg.dll",
    "opus.dll", "portaudio.dll", "sndfile.dll", "vorbis.dll", "vorbisenc.dll"
)
$binDir = Join-Path $build "vcpkg_installed\x64-windows\bin"
foreach ($dll in $requiredDlls) {
    $src = Join-Path $binDir $dll
    if (Test-Path $src) {
        Copy-Item $src $dist -Force
    }
}

$zipName = "audio-tool-v$Version-win64.zip"
$zipPath = Join-Path $root $zipName
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path (Join-Path $dist "*") -DestinationPath $zipPath -Force

Write-Host "Packaged $zipPath"
