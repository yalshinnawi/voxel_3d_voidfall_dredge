@echo off
title Voidfall Dredge - Join Multiplayer
set /p SERVER_IP="Enter Host IP address (default 127.0.0.1): "
if "%SERVER_IP%"=="" set SERVER_IP=127.0.0.1
start "" "%~dp0VoidfallDredge.exe" --client %SERVER_IP% --port 27015
