"""Text-mode setup wizard.

A friendly, linear question-and-answer flow for headless machines (and the
fallback when the GUI can't start). The steps mirror the graphical wizard
exactly: find TV -> pair -> choose timeout -> done. Input/output are injectable
so the flow can be driven by tests without a terminal.

When something goes wrong (no TV found, or a manually entered IP won't connect)
the wizard prints plain-language diagnostics - which network the PC is on,
whether the TV's ports answer - and offers to try again, rather than failing
silently. The launchers keep the window open afterwards so the user can read and
report what it said.
"""
from __future__ import annotations

from typing import Callable, List, Optional, Tuple

from .config import Config, Device
from .discovery import Discovered, discover
from .netdiag import env_summary, probe_tv
from .webos import WebOSClient, pair_with_fallback


def _choose_tv(input_fn, out, discover_fn) -> Tuple[str, str]:
    """Step 1: let the user pick a discovered TV or type an IP. Returns (ip, name)."""
    out("Step 1 of 3: Find your TV")
    out("Make sure the TV is on and connected to the same network.")
    name = "My LG TV"
    answer = input_fn("Scan the network automatically? [Y/n]: ").strip().lower()
    if answer in ("", "y", "yes"):
        out("Scanning the network (this takes a few seconds)...")
        found = discover_fn(log=lambda m: out("  " + m))
        if found:
            for i, dev in enumerate(found, 1):
                out(f"  {i}. {dev.name} @ {dev.ip}")
            choice = input_fn(
                f"Pick a TV [1-{len(found)}], or 'm' to type an IP: "
            ).strip().lower()
            if choice.isdigit() and 1 <= int(choice) <= len(found):
                sel = found[int(choice) - 1]
                return sel.ip, sel.name
        else:
            out("No TVs found automatically - you can enter the IP by hand below.")
    ip = input_fn("Enter your TV's IP address (e.g. 192.168.1.50): ").strip()
    typed = input_fn("Give it a name [My LG TV]: ").strip()
    if typed:
        name = typed
    return ip, name


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
    out("System info (handy if you need to report a problem):")
    for line in env_summary():
        out("  " + line)
    out("")

    key = ""
    ip = ""
    name = "My LG TV"

    # Step 1 + 2 together, in a retry loop: a failed pairing shouldn't dump the
    # user out of setup - it should explain what went wrong and let them retry.
    while True:
        ip, name = _choose_tv(input_fn, out, discover_fn)
        if not ip:
            out("No TV selected. Setup cancelled.")
            return 1

        out("")
        out("Step 2 of 3: Pair with the TV")
        out(f"Connecting to {ip} ...")
        client = client_factory(ip)

        def on_prompt():
            out(">> A prompt should appear ON YOUR TV. Press OK / Accept with the remote.")

        try:
            key = pair_with_fallback(
                client,
                client_key=cfg.device.key if cfg.device.ip == ip else "",
                on_prompt=on_prompt, prompt_timeout=120.0,
                log=lambda m: out("  " + m))
        except Exception as exc:  # noqa: BLE001
            out("")
            out(f"Could not pair with the TV at {ip}: {exc}")
            out("--- Connection diagnostics (please copy these if asking for help) ---")
            probe_tv(ip, lambda m: out("  " + m))
            out("--------------------------------------------------------------------")
            again = input_fn(
                "Try again (re-scan or type a different IP)? [Y/n]: "
            ).strip().lower()
            if again in ("", "y", "yes"):
                out("")
                continue
            out("Setup not completed.")
            return 1
        finally:
            client.close()
        break

    out("Paired successfully.")

    # --- Step 3: timeout ----------------------------------------------
    out("")
    out("Step 3 of 3: Sleep timeout")
    raw = input_fn(
        f"Turn the screen off after how many minutes idle? "
        f"(type a number, or press Enter to keep {cfg.idle_minutes:g}): "
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

    unit = "minute" if minutes == 1 else "minutes"
    out("")
    out("=" * 60)
    out("  All set!")
    out(f"  {name} will sleep after {minutes:g} {unit} of inactivity")
    out("  and wake the moment you move the mouse or press a key.")
    out("")
    out("  Start it now with:  lgtv-easy run")
    out("  (the launcher does this automatically in the background)")
    out("=" * 60)
    return 0
