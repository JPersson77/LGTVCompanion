"""Command-line interface for Easy Mode.

Everything the GUI can do is also available here, which makes the app scriptable,
testable, and usable on headless machines. Subcommands:

    scan      discover LG TVs on the network
    pair      pair with a TV (by IP) and save it
    set       change settings, e.g. the idle timeout in minutes
    status    show current configuration and idle backend
    test      verify the saved TV by blinking the screen off then on
    run       run the idle-monitoring daemon in the foreground
    wizard    run the interactive text setup wizard
"""
from __future__ import annotations

import argparse
import sys
import time

from . import __version__
from .config import Config, Device, config_path, log_path
from .daemon import Daemon
from . import idle as idle_mod
from .webos import WebOSClient


def _print(msg: str = "") -> None:
    print(msg, flush=True)


def cmd_scan(args) -> int:
    from .discovery import discover
    _print("Scanning the network for LG TVs (a few seconds)...")
    found = discover(timeout=args.timeout, log=_print)
    if not found:
        _print("No TVs found. You can still add one by IP with: lgtv-easy pair <ip>")
        return 1
    for i, dev in enumerate(found, 1):
        _print(f"  {i}. {dev.name}  @ {dev.ip}")
    return 0


def cmd_pair(args) -> int:
    cfg = Config.load()
    client = WebOSClient(args.ip, secure=args.secure)
    _print(f"Connecting to {args.ip} ...")

    def on_prompt():
        _print(">> Look at your TV and press OK / Accept on the pairing prompt.")

    from .webos import pair_with_fallback
    try:
        key = pair_with_fallback(
            client,
            client_key=cfg.device.key if cfg.device.ip == args.ip else "",
            on_prompt=on_prompt, prompt_timeout=args.timeout, log=_print)
    except Exception as exc:  # noqa: BLE001
        _print(f"Pairing failed: {exc}")
        from .netdiag import probe_tv
        _print("--- Connection diagnostics ---")
        probe_tv(args.ip, _print)
        return 1
    finally:
        client.close()
    mac = args.mac or cfg.device.mac
    if not mac:
        from .netdiag import mac_for_ip
        mac = mac_for_ip(args.ip)
        if mac:
            _print(f"Detected TV hardware address {mac} for Wake-on-LAN.")
    cfg.device = Device(name=args.name or cfg.device.name or "My LG TV",
                        ip=args.ip, mac=mac, key=key, secure=client.secure)
    cfg.setup_complete = True
    cfg.save()
    _print(f"Paired! Saved TV '{cfg.device.name}' at {args.ip}.")
    return 0


def cmd_set(args) -> int:
    cfg = Config.load()
    changed = []
    if args.minutes is not None:
        cfg.idle_minutes = args.minutes
        changed.append(f"timeout={args.minutes} min")
    if args.enabled is not None:
        cfg.idle_enabled = args.enabled
        changed.append(f"enabled={args.enabled}")
    if args.mute is not None:
        cfg.mute_on_sleep = args.mute
        changed.append(f"mute_on_sleep={args.mute}")
    if args.deep_off is not None:
        cfg.deep_off_enabled = args.deep_off
        changed.append(f"deep_off={args.deep_off}")
    if args.deep_off_minutes is not None:
        cfg.deep_off_minutes = args.deep_off_minutes
        changed.append(f"deep_off_minutes={args.deep_off_minutes}")
    if args.mac is not None:
        cfg.device.mac = args.mac
        changed.append(f"mac={args.mac}")
    cfg.save()
    _print("Updated: " + (", ".join(changed) if changed else "(nothing)"))
    return 0


def cmd_status(args) -> int:
    cfg = Config.load()
    _print(f"LGTV Companion Easy Mode {__version__}")
    _print(f"  Config file : {config_path()}")
    _print(f"  Log file    : {log_path()}")
    _print(f"  Setup done  : {cfg.setup_complete}")
    _print(f"  TV          : {cfg.device.name} @ {cfg.device.ip or '(none)'}"
           f"  paired={cfg.device.paired}  "
           f"port={'3001/wss' if cfg.device.secure else '3000/ws'}")
    _print(f"  Idle sleep  : {'ON' if cfg.idle_enabled else 'OFF'} after "
           f"{cfg.idle_minutes} min  (mute={cfg.mute_on_sleep})")
    if cfg.deep_off_enabled:
        _print(f"  Deep off    : ON after {cfg.deep_off_minutes} min "
               f"(full power-off, WOL mac={cfg.device.mac or '(none!)'})")
    else:
        _print("  Deep off    : OFF (screen blanks only; TV stays powered)")
    _print(f"  Idle backend: {idle_mod.idle_backend_name()} "
           f"(real={idle_mod.is_real_backend()})")
    _print(f"  Current idle: {idle_mod.get_idle_seconds():.0f}s")
    from . import autostart
    _print(f"  Auto-start  : {autostart.status()}")
    return 0


def cmd_test(args) -> int:
    cfg = Config.load()
    if not cfg.device.ip:
        _print("No TV configured. Run 'lgtv-easy pair <ip>' first.")
        return 1
    from .webos import (URI_GET_NETWORK_STATUS, URI_GET_SW_INFO,
                        pair_with_fallback)
    client = WebOSClient(cfg.device.ip, secure=cfg.device.secure)
    try:
        pair_with_fallback(client, client_key=cfg.device.key,
                           prefer_secure=cfg.device.secure, log=_print)
        _print("Turning screen OFF for 3 seconds...")
        client.screen_off()
        time.sleep(3)
        _print("Turning screen ON...")
        client.screen_on()
        # While connected, learn (and save) the TV's MAC for Wake-on-LAN.
        mac = client.get_mac()
        if mac:
            if mac != cfg.device.mac:
                cfg.device.mac = mac
                cfg.save()
            _print(f"TV MAC for Wake-on-LAN: {mac}  (saved)")
        else:
            _print("Could not read the TV's MAC. Raw network info follows so it")
            _print("can be reported:")
            _print(f"  getStatus: {client.request(URI_GET_NETWORK_STATUS)}")
            _print(f"  swInfo   : {client.request(URI_GET_SW_INFO)}")
    except Exception as exc:  # noqa: BLE001
        _print(f"Test failed: {exc}")
        return 1
    finally:
        client.close()
    _print("Test OK - your TV responds to Easy Mode.")
    return 0


def cmd_run(args) -> int:
    cfg = Config.load()
    if not cfg.device.ip:
        _print("No TV configured yet. Run the wizard or 'lgtv-easy pair <ip>'.")
        return 1
    # Only one daemon should drive the TV. A supervised child (LGTV_EASY_WAIT_LOCK)
    # waits its turn; a manual run exits politely if one is already going.
    import os
    from .singleton import SingleInstance
    lock = SingleInstance("daemon")
    wait = os.environ.get("LGTV_EASY_WAIT_LOCK") == "1"
    if not lock.acquire(wait=wait):
        _print("Another Easy Mode watcher is already running; nothing to do.")
        return 0
    daemon = Daemon(cfg)
    _print(f"Idle daemon running. Screen sleeps after {cfg.idle_minutes} min. "
           "Press Ctrl+C to stop.")
    try:
        daemon.run()  # blocks
    except KeyboardInterrupt:
        daemon.stop()
        _print("\nStopped.")
    finally:
        lock.release()
    return 0


def cmd_autostart(args) -> int:
    from . import autostart
    action = getattr(args, "action", None) or "status"
    if action == "enable":
        try:
            path = autostart.enable(method=getattr(args, "method", "") or "")
        except Exception as exc:  # noqa: BLE001
            _print(f"Could not enable auto-start: {exc}")
            return 1
        _print(f"Auto-start at login ENABLED via {path}")
    elif action == "disable":
        autostart.disable()
        _print("Auto-start at login DISABLED.")
    else:
        _print(f"Auto-start at login: {autostart.status()}")
    return 0


def cmd_wizard(args) -> int:
    from .wizard_text import run_text_wizard
    rc = run_text_wizard()
    if rc != 0:
        from .netdiag import env_summary
        _print("")
        _print("=" * 60)
        _print("  Setup did not finish. If you need help, copy everything")
        _print("  above plus these details into a bug report:")
        _print("=" * 60)
        for line in env_summary():
            _print("  " + line)
        _print(f"  Log file    : {log_path()}")
        _print("=" * 60)
    return rc


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="lgtv-easy",
        description="LGTV Companion Easy Mode - sleep your LG TV when idle.")
    p.add_argument("--version", action="version",
                   version=f"%(prog)s {__version__}")
    sub = p.add_subparsers(dest="command")

    s = sub.add_parser("scan", help="find LG TVs on the network")
    s.add_argument("--timeout", type=float, default=3.0)
    s.set_defaults(func=cmd_scan)

    s = sub.add_parser("pair", help="pair with a TV by IP and save it")
    s.add_argument("ip")
    s.add_argument("--name", default="")
    s.add_argument("--mac", default="")
    s.add_argument("--secure", action="store_true")
    s.add_argument("--timeout", type=float, default=60.0)
    s.set_defaults(func=cmd_pair)

    s = sub.add_parser("set", help="change settings")
    s.add_argument("--minutes", type=float, help="screen-off timeout in minutes")
    s.add_argument("--enabled", type=_boolish, help="true/false")
    s.add_argument("--mute", type=_boolish, help="mute speakers on sleep")
    s.add_argument("--deep-off", dest="deep_off", type=_boolish,
                   help="fully power the TV off after a longer idle (true/false)")
    s.add_argument("--deep-off-minutes", dest="deep_off_minutes", type=float,
                   help="total idle minutes before fully powering off")
    s.add_argument("--mac", help="set Wake-on-LAN MAC address")
    s.set_defaults(func=cmd_set)

    s = sub.add_parser("status", help="show current settings")
    s.set_defaults(func=cmd_status)

    s = sub.add_parser("test", help="blink the screen to verify the TV")
    s.set_defaults(func=cmd_test)

    s = sub.add_parser("run", help="run the idle-monitoring daemon")
    s.set_defaults(func=cmd_run)

    s = sub.add_parser("wizard", help="interactive text setup wizard")
    s.set_defaults(func=cmd_wizard)

    s = sub.add_parser("autostart", help="start automatically at login")
    s.add_argument("action", nargs="?", choices=["enable", "disable", "status"],
                   default="status")
    s.add_argument("--method", choices=["startup", "task", "desktop"], default="",
                   help="Windows: 'startup' folder (default) or 'task' "
                        "(Task Scheduler, for locked-down Startup folders)")
    s.set_defaults(func=cmd_autostart)
    return p


def _boolish(value: str) -> bool:
    return str(value).strip().lower() in ("1", "true", "yes", "on", "y")


def main(argv=None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if not getattr(args, "command", None):
        # No subcommand: try to launch the GUI, fall back to text wizard.
        try:
            from .gui import main as gui_main
            return gui_main()
        except Exception:  # noqa: BLE001 - no display / no tk
            _print("(No GUI available - starting text wizard.)\n")
            from .wizard_text import run_text_wizard
            return run_text_wizard()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
