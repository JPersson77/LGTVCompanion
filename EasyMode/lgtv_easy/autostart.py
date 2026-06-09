"""Cross-platform "start when I log in" support.

Registers (or removes) a per-user auto-start entry that launches the idle daemon
quietly at login, so the TV keeps sleeping on idle without the user having to run
anything. No third-party dependencies, no admin rights:

* Windows: a small ``.cmd`` in the user's Startup folder that runs the daemon
  with ``pythonw`` (no console window).
* Linux: a freedesktop ``~/.config/autostart/*.desktop`` entry (honoured by
  GNOME, KDE, XFCE, etc.).

Everything here is best-effort and reports what it did; callers handle errors.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

APP_ID = "lgtv-companion-easy"
FRIENDLY = "LGTV Companion Easy Mode"


def _app_dir() -> str:
    """Directory containing the ``lgtv_easy`` package (so ``-m lgtv_easy`` works)."""
    return str(Path(__file__).resolve().parents[1])


def _pythonw() -> str:
    """Prefer pythonw.exe on Windows so no console window flashes at login."""
    exe = Path(sys.executable) if sys.executable else Path("python")
    candidate = exe.with_name("pythonw.exe")
    return str(candidate if candidate.exists() else exe)


def _windows_target() -> Path:
    base = os.environ.get("APPDATA") or str(Path.home())
    return (Path(base) / "Microsoft" / "Windows" / "Start Menu" / "Programs"
            / "Startup" / "LGTV-Easy-Mode.cmd")


def _linux_target() -> Path:
    base = os.environ.get("XDG_CONFIG_HOME") or str(Path.home() / ".config")
    return Path(base) / "autostart" / f"{APP_ID}.desktop"


def target_path() -> Path:
    return _windows_target() if os.name == "nt" else _linux_target()


def is_enabled() -> bool:
    try:
        return target_path().exists()
    except OSError:
        return False


def enable() -> str:
    """Create the auto-start entry. Returns the path written; raises on failure."""
    path = target_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    appdir = _app_dir()
    if os.name == "nt":
        content = (
            "@echo off\r\n"
            f'cd /d "{appdir}"\r\n'
            f'start "" "{_pythonw()}" -m lgtv_easy run\r\n'
        )
    else:
        py = sys.executable or "python3"
        content = (
            "[Desktop Entry]\n"
            "Type=Application\n"
            f"Name={FRIENDLY}\n"
            f"Exec=sh -c 'cd \"{appdir}\" && \"{py}\" -m lgtv_easy run'\n"
            "Terminal=false\n"
            "X-GNOME-Autostart-enabled=true\n"
        )
    path.write_text(content, encoding="utf-8")
    return str(path)


def disable() -> bool:
    """Remove the auto-start entry. Returns True if one was removed."""
    path = target_path()
    try:
        if path.exists():
            path.unlink()
            return True
    except OSError:
        pass
    return False


def set_enabled(enabled: bool) -> str:
    """Convenience: enable or disable, returning a short human status string."""
    if enabled:
        try:
            return f"auto-start at login ENABLED ({enable()})"
        except OSError as exc:
            return f"could not enable auto-start: {exc}"
    disable()
    return "auto-start at login DISABLED"


def status() -> str:
    return f"{'enabled' if is_enabled() else 'disabled'} ({target_path()})"
