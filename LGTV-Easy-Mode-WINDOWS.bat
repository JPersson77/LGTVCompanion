@echo off
REM ===========================================================================
REM  LGTV Companion Easy Mode - Windows launcher  (just double-click this file)
REM ===========================================================================
REM  This file is a tiny, stable shim: it finds the real PowerShell launcher and
REM  runs it. It deliberately prefers the SELF-UPDATING internal copy (kept in
REM  %LOCALAPPDATA%) over the copy next to this file, so all the real logic stays
REM  current even though this .bat never changes. The window is kept open at the
REM  end so it can never just vanish on a double-click.
setlocal
set "APP_PS1=%LOCALAPPDATA%\lgtv-companion-easy\app\LGTV-Easy-Mode-WINDOWS.ps1"
set "LOCAL_PS1=%~dp0LGTV-Easy-Mode-WINDOWS.ps1"

REM Prefer the auto-updated internal copy; fall back to the one beside this file
REM (needed for the very first run, before anything has been cloned).
if exist "%APP_PS1%" (
  set "PS1=%APP_PS1%"
) else (
  set "PS1=%LOCAL_PS1%"
)

if not exist "%PS1%" (
  echo.
  echo ERROR: Could not find the PowerShell launcher:
  echo   "%PS1%"
  echo Make sure LGTV-Easy-Mode-WINDOWS.bat and LGTV-Easy-Mode-WINDOWS.ps1
  echo are BOTH in the same folder, then run this again.
  echo.
  pause
  exit /b 1
)

echo Starting LGTV Companion Easy Mode... (this window will stay open)
echo.
powershell.exe -ExecutionPolicy Bypass -NoProfile -File "%PS1%" -Background %*
set "RC=%ERRORLEVEL%"

echo.
if not "%RC%"=="0" (
  echo ----------------------------------------------------------------------
  echo  The launcher exited with an error ^(code %RC%^).
  echo  Scroll up to read the messages above - they explain what went wrong,
  echo  and can be shared to get help.
  echo ----------------------------------------------------------------------
)
REM Always pause so the window never just vanishes on a double-click. The TV
REM watcher, when started, runs as a separate background process and keeps
REM running after this window is closed.
pause
endlocal
