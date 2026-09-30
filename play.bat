@echo off
cd /d "%~dp0"
if exist "build\Release\VoidfallDredge.exe" (
    start "" "build\Release\VoidfallDredge.exe" %*
) else if exist "build\VoidfallDredge.exe" (
    start "" "build\VoidfallDredge.exe" %*
) else (
    echo Error: VoidfallDredge.exe not found. Please build the project first.
    pause
)
