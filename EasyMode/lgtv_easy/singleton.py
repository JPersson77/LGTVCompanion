"""A tiny cross-platform single-instance guard for the idle daemon.

Now that the daemon can be launched several ways - by the auto-start entry at
login, by the supervising launcher, or by hand - we must ensure only one copy is
actually driving the TV at a time. A pidfile in the config directory does that:
each daemon records its PID and checks whether a live one already holds it.

Two acquisition modes:
* ``wait=False`` (default, e.g. a manual ``lgtv-easy run``): if another live
  daemon holds the lock, give up immediately so the command can exit politely.
* ``wait=True`` (used by the supervisor, via LGTV_EASY_WAIT_LOCK=1): block until
  the lock is free, so a supervised child quietly stands by instead of spinning.
"""
from __future__ import annotations

import os
import time
from typing import Optional

from .config import config_dir


def _alive(pid: int) -> bool:
    if pid <= 0:
        return False
    try:
        if os.name == "nt":
            import ctypes

            kernel32 = ctypes.windll.kernel32  # type: ignore[attr-defined]
            PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
            handle = kernel32.OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
            if handle:
                kernel32.CloseHandle(handle)
                return True
            return False
        os.kill(pid, 0)
        return True
    except (OSError, ValueError):
        return False


class SingleInstance:
    def __init__(self, name: str = "daemon"):
        self.path = os.path.join(config_dir(), f"{name}.pid")
        self.acquired = False

    def _holder(self) -> Optional[int]:
        try:
            with open(self.path, "r", encoding="utf-8") as fh:
                pid = int(fh.read().strip())
        except (OSError, ValueError):
            return None
        return pid if _alive(pid) else None

    def holder(self) -> Optional[int]:
        """PID of the live process currently holding the lock, or None."""
        return self._holder()

    def _write(self) -> None:
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        with open(self.path, "w", encoding="utf-8") as fh:
            fh.write(str(os.getpid()))

    def acquire(self, wait: bool = False, poll: float = 5.0,
                sleep_fn=time.sleep) -> bool:
        """Take the lock. Returns True if held, False if busy and not waiting."""
        while True:
            holder = self._holder()
            if holder is None or holder == os.getpid():
                self._write()
                self.acquired = True
                return True
            if not wait:
                return False
            sleep_fn(poll)

    def release(self) -> None:
        if self.acquired:
            try:
                # Only remove if it's still ours.
                if self._holder() == os.getpid():
                    os.remove(self.path)
            except OSError:
                pass
            self.acquired = False

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.release()
