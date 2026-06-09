#!/usr/bin/env bash
#
# LGTV Companion Easy Mode - self-updating launcher (Ubuntu / Linux)
# -----------------------------------------------------------------------------
# This is the ONE file Linux users run. It:
#   1. Installs dependencies (git, python3, tkinter for the GUI).
#   2. Clones or updates the app from GitHub's default branch - INCLUDING
#      updates to this very launcher (it re-executes itself if it changed).
#   3. Runs the setup wizard on first use.
#   4. Supervises the idle daemon in the background, restarting it if it crashes
#      and periodically pulling updates. All errors go to a persistent log.
#
# Usage:
#   ./LGTV-Easy-Mode-UBUNTU.sh              # set up (if needed), run in foreground
#   ./LGTV-Easy-Mode-UBUNTU.sh --background # detach and run as a background daemon
#   ./LGTV-Easy-Mode-UBUNTU.sh --setup      # force the setup wizard, then exit
#   ./LGTV-Easy-Mode-UBUNTU.sh --stop       # stop a running background supervisor
#
# Safe to re-run any time; it is idempotent.
# -----------------------------------------------------------------------------
set -uo pipefail

# ---- configuration ----------------------------------------------------------
REPO_URL="${LGTV_EASY_REPO:-https://github.com/routine88/lgtvcompanion-easier.git}"
# Track the repository's default branch (master). Override with LGTV_EASY_BRANCH.
REPO_BRANCH="${LGTV_EASY_BRANCH:-master}"
APP_HOME="${LGTV_EASY_APP_HOME:-$HOME/.local/share/lgtv-companion-easy}"
STATE_DIR="${LGTV_EASY_HOME:-$HOME/.config/lgtv-companion-easy}"
LOG_FILE="$STATE_DIR/launcher.log"
PID_FILE="$STATE_DIR/launcher.pid"
UPDATE_EVERY_SECONDS="${LGTV_EASY_UPDATE_INTERVAL:-3600}"
# Set LGTV_EASY_NO_UPDATE=1 to freeze the code: no git fetch/clone, no
# self-update, no periodic pulls. Run only the code already on disk.
NO_UPDATE="${LGTV_EASY_NO_UPDATE:-0}"
# The Python app lives in the EasyMode/ subdirectory of the repo; this launcher
# lives at the repo root.
SUBDIR="EasyMode"
LAUNCHER_NAME="LGTV-Easy-Mode-UBUNTU.sh"

mkdir -p "$STATE_DIR"

log() { echo "$(date '+%Y-%m-%d %H:%M:%S') [launcher] $*" | tee -a "$LOG_FILE" >&2; }

# Keep the terminal open after a failure so the user can read and report the
# diagnostics printed above (the window otherwise closes the instant we exit).
pause_before_exit() {
  if [ -t 0 ]; then
    echo ""
    echo "----------------------------------------------------------------------"
    echo "  Setup did not finish. The diagnostics above (and the log file"
    echo "  $LOG_FILE) can be shared to get help."
    echo "  This window will stay open so nothing is lost."
    echo "----------------------------------------------------------------------"
    read -r -p "Press Enter to close this window... " _ || true
  fi
}

# Hash of this script as it was when we started, so we can tell if a git update
# rewrote it underneath us and re-execute the new version.
SELF_PATH="$(readlink -f "$0" 2>/dev/null || echo "$0")"
LAUNCHER_START_HASH="$( (sha1sum "$SELF_PATH" 2>/dev/null || echo none) | cut -d' ' -f1)"

# ---- dependency installation ------------------------------------------------
have() { command -v "$1" >/dev/null 2>&1; }

install_deps() {
  local need_pkgs=()
  have git || need_pkgs+=("git")
  have python3 || need_pkgs+=("python3")
  # tkinter is needed for the graphical wizard; the app still works headless.
  python3 -c "import tkinter" >/dev/null 2>&1 || need_pkgs+=("python3-tk")
  # xprintidle gives accurate idle detection on X11 (optional but recommended).
  have xprintidle || need_pkgs+=("xprintidle")

  if [ "${#need_pkgs[@]}" -eq 0 ]; then
    log "All dependencies present."
    return 0
  fi
  log "Installing dependencies: ${need_pkgs[*]}"
  if have apt-get; then
    local SUDO=""; [ "$(id -u)" -ne 0 ] && have sudo && SUDO="sudo"
    $SUDO apt-get update -y -q >>"$LOG_FILE" 2>&1 || log "apt-get update failed (continuing)"
    $SUDO apt-get install -y -q "${need_pkgs[@]}" >>"$LOG_FILE" 2>&1 \
      || log "WARNING: could not install some packages: ${need_pkgs[*]}"
  else
    log "WARNING: apt-get not found. Please install manually: ${need_pkgs[*]}"
  fi
}

# ---- repository / self-update ----------------------------------------------
sync_repo() {
  if [ ! -d "$APP_HOME/.git" ]; then
    log "Cloning $REPO_URL into $APP_HOME"
    git clone --branch "$REPO_BRANCH" "$REPO_URL" "$APP_HOME" >>"$LOG_FILE" 2>&1 \
      || { log "ERROR: clone failed"; return 1; }
  else
    log "Updating from GitHub ($REPO_BRANCH)"
    git -C "$APP_HOME" fetch --quiet origin "$REPO_BRANCH" >>"$LOG_FILE" 2>&1 \
      || { log "WARNING: fetch failed (offline?), using local copy"; return 0; }
    git -C "$APP_HOME" checkout --quiet "$REPO_BRANCH" >>"$LOG_FILE" 2>&1 || true
    git -C "$APP_HOME" reset --hard "origin/$REPO_BRANCH" >>"$LOG_FILE" 2>&1 \
      || log "WARNING: could not fast-forward"
  fi
  return 0
}

# This is how the launcher updates itself after a git pull:
#  - If we were started from a copy outside the repo (a bootstrap), hand off to
#    the canonical repo copy.
#  - If we ARE the repo copy and git rewrote it underneath us, re-exec the new
#    version (detected by comparing the start-time hash to the on-disk hash).
maybe_self_update() {
  local repo_launcher="$APP_HOME/$LAUNCHER_NAME"
  [ -f "$repo_launcher" ] || return 0
  local target; target="$(readlink -f "$repo_launcher")"
  if [ "$SELF_PATH" != "$target" ]; then
    log "Handing off to the canonical repo launcher."
    export LGTV_EASY_HANDOFF=1
    exec "$repo_launcher" "$@"
  fi
  local now_hash; now_hash="$( (sha1sum "$SELF_PATH" 2>/dev/null || echo none) | cut -d' ' -f1)"
  if [ "$now_hash" != "$LAUNCHER_START_HASH" ]; then
    log "Launcher updated itself; re-executing new version."
    export LGTV_EASY_HANDOFF=1
    exec "$SELF_PATH" "$@"
  fi
}

APP_DIR() { echo "$APP_HOME/$SUBDIR"; }

run_cli() { ( cd "$(APP_DIR)" && python3 -m lgtv_easy "$@" ); }

needs_setup() {
  ! python3 - "$STATE_DIR/config.json" <<'PY' 2>/dev/null
import json, sys
try:
    d = json.load(open(sys.argv[1]))
    sys.exit(0 if d.get("setup_complete") and d.get("device", {}).get("key") else 1)
except Exception:
    sys.exit(1)
PY
}

# ---- the supervisor loop ----------------------------------------------------
supervise() {
  echo $$ > "$PID_FILE"
  trap 'log "Supervisor stopping."; rm -f "$PID_FILE"; exit 0' INT TERM
  log "Supervisor started (pid $$). Daemon errors are logged here."
  # If another watcher (e.g. the login auto-start) already holds the lock, our
  # daemon child should wait for it rather than spin-restart.
  export LGTV_EASY_WAIT_LOCK=1
  local last_update; last_update=$(date +%s)

  while true; do
    log "Starting idle daemon."
    # Run the daemon; capture its stderr/stdout into the persistent log.
    ( cd "$(APP_DIR)" && exec python3 -m lgtv_easy run ) >>"$LOG_FILE" 2>&1 &
    local daemon_pid=$!

    # Watch the daemon while periodically checking for updates.
    while kill -0 "$daemon_pid" 2>/dev/null; do
      sleep 15
      local now; now=$(date +%s)
      if [ "$NO_UPDATE" != "1" ] && [ $(( now - last_update )) -ge "$UPDATE_EVERY_SECONDS" ]; then
        last_update=$now
        log "Periodic update check."
        if sync_repo; then
          maybe_self_update "$@"   # may re-exec and replace this process
          # Restart the daemon to pick up any code changes. Use SIGUSR1 so the
          # daemon stops WITHOUT powering off the TV (that's only for real
          # shutdowns, which arrive as SIGTERM).
          log "Restarting daemon to apply updates."
          kill -USR1 "$daemon_pid" 2>/dev/null || kill "$daemon_pid" 2>/dev/null
        fi
      fi
    done

    wait "$daemon_pid" 2>/dev/null
    local rc=$?
    log "Daemon exited (code $rc). Restarting in 5s."
    sleep 5
  done
}

stop_background() {
  if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
    log "Stopping background supervisor (pid $(cat "$PID_FILE"))."
    kill "$(cat "$PID_FILE")"
    rm -f "$PID_FILE"
  else
    log "No running background supervisor found."
  fi
}

# ---- main -------------------------------------------------------------------
main() {
  case "${1:-}" in
    --stop) stop_background; exit 0 ;;
  esac

  # The bootstrap copy installs deps and self-updates, then hands off to the
  # up-to-date internal copy (LGTV_EASY_HANDOFF=1) - which skips redoing all that.
  if [ "${LGTV_EASY_HANDOFF:-0}" = "1" ]; then
    log "Running the up-to-date launcher."
  else
    install_deps
    if [ "$NO_UPDATE" = "1" ]; then
      log "Auto-update disabled (LGTV_EASY_NO_UPDATE=1); using the on-disk copy."
    else
      sync_repo || log "Continuing with existing copy."
      maybe_self_update "$@"
    fi
  fi

  case "${1:-}" in
    --setup)
      log "Running setup wizard (forced)."
      if ! run_cli wizard; then
        pause_before_exit
        exit 1
      fi
      exit 0
      ;;
    --background)
      if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
        log "Already running in background (pid $(cat "$PID_FILE"))."
        exit 0
      fi
      if needs_setup; then
        log "First run: launching setup wizard before backgrounding."
        run_cli wizard
        if needs_setup; then
          log "Setup not completed; not backgrounding."
          pause_before_exit
          exit 1
        fi
      fi
      log "Detaching to background. Log: $LOG_FILE"
      setsid "$0" --supervise </dev/null >>"$LOG_FILE" 2>&1 &
      exit 0
      ;;
    --supervise)
      supervise "$@"
      ;;
    *)
      # A manual run is a control panel: open the setup/settings wizard (quick
      # when already set up), then run the watcher in the foreground.
      log "Opening setup/settings wizard."
      if ! run_cli wizard || needs_setup; then
        log "Setup not completed."
        pause_before_exit
        exit 1
      fi
      supervise "$@"
      ;;
  esac
}

main "$@"
