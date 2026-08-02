@echo off
start "BMW Remote Simulator" powershell.exe -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0simulator-gui.ps1"
