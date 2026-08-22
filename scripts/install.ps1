$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$name = "obs-selected-windows-moncap"

$candidates = @(
    (Join-Path $root "build_x64\rundir\RelWithDebInfo\$name.dll"),
    (Join-Path $root "build_x64\RelWithDebInfo\$name.dll")
)

$dll = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $dll) {
    throw "Built plugin DLL not found. Run scripts\build.ps1 first."
}

$pdb = [System.IO.Path]::ChangeExtension($dll, ".pdb")
$data = Join-Path $root "data"

# OBS 31 on Windows loads extra plugins from ProgramData.
# AppData is copied too for tools that look there; OBS 31 itself uses ProgramData.
$destinations = @(
    (Join-Path $env:ALLUSERSPROFILE "obs-studio\plugins\$name"),
    (Join-Path $env:APPDATA "obs-studio\plugins\$name")
)

foreach ($dest in $destinations) {
    New-Item -ItemType Directory -Force -Path (Join-Path $dest "bin\64bit") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $dest "data") | Out-Null
    Copy-Item -Force $dll (Join-Path $dest "bin\64bit\$name.dll")
    if (Test-Path $pdb) {
        Copy-Item -Force $pdb (Join-Path $dest "bin\64bit\$name.pdb")
    }
    Copy-Item -Force -Recurse (Join-Path $data "*") (Join-Path $dest "data")
    Write-Host "Installed to $dest"
}

Write-Host "Restart OBS Studio and add the 'Selected Windows Capture' source."
