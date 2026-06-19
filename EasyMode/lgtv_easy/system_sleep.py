"""Detect when the whole computer suspends, so the TV can follow it to sleep.

Idle detection blanks the TV after a few minutes without input. But when the
user explicitly puts the PC to sleep - the Start-menu "Sleep" item, closing a
laptop lid, ``systemctl suspend`` - the machine suspends *immediately*, long
before the idle timeout, which would otherwise leave the TV switched on showing a
frozen image or a "no signal" screen. This module catches the OS "about to
suspend" event so the daemon can blank the TV first, exactly the way a desk
monitor goes dark when the PC sleeps, and bring it back when the PC resumes.

Everything here is best-effort and dependency-free:

* Windows: ``PowerRegisterSuspendResumeNotification`` (powrprof.dll) delivers a
  callback on suspend/resume via ctypes - no window or message loop needed.
* Linux (systemd): logind broadcasts ``PrepareForSleep`` on the system bus before
  it suspends. We watch it with ``gdbus`` (already used for GNOME idle detection)
  and hold a short *delay* inhibitor lock via ``systemd-inhibit`` so the TV is
  blanked before the suspend actually lands.
* Anything else: a no-op watcher; the feature simply does nothing.

A watcher calls ``on_sleep()`` just before the machine suspends and ``on_resume()``
just after it wakes. Both run on a background thread, so the callbacks must be
thread-safe (the daemon serialises its TV actions with a lock).
"""
from __future__ import annotations

import shutil
import subprocess
import sys
import threading
from typing import Callable, Optional


class _NullWatcher:
    """A watcher for systems where we can't observe suspend/resume."""

    backend_name = "none"

    def start(self) -> None:
        return None

    def stop(self) -> None:
        return None


# --------------------------------------------------------------------------
# Windows
# --------------------------------------------------------------------------
class _WindowsWatcher:
    """Suspend/resume via PowerRegisterSuspendResumeNotification (no window)."""

    backend_name = "win-powernotify"

    # WM_POWERBROADCAST event types passed to the callback.
    _PBT_APMSUSPEND = 0x0004
    _PBT_APMRESUMESUSPEND = 0x0007
    _PBT_APMRESUMEAUTOMATIC = 0x0012
    _DEVICE_NOTIFY_CALLBACK = 0

    def __init__(self, on_sleep, on_resume, logger):
        self._on_sleep = on_sleep
        self._on_resume = on_resume
        self._logger = logger
        self._handle = None
        self._cb_ref = None   # keep the ctypes callback alive while registered
        self._params = None

    def start(self) -> None:
        import ctypes
        from ctypes import wintypes

        # ULONG CALLBACK Handler(PVOID Context, ULONG Type, PVOID Setting)
        proto = ctypes.WINFUNCTYPE(wintypes.ULONG, ctypes.c_void_p,
                                   wintypes.ULONG, ctypes.c_void_p)

        class _SUBSCRIBE(ctypes.Structure):
            _fields_ = [("Callback", proto), ("Context", ctypes.c_void_p)]

        def _handler(_context, etype, _setting):
            try:
                if etype == self._PBT_APMSUSPEND:
                    self._on_sleep()
                elif etype in (self._PBT_APMRESUMESUSPEND,
                               self._PBT_APMRESUMEAUTOMATIC):
                    if self._on_resume:
                        self._on_resume()
            except Exception:  # noqa: BLE001 - never throw back into the OS
                pass
            return 0

        self._cb_ref = proto(_handler)
        self._params = _SUBSCRIBE(self._cb_ref, None)
        handle = ctypes.c_void_p()
        res = ctypes.windll.powrprof.PowerRegisterSuspendResumeNotification(
            self._DEVICE_NOTIFY_CALLBACK, ctypes.byref(self._params),
            ctypes.byref(handle))
        if res != 0:  # anything but ERROR_SUCCESS
            raise OSError(
                f"PowerRegisterSuspendResumeNotification failed (code {res})")
        self._handle = handle

    def stop(self) -> None:
        if self._handle is not None:
            try:
                import ctypes
                ctypes.windll.powrprof.PowerUnregisterSuspendResumeNotification(
                    self._handle)
            except Exception:  # noqa: BLE001
                pass
            self._handle = None


# --------------------------------------------------------------------------
# Linux (systemd-logind)
# --------------------------------------------------------------------------
class _LogindWatcher:
    """Suspend/resume via logind's PrepareForSleep signal, watched with gdbus."""

    backend_name = "logind"

    def __init__(self, on_sleep, on_resume, logger, gdbus, inhibit):
        self._on_sleep = on_sleep
        self._on_resume = on_resume
        self._logger = logger
        self._gdbus = gdbus
        self._inhibit = inhibit   # path to systemd-inhibit, or None
        self._mon: Optional[subprocess.Popen] = None
        self._lock_proc: Optional[subprocess.Popen] = None
        self._thread: Optional[threading.Thread] = None
        self._stop = threading.Event()

    def start(self) -> None:
        self._acquire_delay_lock()
        try:
            self._mon = subprocess.Popen(
                [self._gdbus, "monitor", "--system",
                 "--dest", "org.freedesktop.login1",
                 "--object-path", "/org/freedesktop/login1"],
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        except Exception:  # noqa: BLE001
            self._release_delay_lock()
            raise
        self._thread = threading.Thread(target=self._run, daemon=True,
                                        name="lgtv-easy-sleep")
        self._thread.start()

    def _acquire_delay_lock(self) -> None:
        # A *delay* inhibitor makes logind wait (up to InhibitDelayMaxSec, ~5s)
        # after emitting PrepareForSleep before it actually suspends - just long
        # enough to blank the TV. Without it the machine can suspend before our
        # screen-off reaches the TV. Best-effort: skip if systemd-inhibit is
        # missing or we already hold a lock.
        if not self._inhibit:
            return
        if self._lock_proc is not None and self._lock_proc.poll() is None:
            return
        try:
            self._lock_proc = subprocess.Popen(
                [self._inhibit, "--what=sleep", "--mode=delay",
                 "--who=LGTV Companion Easy Mode",
                 "--why=Turn the TV screen off before the PC sleeps",
                 "sleep", "infinity"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:  # noqa: BLE001
            self._lock_proc = None

    def _release_delay_lock(self) -> None:
        proc, self._lock_proc = self._lock_proc, None
        if proc is not None and proc.poll() is None:
            try:
                proc.terminate()
            except Exception:  # noqa: BLE001
                pass

    def _run(self) -> None:
        stdout = self._mon.stdout if self._mon else None
        if stdout is None:
            return
        for line in stdout:
            if self._stop.is_set():
                break
            if "PrepareForSleep" not in line:
                continue
            if "true" in line:
                # Going to sleep: blank the TV, then drop the delay lock so the
                # suspend can proceed without waiting out the whole timeout.
                try:
                    self._on_sleep()
                finally:
                    self._release_delay_lock()
            elif "false" in line:
                # Resumed: re-arm the delay lock for next time, then notify.
                self._acquire_delay_lock()
                if self._on_resume:
                    self._on_resume()

    def stop(self) -> None:
        self._stop.set()
        self._release_delay_lock()
        if self._mon is not None and self._mon.poll() is None:
            try:
                self._mon.terminate()
            except Exception:  # noqa: BLE001
                pass
        self._mon = None


def _linux_logind_available(gdbus: str) -> bool:
    """True if logind answers on the system bus, so monitoring is worthwhile."""
    try:
        res = subprocess.run(
            [gdbus, "call", "--system", "--dest", "org.freedesktop.login1",
             "--object-path", "/org/freedesktop/login1",
             "--method", "org.freedesktop.DBus.Peer.Ping"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=3)
        return res.returncode == 0
    except Exception:  # noqa: BLE001
        return False


def make_watcher(on_sleep: Callable[[], None],
                 on_resume: Optional[Callable[[], None]] = None,
                 logger=None):
    """Return a suspend/resume watcher for the current OS (never raises).

    The result always exposes ``start()``/``stop()`` and a ``backend_name``; on
    an unsupported system that's a harmless no-op. ``start()`` may still raise if
    the OS hook fails to register - the caller is expected to guard it.
    """
    try:
        if sys.platform.startswith("win"):
            import ctypes
            ctypes.WinDLL("powrprof")  # probe: present on Windows 8+
            return _WindowsWatcher(on_sleep, on_resume, logger)
        if sys.platform.startswith("linux"):
            gdbus = shutil.which("gdbus")
            if gdbus and _linux_logind_available(gdbus):
                return _LogindWatcher(on_sleep, on_resume, logger, gdbus,
                                      shutil.which("systemd-inhibit"))
    except Exception:  # noqa: BLE001 - fall back to the no-op watcher
        pass
    return _NullWatcher()
