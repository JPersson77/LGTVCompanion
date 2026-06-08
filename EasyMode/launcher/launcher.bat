@echo off
REM LGTV Companion Easy Mode - double-clickable Windows launcher.
REM Just runs the PowerShell launcher in background mode. First run shows the
REM setup wizard; after that it quietly keeps your TV sleeping when you're away.
setlocal
set SCRIPT_DIR=%~dp0
powershell.exe -ExecutionPolicy Bypass -NoProfile -File "%SCRIPT_DIR%launcher.ps1" -Background %*
endlocal
