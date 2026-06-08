"""Text-mode setup wizard.

A friendly, linear question-and-answer flow for headless machines (and the
fallback when the GUI can't start). The steps mirror the graphical wizard
exactly: find TV -> pair -> choose timeout -> done. Input/output are injectable
so the flow can be driven by tests without a terminal.
"""
from __future__ import annotations

from typing import Callable, List, Optional

from .config import Config, Device
from .discovery import Discovered, discover
from .webos import WebOSClient


def run_text_wizard(
    input_fn: Callable[[str], str] = input,
    output_fn: Callable[[str], None] = lambda m: print(m, flush=True),
    discover_fn: Callable[..., List[Discovered]] = discover,
    client_factory: Callable[[str], WebOSClient] = lambda ip: WebOSClient(ip),
    config: Optional[Config] = None,
) -> int:
    cfg = config or Config.load()
    out = output_fn

    out("=" * 60)
    out("  LGTV Companion Easy Mode - Setup")
    out("=" * 60)
    out("")
    out("This wizard sets your LG TV to sleep after a few minutes of")
    out("inactivity, just like a normal PC monitor. Three quick steps.")
    out("")

    # --- Step 1: choose a TV ------------------------------------------
    out("Step 1 of 3: Find your TV")
    out("Make sure the TV is on and connected to the same network.")
    ip = ""
    name = "My LG TV"
    answer = input_fn("Scan the network automatically? [Y/n]: ").strip().lower()
    if answer in ("", "y", "yes"):
        out("Scanning...")
        found = discover_fn()
        if found:
            for i, dev in enumerate(found, 1):
                out(f"  {i}. {dev.name} @ {dev.ip}")
            choice = input_fn(
                f"Pick a TV [1-{len(found)}], or 'm' to type an IP: "
            ).strip().lower()
            if choice.isdigit() and 1 <= int(choice) <= len(found):
                sel = found[int(choice) - 1]
                ip, name = sel.ip, sel.name
        else:
            out("No TVs found automatically.")
    if not ip:
        ip = input_fn("Enter your TV's IP address (e.g. 192.168.1.50): ").strip()
        typed = input_fn("Give it a name [My LG TV]: ").strip()
        if typed:
            name = typed
    if not ip:
        out("No TV selected. Setup cancelled.")
        return 1

    # --- Step 2: pair --------------------------------------------------
    out("")
    out("Step 2 of 3: Pair with the TV")
    out(f"Connecting to {ip} ...")
    client = client_factory(ip)
    prompt_shown = {"v": False}

    def on_prompt():
        prompt_shown["v"] = True
        out(">> A prompt should appear ON YOUR TV. Press OK / Accept with the remote.")

    try:
        key = client.connect(
            client_key=cfg.device.key if cfg.device.ip == ip else "",
            on_prompt=on_prompt, prompt_timeout=120.0)
    except Exception as exc:  # noqa: BLE001
        out(f"Could not pair: {exc}")
        out("Check the IP address and that the TV is on, then run setup again.")
        return 1
    finally:
        client.close()
    out("Paired successfully.")

    # --- Step 3: timeout ----------------------------------------------
    out("")
    out("Step 3 of 3: Sleep timeout")
    raw = input_fn(
        f"Turn the screen off after how many minutes idle? [{cfg.idle_minutes:g}]: "
    ).strip()
    minutes = cfg.idle_minutes
    if raw:
        try:
            minutes = max(0.5, float(raw))
        except ValueError:
            out(f"Not a number; keeping {cfg.idle_minutes:g} minutes.")

    cfg.device = Device(name=name, ip=ip, mac=cfg.device.mac, key=key)
    cfg.idle_minutes = minutes
    cfg.idle_enabled = True
    cfg.setup_complete = True
    cfg.save()

    out("")
    out("=" * 60)
    out("  All set!")
    out(f"  {name} will sleep after {minutes:g} minutes of inactivity")
    out("  and wake the moment you move the mouse or press a key.")
    out("")
    out("  Start it now with:  lgtv-easy run")
    out("  (the launcher does this automatically in the background)")
    out("=" * 60)
    return 0
