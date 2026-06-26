"""Connect to the saved TV, healing a stale IP on the way.

This is the on-demand counterpart to the daemon's automatic relocation: the GUI
"Test" button and the one-shot CLI commands (`on`, `off`, `test`) all go through
here, so wherever the user touches the app it transparently copes with the TV
having taken a new DHCP address. The TV's MAC never changes, so when the saved IP
stops answering we look the TV up again, rewrite the stored IP, and retry once.
"""
from __future__ import annotations

from typing import Callable, Optional

from .config import Config
from .webos import WebOSClient, pair_with_fallback


def connect_tv(cfg: Config, *, on_prompt: Optional[Callable[[], None]] = None,
               prompt_timeout: float = 60.0, timeout: float = 10.0,
               recover: bool = True, discover_timeout: float = 3.0,
               persist: bool = True,
               log: Optional[Callable[[str], None]] = None) -> WebOSClient:
    """Return a live, paired client for ``cfg.device`` - relocating it if moved.

    Tries the saved IP first. If that fails and ``recover`` is set, it locates
    the TV again (by MAC, else by discovery), updates and persists
    ``cfg.device.ip``, and retries once. Raises the original error if the TV
    still can't be reached. The caller owns the returned client and must close
    it. ``recover=False`` keeps it fast for time-critical paths (e.g. powering
    the TV off as the PC shuts down).
    """
    out = log or (lambda _m: None)

    def _open() -> WebOSClient:
        client = WebOSClient(cfg.device.ip, secure=cfg.device.secure,
                             timeout=timeout)
        pair_with_fallback(client, client_key=cfg.device.key,
                           on_prompt=on_prompt, prompt_timeout=prompt_timeout,
                           prefer_secure=cfg.device.secure, log=out)
        return client

    try:
        return _open()
    except Exception as exc:  # noqa: BLE001 - network errors are expected
        if not recover:
            raise
        from .discovery import locate_tv
        out("Saved TV address didn't answer; looking for the TV again...")
        new_ip = locate_tv(cfg.device.mac, timeout=discover_timeout, log=out)
        if not new_ip:
            raise
        host = new_ip.rpartition(":")[0] if ":" in new_ip else new_ip
        if host == cfg.device.ip:
            raise  # found at the same address; the failure was something else
        out(f"TV moved to {host}; updating the saved address.")
        cfg.device.ip = host
        if persist:
            try:
                cfg.save()
            except Exception:  # noqa: BLE001 - persistence is best-effort
                pass
        return _open()
