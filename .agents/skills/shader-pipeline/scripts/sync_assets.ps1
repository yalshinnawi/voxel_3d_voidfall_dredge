# Rapid Asset & Shader Sync Script
param (
    [string]$Config = "Release"
)

$Root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
if (-not (Test-Path "$Root\assets")) {
    $Root = (Get-Location).Path
}

$Src = "$Root\assets"
$Dst = "$Root\build\$Config\assets"

if (-not (Test-Path $Dst)) {
    New-Item -ItemType Directory -Path $Dst -Force | Out-Null
}

Write-Host "[>] Mirroring assets from $Src to $Dst..." -ForegroundColor Cyan
Copy-Item -Path "$Src\*" -Destination $Dst -Recurse -Force
Write-Host "[+] Asset mirror complete. Shaders & textures are live in $Config." -ForegroundColor Green
