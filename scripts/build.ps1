$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake is not on PATH"
}

Write-Host "Configuring windows-x64 preset..."
cmake --preset windows-x64
if ($LASTEXITCODE -ne 0) {
    throw "cmake configure failed with exit code $LASTEXITCODE"
}

Write-Host "Building RelWithDebInfo..."
cmake --build --preset windows-x64
if ($LASTEXITCODE -ne 0) {
    throw "cmake build failed with exit code $LASTEXITCODE"
}

Write-Host "Build finished. DLL rundir: $root\build_x64\rundir\RelWithDebInfo"
