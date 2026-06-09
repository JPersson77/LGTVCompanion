"""Configuration storage for Easy Mode.

The config is a single small JSON file kept in the per-user config directory so
it survives app updates. It intentionally mirrors the handful of settings a
monitor user actually cares about, instead of the dozens the original UI exposes.
"""
from __future__ import annotations

import json
import os
import tempfile
from dataclasses import asdict, dataclass, field
from typing import Optional


def config_dir() -> str:
    """Return the per-user directory where Easy Mode keeps its files."""
    override = os.environ.get("LGTV_EASY_HOME")
    if override:
        return override
    if os.name == "nt":
        base = os.environ.get("APPDATA") or os.path.expanduser("~")
        return os.path.join(base, "LGTV Companion Easy Mode")
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.join(
        os.path.expanduser("~"), ".config"
    )
    return os.path.join(base, "lgtv-companion-easy")


def config_path() -> str:
    return os.path.join(config_dir(), "config.json")


def log_path() -> str:
    return os.path.join(config_dir(), "easy-mode.log")


@dataclass
class Device:
    """A single TV. ``key`` is the WebOS pairing client-key once paired."""

    name: str = "My LG TV"
    ip: str = ""
    mac: str = ""
    key: str = ""

    @property
    def paired(self) -> bool:
        return bool(self.key)


@dataclass
class Config:
    # The whole point of the app: blank the screen after this many minutes idle.
    idle_minutes: float = 7.0
    # Master switch. When false the daemon stays running but does nothing.
    idle_enabled: bool = True
    # Re-check the system idle timer this often (seconds).
    poll_seconds: float = 5.0
    # Mute the TV speakers when the screen sleeps (handy for some setups).
    mute_on_sleep: bool = False
    # Energy saving: after a longer idle, fully power the TV OFF (true standby,
    # ~0.5W) instead of just blanking the screen. Waking from this needs
    # Wake-on-LAN (the TV's "Turn on via Wi-Fi"/"Quick Start+" setting), so the
    # TV's MAC address is stored on the device for the magic packet.
    deep_off_enabled: bool = False
    deep_off_minutes: float = 30.0
    # True once the setup wizard has completed successfully.
    setup_complete: bool = False
    device: Device = field(default_factory=Device)

    # ----- persistence -------------------------------------------------
    @classmethod
    def load(cls, path: Optional[str] = None) -> "Config":
        path = path or config_path()
        try:
            with open(path, "r", encoding="utf-8") as fh:
                data = json.load(fh)
        except (FileNotFoundError, ValueError):
            return cls()
        dev = Device(**{k: v for k, v in data.pop("device", {}).items()
                        if k in Device.__dataclass_fields__})
        known = {k: v for k, v in data.items() if k in cls.__dataclass_fields__}
        cfg = cls(**known)
        cfg.device = dev
        return cfg

    def save(self, path: Optional[str] = None) -> str:
        path = path or config_path()
        os.makedirs(os.path.dirname(path), exist_ok=True)
        payload = asdict(self)
        # Atomic write so a crash mid-save never corrupts the config.
        fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path), suffix=".tmp")
        try:
            with os.fdopen(fd, "w", encoding="utf-8") as fh:
                json.dump(payload, fh, indent=2)
            os.replace(tmp, path)
        finally:
            if os.path.exists(tmp):
                os.remove(tmp)
        return path

    @property
    def idle_seconds(self) -> float:
        return max(1.0, float(self.idle_minutes) * 60.0)

    @property
    def deep_off_seconds(self) -> float:
        return max(1.0, float(self.deep_off_minutes) * 60.0)
