<#
  LGTV Companion Easy Mode - self-updating launcher (Windows / PowerShell)
  ---------------------------------------------------------------------------
  One script that:
    1. Installs dependencies (git + Python, via winget; tkinter ships with the
       official Python installer).
    2. Clones or updates the app from GitHub - INCLUDING updates to this very
       launcher (it re-runs itself if the script changed).
    3. Runs the setup wizard on first use.
    4. Supervises the idle daemon in the background, restarting it if it crashes
       and periodically pulling updates. All errors go to a persistent log.

  Usage (from PowerShell, or just double-click LGTV-Easy-Mode-WINDOWS.bat):
    .\LGTV-Easy-Mode-WINDOWS.ps1              # set up if needed, then supervise
    .\LGTV-Easy-Mode-WINDOWS.ps1 -Background  # detach and supervise in background
    .\LGTV-Easy-Mode-WINDOWS.ps1 -Setup       # force the setup wizard, then exit
    .\LGTV-Easy-Mode-WINDOWS.ps1 -Stop        # stop a running background supervisor
#>
[CmdletBinding()]
param(
    [switch]$Background,
    [switch]$Supervise,
    [switch]$Setup,
    [switch]$Stop
)

$ErrorActionPreference = "Continue"

# ---- configuration ----------------------------------------------------------
$RepoUrl    = if ($env:LGTV_EASY_REPO)   { $env:LGTV_EASY_REPO }   else { "https://github.com/JPersson77/LGTVCompanion.git" }
# Track the repository's default branch (master). Override with LGTV_EASY_BRANCH.
$RepoBranch = if ($env:LGTV_EASY_BRANCH) { $env:LGTV_EASY_BRANCH } else { "master" }
$AppHome    = if ($env:LGTV_EASY_APP_HOME) { $env:LGTV_EASY_APP_HOME } else { Join-Path $env:LOCALAPPDATA "lgtv-companion-easy\app" }
$StateDir   = if ($env:LGTV_EASY_HOME) { $env:LGTV_EASY_HOME } else { Join-Path $env:APPDATA "LGTV Companion Easy Mode" }
$UpdateEvery = if ($env:LGTV_EASY_UPDATE_INTERVAL) { [int]$env:LGTV_EASY_UPDATE_INTERVAL } else { 3600 }
# Set LGTV_EASY_NO_UPDATE=1 to freeze the code: no git fetch/clone, no
# self-update, no periodic pulls. Run only the code already on disk.
$NoUpdate = ($env:LGTV_EASY_NO_UPDATE -eq "1")
# The Python app lives in EasyMode/; this launcher lives at the repo root.
$SubDir = "EasyMode"
$LauncherName = "LGTV-Easy-Mode-WINDOWS.ps1"

$LogFile = Join-Path $StateDir "launcher.log"
$PidFile = Join-Path $StateDir "launcher.pid"
New-Item -ItemType Directory -Force -Path $StateDir | Out-Null

# Hash of this script when we started, to detect a git update rewriting it.
$SelfPath = $PSCommandPath
$LauncherStartHash = if ($SelfPath -and (Test-Path $SelfPath)) {
    (Get-FileHash $SelfPath).Hash
} else { "none" }

function Log($msg) {
    $line = "{0} [launcher] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $msg
    Add-Content -Path $LogFile -Value $line
    Write-Host $line
}

function Have($cmd) { [bool](Get-Command $cmd -ErrorAction SilentlyContinue) }

# Print-only banners. The actual "keep the window open" pause lives in the .bat
# (the single, version-robust place a double-click goes through), so these just
# explain what happened; they must NOT block, or we'd pause twice.
function Pause-BeforeExit {
    Write-Host ""
    Write-Host "----------------------------------------------------------------------"
    Write-Host "  Setup did not finish. The diagnostics above (and the log file"
    Write-Host "  $LogFile) can be shared to get help."
    Write-Host "----------------------------------------------------------------------"
}

# Positive confirmation for the common "already set up" case: the watcher runs as
# a detached background process. Closing the window does NOT stop it.
function Pause-Info([string[]]$lines) {
    Write-Host ""
    Write-Host "======================================================================"
    foreach ($l in $lines) { Write-Host "  $l" }
    Write-Host "======================================================================"
}

# ---- dependency installation ------------------------------------------------
function Refresh-Path {
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("Path","User")
}

function Install-Deps {
    if (-not (Have "winget")) {
        Log "winget not found. Please install Git and Python 3 manually from python.org and git-scm.com."
    }
    if (-not (Have "git")) {
        Log "Installing Git via winget..."
        winget install --id Git.Git -e --source winget --accept-source-agreements --accept-package-agreements 2>&1 | Out-Null
        Refresh-Path
    }
    if (-not (Have "python")) {
        Log "Installing Python 3 via winget..."
        winget install --id Python.Python.3.12 -e --source winget --accept-source-agreements --accept-package-agreements 2>&1 | Out-Null
        Refresh-Path
    }
    # tkinter check (ships with the official installer).
    & python -c "import tkinter" 2>$null
    if ($LASTEXITCODE -ne 0) {
        Log "WARNING: tkinter not available in this Python. The graphical wizard needs it; the text wizard still works."
    }
}

# Report what we actually have to work with - the first thing to check when the
# program "won't launch" is whether Python and Git are even on PATH.
function Log-Diagnostics {
    if (Have "python") {
        Log ("Python: " + ((& python --version 2>&1) -join " "))
    } else {
        Log "Python: NOT FOUND on PATH."
    }
    if (Have "git") {
        Log ("Git: " + ((& git --version 2>&1) -join " "))
    } else {
        Log "Git: NOT FOUND on PATH."
    }
    Log "App folder: $AppHome"
    Log "Log file  : $LogFile"
}

# If Python still isn't usable we cannot run the app at all - say so plainly and
# keep the window open, instead of failing somewhere deeper with a cryptic error.
function Require-Python {
    if (Have "python") {
        & python -c "import sys" 2>$null
        if ($LASTEXITCODE -eq 0) { return }
    }
    Log "ERROR: Python 3 is required but was not found (or won't run)."
    Write-Host ""
    Write-Host "Python 3 could not be found on this PC. Easy Mode needs it to run."
    Write-Host "Fix: install Python 3 from https://www.python.org/downloads/ and,"
    Write-Host "on the first installer screen, TICK 'Add python.exe to PATH'."
    Write-Host "Then run this launcher again."
    Pause-BeforeExit
    exit 1
}

# ---- repository / self-update ----------------------------------------------
function Sync-Repo {
    if (-not (Test-Path (Join-Path $AppHome ".git"))) {
        Log "Cloning $RepoUrl into $AppHome"
        New-Item -ItemType Directory -Force -Path (Split-Path $AppHome) | Out-Null
        git clone --branch $RepoBranch $RepoUrl $AppHome 2>&1 | Add-Content $LogFile
        return ($LASTEXITCODE -eq 0)
    }
    Log "Updating from GitHub ($RepoBranch)"
    git -C $AppHome fetch --quiet origin $RepoBranch 2>&1 | Add-Content $LogFile
    if ($LASTEXITCODE -ne 0) { Log "Fetch failed (offline?); using local copy."; return $true }
    git -C $AppHome checkout --quiet $RepoBranch 2>&1 | Add-Content $LogFile
    git -C $AppHome reset --hard "origin/$RepoBranch" 2>&1 | Add-Content $LogFile
    return $true
}

function Maybe-SelfUpdate {
    $repoLauncher = Join-Path $AppHome $LauncherName
    if (-not (Test-Path $repoLauncher)) { return }
    if (-not $SelfPath) { return }
    try { $selfFull = (Resolve-Path $SelfPath).Path } catch { return }
    try { $repoFull = (Resolve-Path $repoLauncher).Path } catch { return }
    if ($selfFull -ine $repoFull) {
        # Started from a bootstrap/Desktop copy: run the up-to-date repo copy IN
        # THIS SAME WINDOW and adopt its exit code. Running it in place (rather
        # than Start-Process, which opens a detached window and loses the exit
        # code) keeps everything in one console, so the .bat's "keep window open
        # on failure" safety net still works and nothing flashes past.
        Log "Running the up-to-date launcher from $AppHome."
        $fwd = @($script:ForwardArgs)
        $env:LGTV_EASY_HANDOFF = "1"
        & powershell.exe -ExecutionPolicy Bypass -NoProfile -File $repoFull @fwd
        exit $LASTEXITCODE
    }
    # We ARE the repo copy; if git rewrote it underneath us, re-run the new one.
    if ((Get-FileHash $selfFull).Hash -ne $LauncherStartHash) {
        Log "Launcher updated itself; re-running the new version."
        $fwd = @($script:ForwardArgs)
        $env:LGTV_EASY_HANDOFF = "1"
        if ($Supervise) {
            # The hidden background supervisor restarts itself detached, so the
            # old process doesn't linger waiting on the new one.
            Start-Process -FilePath "powershell.exe" -WindowStyle Hidden `
                -ArgumentList (@("-ExecutionPolicy","Bypass","-NoProfile","-File",$selfFull) + $fwd)
            exit 0
        }
        & powershell.exe -ExecutionPolicy Bypass -NoProfile -File $selfFull @fwd
        exit $LASTEXITCODE
    }
}

function App-Dir { Join-Path $AppHome $SubDir }
function Run-Cli([string[]]$cliArgs) {
    Push-Location (App-Dir)
    try { & python -m lgtv_easy @cliArgs } finally { Pop-Location }
}

function Needs-Setup {
    $cfg = Join-Path $StateDir "config.json"
    if (-not (Test-Path $cfg)) { return $true }
    try {
        $j = Get-Content $cfg -Raw | ConvertFrom-Json
        return -not ($j.setup_complete -and $j.device.key)
    } catch { return $true }
}

# ---- supervisor loop --------------------------------------------------------
function Start-Supervisor {
    Set-Content -Path $PidFile -Value $PID
    Log "Supervisor started (pid $PID). Daemon errors are logged here."
    # If another watcher (e.g. the login auto-start) already holds the lock, our
    # daemon child should wait for it rather than spin-restart.
    $env:LGTV_EASY_WAIT_LOCK = "1"
    $lastUpdate = Get-Date
    try {
        while ($true) {
            Log "Starting idle daemon."
            $proc = Start-Process -FilePath "python" -ArgumentList @("-m","lgtv_easy","run") `
                -WorkingDirectory (App-Dir) -NoNewWindow -PassThru `
                -RedirectStandardError $LogFile -RedirectStandardOutput $LogFile
            while (-not $proc.HasExited) {
                Start-Sleep -Seconds 15
                if (-not $NoUpdate -and ((Get-Date) - $lastUpdate).TotalSeconds -ge $UpdateEvery) {
                    $lastUpdate = Get-Date
                    Log "Periodic update check."
                    if (Sync-Repo) {
                        Maybe-SelfUpdate
                        Log "Restarting daemon to apply updates."
                        try { $proc.Kill() } catch {}
                    }
                }
            }
            Log "Daemon exited (code $($proc.ExitCode)). Restarting in 5s."
            Start-Sleep -Seconds 5
        }
    } finally {
        Remove-Item $PidFile -ErrorAction SilentlyContinue
    }
}

function Stop-Background {
    if (Test-Path $PidFile) {
        $oldPid = Get-Content $PidFile
        try {
            Stop-Process -Id $oldPid -ErrorAction Stop
            Log "Stopped background supervisor (pid $oldPid)."
        } catch { Log "No running supervisor with pid $oldPid." }
        Remove-Item $PidFile -ErrorAction SilentlyContinue
    } else { Log "No background supervisor found." }
}

# ---- main -------------------------------------------------------------------
$script:ForwardArgs = @()
if ($Supervise)  { $script:ForwardArgs += "-Supervise" }
if ($Background) { $script:ForwardArgs += "-Background" }
if ($Setup)      { $script:ForwardArgs += "-Setup" }

if ($Stop) { Stop-Background; exit 0 }

# Catch-all backstop: any unexpected error prints clearly and keeps the window
# open, rather than the console vanishing before it can be read.
trap {
    Log "FATAL: $($_.Exception.Message)"
    Write-Host ""
    Write-Host "Unexpected error while starting Easy Mode:"
    Write-Host "  $($_.Exception.Message)"
    if ($_.ScriptStackTrace) { Write-Host $_.ScriptStackTrace }
    Pause-BeforeExit
    exit 1
}

# The bootstrap copy installs deps and self-updates, then hands off to the
# up-to-date internal copy (LGTV_EASY_HANDOFF=1) - which skips redoing all that.
if ($env:LGTV_EASY_HANDOFF -eq "1") {
    Log "Running the up-to-date launcher."
} else {
    Install-Deps
    Log-Diagnostics
    Require-Python
    if ($NoUpdate) {
        Log "Auto-update disabled (LGTV_EASY_NO_UPDATE=1); using the on-disk copy."
    } else {
        Sync-Repo | Out-Null
        Maybe-SelfUpdate
    }
}

if ($Setup) {
    Log "Running setup wizard (forced)."
    Run-Cli @("wizard")
    if (Needs-Setup) { Pause-BeforeExit; exit 1 }
    exit 0
}

if ($Supervise) { Start-Supervisor; exit 0 }

if ($Background) {
    # A manual launch is a little control panel: open the setup/settings wizard
    # (quick when already set up - it just asks what to change), then make sure
    # the background watcher is running.
    Log "Opening setup/settings wizard."
    Run-Cli @("wizard")
    if (Needs-Setup) { Log "Setup not completed."; Pause-BeforeExit; exit 1 }

    if (Test-Path $PidFile) {
        $oldPid = Get-Content $PidFile
        if (Get-Process -Id $oldPid -ErrorAction SilentlyContinue) {
            Log "Watcher already running (pid $oldPid)."
            Pause-Info @("Settings saved. Easy Mode is already running in the background (pid $oldPid).",
                         "Your LG TV will blank/sleep when the PC is idle.",
                         "To stop it: run this launcher again with  -Stop")
            exit 0
        }
    }
    Log "Detaching watcher to background. Log: $LogFile"
    $repoSelf = Join-Path $AppHome $LauncherName
    $useSelf = if (Test-Path $repoSelf) { $repoSelf } else { $PSCommandPath }
    Start-Process -FilePath "powershell.exe" -WindowStyle Hidden `
        -ArgumentList @("-ExecutionPolicy","Bypass","-File",$useSelf,"-Supervise")
    Pause-Info @("Settings saved. Easy Mode is now running in the background.",
                 "Your LG TV will blank/sleep when the PC is idle, and wake when you",
                 "move the mouse or press a key.",
                 "Closing this window does NOT stop it. To stop: run with  -Stop")
    exit 0
}

# Default: foreground. Run setup first if needed, then supervise.
if (Needs-Setup) {
    Log "First run: launching setup wizard."
    Run-Cli @("wizard")
    if (Needs-Setup) { Log "Setup not completed."; Pause-BeforeExit; exit 1 }
}
Start-Supervisor
