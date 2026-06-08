@echo off
REM ===========================================================================
REM  LGTV Companion Easy Mode - Windows launcher  (just double-click this file)
REM ===========================================================================
REM  Runs the PowerShell launcher in background mode. The first run shows the
REM  setup wizard; after that it quietly keeps your TV sleeping when you're away,
REM  and self-updates from GitHub's default branch.
setlocal
set SCRIPT_DIR=%~dp0
powershell.exe -ExecutionPolicy Bypass -NoProfile -File "%SCRIPT_DIR%LGTV-Easy-Mode-WINDOWS.ps1" -Background %*
endlocal
