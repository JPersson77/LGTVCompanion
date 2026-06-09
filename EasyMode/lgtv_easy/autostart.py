"""Cross-platform "start when I log in" support.

Registers (or removes) a per-user auto-start entry that launches the idle daemon
quietly at login, so the TV keeps sleeping on idle without the user having to run
anything. No third-party dependencies, no admin rights.

Methods:

* Linux: a freedesktop ``~/.config/autostart/*.desktop`` entry.
* Windows "startup" (default): a small ``.cmd`` in the user's Startup folder that
  runs the daemon with ``pythonw`` (no console window).
* Windows "task": a per-user Scheduled Task that runs at logon. Useful when the
  Startup folder is restricted by group policy.

Everything here is best-effort and reports what it did; callers handle errors.
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

APP_ID = "lgtv-companion-easy"
FRIENDLY = "LGTV Companion Easy Mode"
TASK_NAME = "LGTV Companion Easy Mode"


def _app_dir() -> str:
    """Directory containing the ``lgtv_easy`` package (so ``-m lgtv_easy`` works)."""
    return str(Path(__file__).resolve().parents[1])


def _pythonw() -> str:
    """Prefer pythonw.exe on Windows so no console window flashes at login."""
    exe = Path(sys.executable) if sys.executable else Path("python")
    candidate = exe.with_name("pythonw.exe")
    return str(candidate if candidate.exists() else exe)


# ----- Windows: Startup folder ------------------------------------------------
def _startup_target() -> Path:
    base = os.environ.get("APPDATA") or str(Path.home())
    return (Path(base) / "Microsoft" / "Windows" / "Start Menu" / "Programs"
            / "Startup" / "LGTV-Easy-Mode.cmd")


def _windows_run_cmd_content() -> str:
    return (
        "@echo off\r\n"
        f'cd /d "{_app_dir()}"\r\n'
        f'start "" "{_pythonw()}" -m lgtv_easy run\r\n'
    )


# ----- Windows: Scheduled Task ------------------------------------------------
def _task_wrapper_path() -> Path:
    """A stable .cmd the scheduled task points at (kept beside the config)."""
    from .config import config_dir
    return Path(config_dir()) / "autostart-run.cmd"


def _run(args) -> "tuple[int, str]":
    try:
        proc = subprocess.run(args, capture_output=True, text=True, timeout=20)
        return proc.returncode, (proc.stdout or "") + (proc.stderr or "")
    except Exception as exc:  # noqa: BLE001 - tool missing, timeout, etc.
        return 1, str(exc)


def _task_create_args(wrapper: Path) -> list:
    # The /TR value is the command to run; quote the path for spaces.
    return ["schtasks", "/Create", "/TN", TASK_NAME,
            "/TR", f'"{wrapper}"', "/SC", "ONLOGON", "/F"]


def _task_exists() -> bool:
    if os.name != "nt":
        return False
    rc, _ = _run(["schtasks", "/Query", "/TN", TASK_NAME])
    return rc == 0


def _enable_task() -> str:
    wrapper = _task_wrapper_path()
    wrapper.parent.mkdir(parents=True, exist_ok=True)
    wrapper.write_text(_windows_run_cmd_content(), encoding="utf-8")
    rc, out = _run(_task_create_args(wrapper))
    if rc != 0:
        raise OSError(f"schtasks could not create the task: {out.strip()}")
    return f"Scheduled Task '{TASK_NAME}'"


def _disable_task() -> bool:
    removed = False
    if _task_exists():
        rc, _ = _run(["schtasks", "/Delete", "/TN", TASK_NAME, "/F"])
        removed = rc == 0
    try:
        _task_wrapper_path().unlink()
    except OSError:
        pass
    return removed


# ----- Linux: autostart .desktop ----------------------------------------------
def _linux_target() -> Path:
    base = os.environ.get("XDG_CONFIG_HOME") or str(Path.home() / ".config")
    return Path(base) / "autostart" / f"{APP_ID}.desktop"


def _linux_desktop_content() -> str:
    py = sys.executable or "python3"
    return (
        "[Desktop Entry]\n"
        "Type=Application\n"
        f"Name={FRIENDLY}\n"
        f"Exec=sh -c 'cd \"{_app_dir()}\" && \"{py}\" -m lgtv_easy run'\n"
        "Terminal=false\n"
        "X-GNOME-Autostart-enabled=true\n"
    )


# ----- public API -------------------------------------------------------------
def default_method() -> str:
    return "startup" if os.name == "nt" else "desktop"


def is_enabled() -> bool:
    """True if auto-start is active by *any* supported method."""
    try:
        if os.name == "nt":
            return _startup_target().exists() or _task_exists()
        return _linux_target().exists()
    except OSError:
        return False


def enable(method: str = "") -> str:
    """Create the auto-start entry. Returns a short label; raises on failure."""
    method = method or default_method()
    if os.name == "nt":
        if method == "task":
            return _enable_task()
        path = _startup_target()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(_windows_run_cmd_content(), encoding="utf-8")
        return f"Startup folder ({path})"
    path = _linux_target()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(_linux_desktop_content(), encoding="utf-8")
    return f"autostart entry ({path})"


def disable() -> bool:
    """Remove the auto-start entry/entries. Returns True if anything was removed."""
    removed = False
    if os.name == "nt":
        try:
            if _startup_target().exists():
                _startup_target().unlink()
                removed = True
        except OSError:
            pass
        if _disable_task():
            removed = True
        return removed
    try:
        if _linux_target().exists():
            _linux_target().unlink()
            removed = True
    except OSError:
        pass
    return removed


def set_enabled(enabled: bool, method: str = "") -> str:
    """Convenience: enable or disable, returning a short human status string."""
    if enabled:
        try:
            return f"auto-start at login ENABLED via {enable(method)}"
        except OSError as exc:
            return f"could not enable auto-start: {exc}"
    disable()
    return "auto-start at login DISABLED"


def status() -> str:
    if not is_enabled():
        return "disabled"
    if os.name == "nt":
        how = []
        if _startup_target().exists():
            how.append("Startup folder")
        if _task_exists():
            how.append("Scheduled Task")
        return "enabled (" + ", ".join(how) + ")"
    return f"enabled ({_linux_target()})"
