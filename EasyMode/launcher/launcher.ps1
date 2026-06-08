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

  Usage (from PowerShell, or via launcher.bat by double-click):
    .\launcher.ps1                 # set up if needed, then supervise (foreground)
    .\launcher.ps1 -Background     # detach and supervise in the background
    .\launcher.ps1 -Setup          # force the setup wizard, then exit
    .\launcher.ps1 -Stop           # stop a running background supervisor
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
$RepoUrl    = if ($env:LGTV_EASY_REPO)   { $env:LGTV_EASY_REPO }   else { "https://github.com/routine88/lgtvcompanion-easier.git" }
$RepoBranch = if ($env:LGTV_EASY_BRANCH) { $env:LGTV_EASY_BRANCH } else { "claude/great-goldberg-a190g3" }
$AppHome    = if ($env:LGTV_EASY_APP_HOME) { $env:LGTV_EASY_APP_HOME } else { Join-Path $env:LOCALAPPDATA "lgtv-companion-easy\app" }
$StateDir   = if ($env:LGTV_EASY_HOME) { $env:LGTV_EASY_HOME } else { Join-Path $env:APPDATA "LGTV Companion Easy Mode" }
$UpdateEvery = if ($env:LGTV_EASY_UPDATE_INTERVAL) { [int]$env:LGTV_EASY_UPDATE_INTERVAL } else { 3600 }
$SubDir = "EasyMode"

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

# ---- dependency installation ------------------------------------------------
function Install-Deps {
    if (-not (Have "winget")) {
        Log "winget not found. Please install Git and Python 3 manually from python.org and git-scm.com."
    }
    if (-not (Have "git")) {
        Log "Installing Git via winget..."
        winget install --id Git.Git -e --source winget --accept-source-agreements --accept-package-agreements 2>&1 | Out-Null
    }
    if (-not (Have "python")) {
        Log "Installing Python 3 via winget..."
        winget install --id Python.Python.3.12 -e --source winget --accept-source-agreements --accept-package-agreements 2>&1 | Out-Null
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" +
                    [System.Environment]::GetEnvironmentVariable("Path","User")
    }
    # tkinter check (ships with the official installer).
    & python -c "import tkinter" 2>$null
    if ($LASTEXITCODE -ne 0) {
        Log "WARNING: tkinter not available in this Python. The graphical wizard needs it; the text wizard still works."
    }
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
    $repoLauncher = Join-Path $AppHome "$SubDir\launcher\launcher.ps1"
    if (-not (Test-Path $repoLauncher)) { return }
    if (-not $SelfPath) { return }
    try { $selfFull = (Resolve-Path $SelfPath).Path } catch { return }
    try { $repoFull = (Resolve-Path $repoLauncher).Path } catch { return }
    if ($selfFull -ine $repoFull) {
        # Started from a bootstrap copy: hand off to the canonical repo copy.
        Log "Handing off to the canonical repo launcher."
        $argList = @("-ExecutionPolicy","Bypass","-File",$repoFull) + $script:ForwardArgs
        Start-Process -FilePath "powershell.exe" -ArgumentList $argList
        exit 0
    }
    # We ARE the repo copy; if git rewrote it underneath us, re-launch it.
    if ((Get-FileHash $selfFull).Hash -ne $LauncherStartHash) {
        Log "Launcher updated itself; re-launching new version."
        $argList = @("-ExecutionPolicy","Bypass","-File",$selfFull) + $script:ForwardArgs
        Start-Process -FilePath "powershell.exe" -ArgumentList $argList
        exit 0
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
    $lastUpdate = Get-Date
    try {
        while ($true) {
            Log "Starting idle daemon."
            $proc = Start-Process -FilePath "python" -ArgumentList @("-m","lgtv_easy","run") `
                -WorkingDirectory (App-Dir) -NoNewWindow -PassThru `
                -RedirectStandardError $LogFile -RedirectStandardOutput $LogFile
            while (-not $proc.HasExited) {
                Start-Sleep -Seconds 15
                if (((Get-Date) - $lastUpdate).TotalSeconds -ge $UpdateEvery) {
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

Install-Deps
Sync-Repo | Out-Null
Maybe-SelfUpdate

if ($Setup) {
    Log "Running setup wizard (forced)."
    Run-Cli @("wizard")
    exit 0
}

if ($Supervise) { Start-Supervisor; exit 0 }

if ($Background) {
    if (Test-Path $PidFile) {
        $oldPid = Get-Content $PidFile
        if (Get-Process -Id $oldPid -ErrorAction SilentlyContinue) {
            Log "Already running in background (pid $oldPid)."; exit 0
        }
    }
    if (Needs-Setup) { Log "First run: setup wizard."; Run-Cli @("wizard") }
    Log "Detaching to background. Log: $LogFile"
    $repoSelf = Join-Path $AppHome "$SubDir\launcher\launcher.ps1"
    $useSelf = if (Test-Path $repoSelf) { $repoSelf } else { $PSCommandPath }
    Start-Process -FilePath "powershell.exe" -WindowStyle Hidden `
        -ArgumentList @("-ExecutionPolicy","Bypass","-File",$useSelf,"-Supervise")
    exit 0
}

# Default: foreground. Run setup first if needed, then supervise.
if (Needs-Setup) {
    Log "First run: launching setup wizard."
    Run-Cli @("wizard")
    if (Needs-Setup) { Log "Setup not completed."; exit 1 }
}
Start-Supervisor
