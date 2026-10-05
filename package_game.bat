@echo off
title Packaging Voidfall Dredge...
echo ================================================================
echo   PACKAGING VOIDFALL DREDGE FOR RELEASE / DISTRIBUTION
echo ================================================================
python "%~dp0scripts\package_release.py"
if %ERRORLEVEL% EQU 0 (
    echo.
    echo [OK] Standalone package and ZIP generated successfully in dist\
    echo      Target: dist\VoidfallDredge_v1.0.zip
) else (
    echo.
    echo [ERROR] Packaging failed with error code %ERRORLEVEL%.
)
echo.
pause
