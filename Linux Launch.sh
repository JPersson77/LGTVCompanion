#!/usr/bin/env bash
#
# LGTV Companion Easy Mode - self-updating launcher (Ubuntu / Linux)
# -----------------------------------------------------------------------------
# This is the ONE file Linux users run. It:
#   1. Installs dependencies (git, python3, tkinter for the GUI).
#   2. Clones or updates the app from GitHub's default branch - INCLUDING
#      updates to this very launcher (it re-executes itself if it changed).
#   3. Opens the graphical setup window on first use (text wizard if headless).
#   4. Supervises the idle daemon in the background, restarting it if it crashes
#      and periodically pulling updates. All errors go to a persistent log.
#
# Usage:
#   ./"Linux Launch.sh"              # set up (if needed), run in foreground
#   ./"Linux Launch.sh" --background # detach and run as a background daemon
#   ./"Linux Launch.sh" --setup      # force the setup wizard, then exit
#   ./"Linux Launch.sh" --stop       # stop a running background supervisor
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
LAUNCHER_NAME="Linux Launch.sh"

mkdir -p "$STATE_DIR"

log() {
  local line; line="$(date '+%Y-%m-%d %H:%M:%S') [launcher] $*"
  printf '%s\n' "$line" >>"$LOG_FILE"
  # Echo to the terminal too, but only when one is attached. In the detached
  # background supervisor stderr is already redirected to the log file, so
  # writing there as well would duplicate every line.
  [ -t 2 ] && printf '%s\n' "$line" >&2
  return 0
}

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
  # gdbus (from glib) is used for Wayland/GNOME idle detection and to notice when
  # the PC suspends so the TV can sleep with it. Optional - the app degrades if
  # it's missing - but recommended.
  have gdbus || need_pkgs+=("libglib2.0-bin")

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
# True if we're a bootstrap copy that should hand off to the canonical repo
# launcher, or git rewrote this launcher underneath us (start-time hash differs
# from the on-disk hash). Either way the running launcher should re-exec.
launcher_changed() {
  local repo_launcher="$APP_HOME/$LAUNCHER_NAME"
  [ -f "$repo_launcher" ] || return 1
  [ "$SELF_PATH" != "$(readlink -f "$repo_launcher")" ] && return 0
  local now_hash; now_hash="$( (sha1sum "$SELF_PATH" 2>/dev/null || echo none) | cut -d' ' -f1)"
  [ "$now_hash" != "$LAUNCHER_START_HASH" ]
}

maybe_self_update() {
  launcher_changed || return 0
  local repo_launcher="$APP_HOME/$LAUNCHER_NAME"
  export LGTV_EASY_HANDOFF=1
  if [ "$SELF_PATH" != "$(readlink -f "$repo_launcher")" ]; then
    log "Handing off to the canonical repo launcher."
    exec "$repo_launcher" "$@"
  fi
  log "Launcher updated itself; re-executing new version."
  exec "$SELF_PATH" "$@"
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
  # Never run two supervisors at once. Re-opening the app runs the default case,
  # which falls through to supervise(); without this guard each open would clobber
  # PID_FILE (orphaning the previous supervisor so --stop can't find it) and spawn
  # another daemon that just blocks forever waiting for the lock the first daemon
  # already holds - so watchers pile up on every re-open. If a live supervisor
  # already owns the pidfile, stand down and let it keep driving the TV.
  if [ -f "$PID_FILE" ]; then
    local existing; existing="$(cat "$PID_FILE" 2>/dev/null || echo)"
    if [ -n "$existing" ] && [ "$existing" != "$$" ] && kill -0 "$existing" 2>/dev/null; then
      log "A background watcher is already running (pid $existing); not starting another."
      return 0
    fi
  fi
  echo $$ > "$PID_FILE"
  local daemon_pid=""
  # Signal handling, kept deliberately distinct because the two outcomes are
  # opposite - and the daemon child reads them the same way:
  #   * SIGUSR1 / SIGINT -> a plain "stop the watcher" (--stop, or Ctrl+C):
  #     leave the TV exactly as it is. We forward SIGUSR1 to the daemon.
  #   * SIGTERM -> a real machine shutdown or logoff: power the TV OFF. We
  #     forward SIGTERM so the daemon's shutdown handler turns it off.
  # Earlier this forwarded SIGUSR1 on *both* INT and TERM ("never power off"),
  # which at real shutdown raced systemd's own SIGTERM to the daemon and usually
  # won - so the daemon exited before powering off and the TV was left on. Now
  # --stop targets the supervisor with SIGUSR1 (see stop_background), leaving
  # SIGTERM to mean shutdown.
  stop_leave_tv() {
    log "Supervisor stopping (leaving the TV as-is)."
    [ -n "$daemon_pid" ] && kill -USR1 "$daemon_pid" 2>/dev/null
    rm -f "$PID_FILE"; exit 0
  }
  stop_power_off() {
    log "Supervisor stopping for shutdown (powering the TV off)."
    [ -n "$daemon_pid" ] && kill -TERM "$daemon_pid" 2>/dev/null
    rm -f "$PID_FILE"; exit 0
  }
  trap stop_leave_tv INT USR1
  trap stop_power_off TERM
  log "Supervisor started (pid $$). Daemon errors are logged here."
  # If another watcher (e.g. the login auto-start) already holds the lock, our
  # daemon child should wait for it rather than spin-restart.
  export LGTV_EASY_WAIT_LOCK=1
  local last_update; last_update=$(date +%s)

  while true; do
    log "Starting idle daemon."
    # Run the daemon; capture its stderr/stdout into the persistent log.
    ( cd "$(APP_DIR)" && exec python3 -m lgtv_easy run ) >>"$LOG_FILE" 2>&1 &
    daemon_pid=$!

    # Watch the daemon while periodically checking for updates.
    while kill -0 "$daemon_pid" 2>/dev/null; do
      # Interruptible sleep: backgrounding sleep and waiting on it lets a stop
      # signal take effect immediately, instead of after the full poll interval
      # (bash defers traps until the current foreground command returns).
      sleep 15 & wait $! 2>/dev/null
      local now; now=$(date +%s)
      if [ "$NO_UPDATE" != "1" ] && [ $(( now - last_update )) -ge "$UPDATE_EVERY_SECONDS" ]; then
        last_update=$now
        log "Periodic update check."
        if sync_repo; then
          if launcher_changed; then
            # The launcher rewrote itself. Re-exec the new version, but resume as
            # a pure --supervise: re-execing with our original args would replay
            # the GUI-opening default case (popping a window and stacking another
            # supervisor). Stop our daemon child first and drop the pidfile so it
            # isn't orphaned behind the exec; the fresh supervisor restarts it.
            log "Launcher updated itself; re-executing as the background watcher."
            kill -USR1 "$daemon_pid" 2>/dev/null
            rm -f "$PID_FILE"
            export LGTV_EASY_HANDOFF=1
            local repo_launcher; repo_launcher="$APP_HOME/$LAUNCHER_NAME"
            if [ -f "$repo_launcher" ] \
               && [ "$SELF_PATH" != "$(readlink -f "$repo_launcher")" ]; then
              exec "$repo_launcher" --supervise
            fi
            exec "$SELF_PATH" --supervise
          fi
          # Code (not the launcher) changed: just restart the daemon to pick it
          # up. SIGUSR1 stops it WITHOUT powering off the TV (that's only for real
          # shutdowns, which arrive as SIGTERM).
          log "Restarting daemon to apply updates."
          kill -USR1 "$daemon_pid" 2>/dev/null || kill "$daemon_pid" 2>/dev/null
        fi
      fi
    done

    wait "$daemon_pid" 2>/dev/null
    local rc=$?
    log "Daemon exited (code $rc). Restarting in 5s."
    sleep 5 & wait $! 2>/dev/null
  done
}

stop_background() {
  local stopped=0
  if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
    local sp; sp="$(cat "$PID_FILE")"
    log "Stopping background supervisor (pid $sp)."
    # SIGUSR1 = "stop the watcher, leave the TV alone". SIGTERM is reserved for a
    # real OS shutdown (power the TV off), so --stop must NOT use it.
    kill -USR1 "$sp" 2>/dev/null
    # Give it up to ~10s to run its trap (stop the daemon) and exit.
    local _i
    for _i in $(seq 1 20); do kill -0 "$sp" 2>/dev/null || break; sleep 0.5; done
    stopped=1
  fi
  # Also stop the idle daemon directly, in case it outlived its supervisor (or
  # was started by the login auto-start, which has no supervisor). SIGUSR1 means
  # "quit without powering off the TV"; fall back to SIGKILL, never SIGTERM
  # (which would power the TV off).
  local dp="$STATE_DIR/daemon.pid"
  if [ -f "$dp" ] && kill -0 "$(cat "$dp")" 2>/dev/null; then
    local d; d="$(cat "$dp")"
    log "Stopping idle daemon (pid $d)."
    kill -USR1 "$d" 2>/dev/null
    sleep 1
    kill -0 "$d" 2>/dev/null && kill -KILL "$d" 2>/dev/null
    stopped=1
  fi
  rm -f "$PID_FILE"
  if [ "$stopped" = "1" ]; then
    log "Easy Mode stopped. Your TV is left as-is."
  else
    log "No running background watcher found."
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
      log "Opening the setup window (forced)."
      if ! run_cli gui; then
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
        log "First run: opening the setup window before backgrounding."
        run_cli gui
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
      # A manual run is a control panel: open the graphical window (setup wizard
      # on first run, settings panel afterwards; text wizard if there's no
      # display), then run the watcher in the foreground.
      log "Opening the control panel window."
      if ! run_cli gui || needs_setup; then
        log "Setup not completed."
        pause_before_exit
        exit 1
      fi
      supervise "$@"
      ;;
  esac
}

main "$@"
