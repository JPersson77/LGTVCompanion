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

from . import autostart as autostart_mod
from .config import Config, Device
from .discovery import Discovered, discover
from .netdiag import env_summary, mac_for_ip, probe_tv
from .webos import WebOSClient, pair_with_fallback
from .wol import normalize_mac


def _yes(value: str, default_yes: bool = False) -> bool:
    v = value.strip().lower()
    if v == "":
        return default_yes
    return v in ("y", "yes")


def _choose_tv(input_fn, out, discover_fn) -> Tuple[str, str]:
    """Step 1: let the user pick a discovered TV or type an IP. Returns (ip, name)."""
    out("Step 1: Find your TV")
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
    out("inactivity, just like a normal PC monitor. A few quick steps.")
    out("")
    out("System info (handy if you need to report a problem):")
    for line in env_summary():
        out("  " + line)
    out("")

    # Settings mode: if a TV is already paired, don't make the user re-pair just
    # to change a timeout or toggle auto-start - offer to keep it and skip ahead.
    if cfg.setup_complete and cfg.device.paired:
        out(f"Already set up: {cfg.device.name} at {cfg.device.ip}.")
        if _yes(input_fn("Keep this TV and just change settings? [Y/n]: "),
                default_yes=True):
            return _finish(cfg, cfg.device.ip, cfg.device.name, cfg.device.key,
                           cfg.device.mac, cfg.device.secure, input_fn, out)
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
        out("Step 2: Pair with the TV")
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

    # Grab the TV's hardware (MAC) address while we're connected, so Wake-on-LAN
    # can switch it back on if the energy-saving "full power off" is enabled.
    mac = cfg.device.mac
    if not mac:
        detected = mac_for_ip(ip)
        if detected:
            mac = detected
            out(f"Found the TV's hardware address ({mac}) for Wake-on-LAN.")

    return _finish(cfg, ip, name, key, mac, client.secure, input_fn, out)


def _finish(cfg, ip, name, key, mac, secure, input_fn, out) -> int:
    """Steps 3-5 (timeout, energy, start-at-login), then save and summarise.

    Shared by first-time setup and the "just change settings" path, so both flows
    expose exactly the same options.
    """
    # --- Step 3: screen-off timeout -----------------------------------
    out("")
    out("Step 3: Screen-off timeout")
    raw = input_fn(
        f"Blank the screen after how many minutes idle? "
        f"(type a number, or press Enter to keep {cfg.idle_minutes:g}): "
    ).strip()
    minutes = cfg.idle_minutes
    if raw:
        try:
            minutes = max(0.5, float(raw))
        except ValueError:
            out(f"Not a number; keeping {cfg.idle_minutes:g} minutes.")

    # --- Step 4: energy saving (optional) -----------------------------
    out("")
    out("Step 4: Energy saving (optional - press Enter to skip each)")
    mute = _yes(input_fn("Mute the TV speakers while the screen is off? [y/N]: "))
    deep = _yes(input_fn(
        "For the LOWEST power use, fully switch the TV off after a longer idle? "
        "[y/N]: "))
    deep_minutes = cfg.deep_off_minutes
    if deep:
        out("  Note: this uses Wake-on-LAN to switch the TV back on, so enable")
        out("  'Turn on via Wi-Fi'/'Quick Start+' on the TV. Windows may briefly")
        out("  rearrange windows when the TV powers off and on.")
        raw2 = input_fn(
            f"  Fully power off after how many TOTAL minutes idle? "
            f"(more than {minutes:g}) [{deep_minutes:g}]: "
        ).strip()
        if raw2:
            try:
                deep_minutes = max(minutes + 0.5, float(raw2))
            except ValueError:
                pass
        # Wake-on-LAN needs the TV's MAC; auto-detect it, but let the user
        # confirm/override (and supply one if detection came up empty).
        detected = mac or mac_for_ip(ip)
        prompt = (f"  TV's Wake-on-LAN MAC [{detected}]: " if detected
                  else "  TV's Wake-on-LAN MAC (e.g. AA:BB:CC:DD:EE:FF): ")
        typed = input_fn(prompt).strip()
        if typed:
            try:
                normalize_mac(typed)
                mac = typed
            except ValueError:
                out("  That doesn't look like a MAC address; leaving it unset.")
                mac = detected
        else:
            mac = detected
        if not mac:
            out("  No MAC set, so the TV can't be woken from a full power-off -")
            out("  it will only blank the screen. Add one later with: "
                "lgtv-easy set --mac <addr>")

    # --- Step 5: start at login ---------------------------------------
    out("")
    out("Step 5: Start automatically")
    auto = _yes(input_fn(
        "Start Easy Mode automatically when you log in? [Y/n]: "), default_yes=True)

    # Save the configuration FIRST, so setup is complete no matter what happens
    # when we (best-effort) register the auto-start entry.
    cfg.device = Device(name=name, ip=ip, mac=mac, key=key, secure=secure)
    cfg.idle_minutes = minutes
    cfg.idle_enabled = True
    cfg.mute_on_sleep = mute
    cfg.deep_off_enabled = deep
    cfg.deep_off_minutes = deep_minutes
    cfg.setup_complete = True
    cfg.save()

    auto_msg = autostart_mod.set_enabled(auto)

    unit = "minute" if minutes == 1 else "minutes"
    out("")
    out("=" * 60)
    out("  All set!")
    out(f"  {name} will blank after {minutes:g} {unit} of inactivity")
    if deep:
        du = "minute" if deep_minutes == 1 else "minutes"
        out(f"  and fully power off after {deep_minutes:g} {du} for the lowest energy use,")
        out("  waking on a key press or mouse move.")
    else:
        out("  and wake the moment you move the mouse or press a key.")
    out(f"  {auto_msg[0].upper()}{auto_msg[1:]}.")
    out("")
    out("  Start it now with:  lgtv-easy run")
    out("  (the launcher does this automatically in the background)")
    out("=" * 60)
    return 0
