@echo off
REM ===========================================================================
REM  LGTV Companion Easy Mode - Windows launcher  (just double-click this file)
REM ===========================================================================
REM  Runs the PowerShell launcher in background mode. The first run shows the
REM  setup wizard; after that it quietly keeps your TV sleeping when you're away,
REM  and self-updates from GitHub's default branch.
REM
REM  This .bat deliberately keeps the window OPEN if anything fails, so error
REM  messages and setup diagnostics can be read and reported.
setlocal
set "SCRIPT_DIR=%~dp0"
set "PS1=%SCRIPT_DIR%LGTV-Easy-Mode-WINDOWS.ps1"

if not exist "%PS1%" (
  echo.
  echo ERROR: Could not find the PowerShell launcher next to this file:
  echo   "%PS1%"
  echo Make sure LGTV-Easy-Mode-WINDOWS.bat and LGTV-Easy-Mode-WINDOWS.ps1
  echo are BOTH in the same folder, then run this again.
  echo.
  pause
  exit /b 1
)

powershell.exe -ExecutionPolicy Bypass -NoProfile -File "%PS1%" -Background %*
set "RC=%ERRORLEVEL%"

if not "%RC%"=="0" (
  echo.
  echo ----------------------------------------------------------------------
  echo  The launcher exited with an error ^(code %RC%^).
  echo  Scroll up to read the messages above - they explain what went wrong.
  echo  This window is staying open so nothing is lost.
  echo ----------------------------------------------------------------------
  pause
)
endlocal
