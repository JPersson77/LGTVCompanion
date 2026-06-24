"""Cross-platform "seconds since last user input" detection.

* Windows: ``GetLastInputInfo`` via ctypes (no dependencies).
* Linux/Wayland (GNOME): ``org.gnome.Mutter.IdleMonitor`` over D-Bus (via
  ``gdbus``), since the X11 tools can't see input on a Wayland session.
* Linux/X11: ``xprintidle`` if present, else libXScreenSaver via ctypes.
* Anything else / headless: a manual source that can be driven by tests or the
  environment variable ``LGTV_EASY_FAKE_IDLE`` (seconds).

``get_idle_seconds()`` always returns a float; it never raises, so the daemon
can rely on it. ``idle_backend_name()`` reports which method is active, which the
wizard surfaces so the user knows idle detection actually works on their system.
"""
from __future__ import annotations

import ctypes
import os
import re
import shutil
import subprocess
import sys
import time

from . import _dbus

_BACKEND = None  # cached ("name", callable)
_GDBUS_PATH = None  # cached gdbus resolution (False once known-absent)


def _windows_backend():
    class LASTINPUTINFO(ctypes.Structure):
        _fields_ = [("cbSize", ctypes.c_uint), ("dwTime", ctypes.c_ulong)]

    user32 = ctypes.windll.user32  # type: ignore[attr-defined]
    kernel32 = ctypes.windll.kernel32  # type: ignore[attr-defined]

    def _get() -> float:
        info = LASTINPUTINFO()
        info.cbSize = ctypes.sizeof(LASTINPUTINFO)
        if not user32.GetLastInputInfo(ctypes.byref(info)):
            return 0.0
        millis = kernel32.GetTickCount() - info.dwTime
        return max(0.0, millis / 1000.0)

    return ("windows", _get)


def _xprintidle_backend():
    if not os.environ.get("DISPLAY"):
        return None  # xprintidle needs an X display; don't pick a no-op backend
    path = shutil.which("xprintidle")
    if not path:
        return None

    def _get() -> float:
        try:
            out = subprocess.check_output([path], timeout=2)
            return max(0.0, int(out.strip()) / 1000.0)
        except Exception:
            return 0.0

    return ("xprintidle", _get)


def _xss_backend():
    if not os.environ.get("DISPLAY"):
        return None
    try:
        xss = ctypes.cdll.LoadLibrary("libXss.so.1")
        x11 = ctypes.cdll.LoadLibrary("libX11.so.6")
    except OSError:
        return None

    class XScreenSaverInfo(ctypes.Structure):
        _fields_ = [
            ("window", ctypes.c_ulong), ("state", ctypes.c_int),
            ("kind", ctypes.c_int), ("til_or_since", ctypes.c_ulong),
            ("idle", ctypes.c_ulong), ("event_mask", ctypes.c_ulong),
        ]

    x11.XOpenDisplay.restype = ctypes.c_void_p
    xss.XScreenSaverAllocInfo.restype = ctypes.POINTER(XScreenSaverInfo)
    dpy = x11.XOpenDisplay(None)
    if not dpy:
        return None
    root = x11.XDefaultRootWindow(dpy)
    info = xss.XScreenSaverAllocInfo()

    def _get() -> float:
        xss.XScreenSaverQueryInfo(dpy, root, info)
        return max(0.0, info.contents.idle / 1000.0)

    return ("libXss", _get)


def _gdbus_path() -> "str | None":
    """Resolve the gdbus binary once (the daemon polls forever - don't re-scan
    PATH on every call)."""
    global _GDBUS_PATH
    if _GDBUS_PATH is None:
        _GDBUS_PATH = shutil.which("gdbus") or False
    return _GDBUS_PATH or None


def _gdbus_call(dest: str, path: str, method: str) -> "str | None":
    """Call a session-bus D-Bus method via the gdbus CLI; return stdout or None."""
    gdbus = _gdbus_path()
    if not gdbus:
        return None
    try:
        return subprocess.check_output(
            [gdbus, "call", "--session", "--dest", dest,
             "--object-path", path, "--method", method],
            stderr=subprocess.DEVNULL, timeout=2, text=True).strip()
    except Exception:
        return None


def _session_uint(dest: str, path: str, interface: str, member: str) -> "int | None":
    """Read a single unsigned int from a no-arg session-bus method.

    Tries the in-process D-Bus client first (no subprocess), and only falls back
    to spawning ``gdbus`` if that can't answer. Returns None if neither can.
    """
    value = _dbus.session_get_uint(dest, path, interface, member)
    if value is not None:
        return value
    return _parse_uint(_gdbus_call(dest, path, f"{interface}.{member}"))


def _parse_uint(text: "str | None") -> "int | None":
    """Pull the value out of a gdbus reply like '(uint64 12345,)'.

    Note the type token itself contains digits (uint64), so match the integer
    that *follows* the type; fall back to a bare integer for plain replies.
    """
    if not text:
        return None
    m = re.search(r"u?int\d+\s+(\d+)", text)
    if m:
        return int(m.group(1))
    m = re.search(r"\d+", text)
    return int(m.group(0)) if m else None


def _mutter_idle_backend():
    """GNOME's IdleMonitor - works on both Wayland and X11. Returns ms idle."""
    dest = "org.gnome.Mutter.IdleMonitor"
    path = "/org/gnome/Mutter/IdleMonitor/Core"
    interface = "org.gnome.Mutter.IdleMonitor"
    member = "GetIdletime"
    if _session_uint(dest, path, interface, member) is None:
        return None  # not GNOME, or no session bus - don't pick it

    def _get() -> float:
        ms = _session_uint(dest, path, interface, member)
        # On a transient failure report "active" (0) rather than risk a huge
        # value that would sleep the TV spuriously.
        return max(0.0, (ms or 0) / 1000.0)

    return ("gnome-idlemonitor", _get)


def _freedesktop_idle_backend():
    """KDE Plasma (and other non-GNOME freedesktop compositors) expose idle time
    via ``org.freedesktop.ScreenSaver.GetSessionIdleTime`` (milliseconds).

    This fills the gap on KDE Wayland, where GNOME's Mutter monitor isn't
    present and the X11 tools can't see Wayland input - without it those sessions
    fall through to the manual no-op and the TV never sleeps. GNOME does *not*
    implement this method (it returns an error), so this backend only activates
    where the call actually answers, leaving GNOME on its Mutter backend above.
    """
    dest = "org.freedesktop.ScreenSaver"
    interface = "org.freedesktop.ScreenSaver"
    member = "GetSessionIdleTime"
    # KDE registers the service at both object paths; probe whichever answers.
    for path in ("/org/freedesktop/ScreenSaver", "/ScreenSaver"):
        if _session_uint(dest, path, interface, member) is None:
            continue

        def _get(p=path) -> float:
            ms = _session_uint(dest, p, interface, member)
            return max(0.0, (ms or 0) / 1000.0)

        return ("freedesktop-screensaver", _get)
    return None


class ManualIdle:
    """A controllable idle source for tests and headless fallback."""

    def __init__(self, start: float = 0.0):
        self._last_active = time.monotonic() - start

    def get(self) -> float:
        env = os.environ.get("LGTV_EASY_FAKE_IDLE")
        if env is not None:
            try:
                return float(env)
            except ValueError:
                pass
        return max(0.0, time.monotonic() - self._last_active)

    def mark_active(self) -> None:
        self._last_active = time.monotonic()

    def set_idle(self, seconds: float) -> None:
        self._last_active = time.monotonic() - seconds


_manual = ManualIdle()


def _select_backend():
    if sys.platform.startswith("win"):
        try:
            return _windows_backend()
        except Exception:
            pass
    if sys.platform.startswith("linux"):
        session = (os.environ.get("XDG_SESSION_TYPE") or "").lower()
        wayland = session == "wayland" or bool(os.environ.get("WAYLAND_DISPLAY"))
        if wayland:
            # On Wayland the X11 tools (xprintidle/XScreenSaver) only see XWayland
            # input - they report bogus idle - so use the compositor's own monitor:
            # GNOME's Mutter, else the freedesktop/KDE ScreenSaver interface. Only
            # if neither answers do we fall back to manual (honest) instead of lying.
            factories = [_mutter_idle_backend, _freedesktop_idle_backend]
        else:
            factories = [_xprintidle_backend, _xss_backend,
                         _mutter_idle_backend, _freedesktop_idle_backend]
        for factory in factories:
            backend = factory()
            if backend:
                return backend
    return ("manual", _manual.get)


def _backend():
    global _BACKEND
    if _BACKEND is None:
        _BACKEND = _select_backend()
    return _BACKEND


def get_idle_seconds() -> float:
    # A global override so headless setups, the launcher's first run, and tests
    # can force a known idle value regardless of the active backend.
    env = os.environ.get("LGTV_EASY_FAKE_IDLE")
    if env is not None:
        try:
            return float(env)
        except ValueError:
            pass
    try:
        return _backend()[1]()
    except Exception:
        return 0.0


def idle_backend_name() -> str:
    return _backend()[0]


def is_real_backend() -> bool:
    """True when we can actually observe OS input (not the manual fallback)."""
    return _backend()[0] != "manual"


# Exposed so tests can drive idle deterministically.
manual_source = _manual
