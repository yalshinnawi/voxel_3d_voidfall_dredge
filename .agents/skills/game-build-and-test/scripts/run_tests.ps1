# Automated Test Runner for Voidfall Dredge
param (
    [string]$Config = "Release",
    [switch]$StopOnFailure
)

$ErrorActionPreference = "Continue"
$Root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
if (-not (Test-Path "$Root\CMakeLists.txt")) {
    $Root = (Get-Location).Path
}

$BinDir = "$Root\build\$Config"
$Tests = @("test_unit_all.exe", "test_e2e_expeditions.exe", "test_progression.exe")

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Voidfall Dredge - Test Suite Runner ($Config)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

$Passed = 0
$Failed = 0

foreach ($Test in $Tests) {
    $ExePath = "$BinDir\$Test"
    if (-not (Test-Path $ExePath)) {
        Write-Host "[-] SKIP: $Test not found in $BinDir" -ForegroundColor Yellow
        continue
    }

    Write-Host "[>] Running $Test..." -ForegroundColor White
    $Proc = Start-Process -FilePath $ExePath -WorkingDirectory $BinDir -NoNewWindow -PassThru -Wait
    
    if ($Proc.ExitCode -eq 0) {
        Write-Host "[+] PASS: $Test" -ForegroundColor Green
        $Passed++
    } else {
        Write-Host "[x] FAIL: $Test (Exit code: $($Proc.ExitCode))" -ForegroundColor Red
        $Failed++
        if ($StopOnFailure) {
            break
        }
    }
}

Write-Host "------------------------------------------" -ForegroundColor Cyan
Write-Host "Summary: $Passed Passed, $Failed Failed" -ForegroundColor $(if ($Failed -eq 0) { "Green" } else { "Red" })
Write-Host "==========================================" -ForegroundColor Cyan

exit $Failed
