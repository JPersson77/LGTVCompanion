"""Self-test and repair for the TV connection.

This is the "why can't I reach my TV, and can the app fix it itself" engine. The
recovery in :mod:`recovery` (used by the one-shot CLI/GUI actions) is deliberately
lightweight: it tries the saved IP and, if that fails, relocates the TV once by
MAC. That heals the single most common fault - DHCP moved the TV - but it gives up
quietly on everything else and, crucially, does its work silently.

A beginner who presses "Test my TV" and sees ``[Errno 113] No route to host``
deserves more than a dead end. :func:`repair` is the escalating, fully-narrated
counterpart: it reports which network the PC is on, probes the TV's control ports,
relocates the TV by MAC *and* by discovery, then probes and connects to each
candidate it finds - persisting the corrected address (and the learned MAC/port)
when it succeeds. Whatever the outcome, it returns a plain-language summary plus
the full transcript, so the user either gets a fixed connection or a clear reason
why not.

Everything here is best-effort and never raises: it reuses the verified helpers in
:mod:`netdiag`, :mod:`discovery` and :mod:`webos`, and only orchestrates them.
"""
from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import Callable, List, Optional

from . import discovery
from . import netdiag
from .config import Config
from .webos import PairingError, WebOSClient, pair_with_fallback


@dataclass
class RepairResult:
    """The outcome of a :func:`repair` run.

    ``ok`` is the only thing most callers need: True means the TV is reachable
    now. ``repaired`` distinguishes "it was fine all along / a transient blip"
    from "we actively fixed something" (a moved IP, a changed port). ``client``
    is a live, connected client when ``connect=True`` and the repair succeeded -
    the caller owns it and must close it; it is ``None`` otherwise.
    """

    ok: bool = False
    repaired: bool = False
    old_ip: str = ""
    new_ip: str = ""
    summary: str = ""
    steps: List[str] = field(default_factory=list)
    error: str = ""
    client: Optional[WebOSClient] = None


def _host(addr: str) -> str:
    """The bare host of a possibly ``host:port`` address (IPv6 left intact)."""
    if addr and ":" in addr and not addr.startswith("["):
        return addr.rpartition(":")[0]
    return addr


def _ports_for(addr: str) -> List[int]:
    """The TCP port(s) to probe for an address: an explicit one, else both
    standard WebOS control ports."""
    if addr and ":" in addr and not addr.startswith("["):
        port = addr.rpartition(":")[2]
        if port.isdigit():
            return [int(port)]
    return list(netdiag.WEBOS_PORTS)


def quick_health_check(cfg: Config, *, timeout: float = 2.0,
                       log: Optional[Callable[[str], None]] = None) -> bool:
    """Fast, non-intrusive "is the saved TV reachable right now?" check.

    Just opens a TCP connection to the TV's control port(s) - no pairing, no
    discovery, no LAN sweep - so it is cheap enough to run on every app startup
    to decide whether a full :func:`repair` is even warranted. Returns False (not
    an exception) when there is no saved address or nothing answers.
    """
    out = log or (lambda _m: None)
    host = _host(cfg.device.ip)
    if not host:
        return False
    for port in _ports_for(cfg.device.ip):
        ok, _detail = netdiag.tcp_probe(host, port, timeout=timeout)
        if ok:
            out(f"TV control port {host}:{port} is open.")
            return True
    return False


def _looks_like_pairing_problem(exc: Exception) -> bool:
    """True when a connection failure is the TV refusing us, not a routing fault.

    A reachable TV that rejects the registration (a wiped/blocked client-key, a
    pending on-screen prompt nobody pressed) is a pairing problem the user fixes
    by re-pairing - not something relocating to a new IP could ever help.
    """
    if isinstance(exc, PairingError):
        return True
    msg = str(exc).lower()
    return any(token in msg for token in (
        "registration", "pairing", "rejected", "denied", "forbidden",
        "client-key", "401", "403"))


def _blink(client: WebOSClient, out: Callable[[str], None]) -> None:
    """Confirm control end to end by turning the screen off then back on."""
    try:
        out("Confirming control: turning the screen off for a moment...")
        client.screen_off()
        time.sleep(2.0)
        client.screen_on()
        out("Screen turned back on. The TV is responding. ✓")
    except Exception as exc:  # noqa: BLE001 - the blink is a confirmation, not the goal
        out(f"(Connected, but the screen on/off check failed: {exc})")


def _persist_learnings(cfg: Config, client: WebOSClient, ip: str,
                       old_ip: str, res: RepairResult,
                       out: Callable[[str], None], persist: bool) -> None:
    """Save anything the successful connection taught us about the TV.

    The corrected IP, the port that actually worked (plain vs secure), the
    client-key the TV (re)issued, and - so future relocation can track the TV by
    its unchanging hardware address - its MAC. Best-effort: a failure to write the
    config never fails the repair.
    """
    changed = False
    if ip != old_ip:
        cfg.device.ip = ip
        res.repaired = True
        changed = True
        out(f"Saved the TV's new address: {old_ip or '(unset)'} -> {ip}.")
    if client.secure != cfg.device.secure:
        cfg.device.secure = client.secure
        changed = True
        out(f"Saved the working connection type: "
            f"{'secure wss (3001)' if client.secure else 'plain ws (3000)'}.")
    if client.client_key and client.client_key != cfg.device.key:
        cfg.device.key = client.client_key
        changed = True
    if not cfg.device.mac:
        mac = ""
        try:
            mac = client.get_mac()
        except Exception:  # noqa: BLE001 - newer panels block the info APIs
            mac = ""
        if not mac:
            mac = netdiag.mac_for_ip(_host(ip))
        if mac:
            cfg.device.mac = mac
            changed = True
            out(f"Learned the TV's hardware address {mac} for Wake-on-LAN.")
    if changed and persist:
        try:
            cfg.save()
        except Exception:  # noqa: BLE001 - persistence is best-effort
            pass


def repair(cfg: Config, *, log: Optional[Callable[[str], None]] = None,
           persist: bool = True, connect: bool = False, blink: bool = False,
           on_prompt: Optional[Callable[[], None]] = None,
           discover_timeout: float = 3.0, connect_timeout: float = 8.0,
           prompt_timeout: float = 8.0) -> RepairResult:
    """Diagnose why the TV is unreachable and fix it if at all possible.

    The escalation, each step narrated through ``log`` and recorded in
    ``result.steps``:

      1. Report which network this PC is on, and whether the saved TV IP looks
         like it shares that subnet (incl. the Google/Nest Wifi double-NAT trap).
      2. Probe the saved address's WebOS control ports. If they answer, try to
         (re)connect there - a success means the TV never moved and we're done.
      3. If the saved address is dead, relocate the TV: by its MAC when known
         (exact, survives DHCP changes), else by SSDP/port-scan discovery. Reuses
         :func:`discovery.locate_tv`, which refuses to guess between two TVs.
      4. Probe and connect to the relocated address; on success, persist the
         corrected IP/MAC/port and (optionally) blink the screen to prove control.
      5. On failure, compose a plain-language summary of the most likely cause
         and what to try next.

    ``connect=True`` returns the live client (caller closes it); ``connect=False``
    verifies the fix, persists it, and closes the connection - the cheap mode for
    a startup self-test. Never raises.
    """
    out_steps: List[str] = []

    def out(msg: str) -> None:
        out_steps.append(msg)
        if log:
            log(msg)

    res = RepairResult(old_ip=_host(cfg.device.ip), steps=out_steps)
    saved = _host(cfg.device.ip)

    try:
        return _repair_impl(cfg, res, saved, out, persist=persist,
                            connect=connect, blink=blink, on_prompt=on_prompt,
                            discover_timeout=discover_timeout,
                            connect_timeout=connect_timeout,
                            prompt_timeout=prompt_timeout)
    except Exception as exc:  # noqa: BLE001 - this routine must never raise
        res.error = str(exc)
        res.ok = False
        if not res.summary:
            res.summary = f"The repair check hit an unexpected problem: {exc}"
        return res


def _repair_impl(cfg, res, saved, out, *, persist, connect, blink, on_prompt,
                 discover_timeout, connect_timeout, prompt_timeout):
    pc_ips = netdiag.local_ipv4s()
    out("Checking how this PC is connected to the network...")
    netdiag.subnet_report(saved, out)

    def try_connect(ip: str, note: str) -> bool:
        out(f"{note} {ip} ...")
        # WebOSClient handles both a bare IP (standard port from the secure flag)
        # and an explicit "host:port" (used in tests / non-standard setups).
        client = WebOSClient(ip, secure=cfg.device.secure, timeout=connect_timeout)
        try:
            pair_with_fallback(client, client_key=cfg.device.key,
                               on_prompt=on_prompt, prompt_timeout=prompt_timeout,
                               prefer_secure=cfg.device.secure, log=out)
        except Exception as exc:  # noqa: BLE001 - expected when the TV is unreachable
            res.error = str(exc)
            out(f"  Could not connect at {ip}: {exc}")
            try:
                client.close()
            except Exception:  # noqa: BLE001
                pass
            return False
        out(f"  Connected to the TV at {ip}. ✓")
        _persist_learnings(cfg, client, _host(ip), saved, res, out, persist)
        if blink:
            _blink(client, out)
        res.ok = True
        res.new_ip = _host(ip)
        if connect:
            res.client = client
        else:
            try:
                client.close()
            except Exception:  # noqa: BLE001
                pass
        return True

    # ----- step 2: is the saved address still the TV? --------------------
    saved_reachable = False
    if saved:
        out(f"Checking the saved TV address {saved} ...")
        for port in _ports_for(cfg.device.ip):
            ok, detail = netdiag.tcp_probe(saved, port)
            kind = "plain ws" if port == 3000 else ("secure wss" if port == 3001 else "control")
            out(f"  [{'OK' if ok else 'FAIL'}] TCP {saved}:{port} ({kind}) - {detail}")
            saved_reachable = saved_reachable or ok
        if saved_reachable and try_connect(cfg.device.ip, "Reconnecting to the saved address"):
            res.summary = f"Your TV is responding at {res.new_ip}. ✓"
            return res

    # ----- step 3-4: the saved address is dead - find the TV again --------
    out("The saved address didn't lead to a working connection; "
        "searching the network for the TV...")
    new_ip = None
    try:
        new_ip = discovery.locate_tv(cfg.device.mac, timeout=discover_timeout, log=out)
    except Exception as exc:  # noqa: BLE001 - discovery is best-effort
        out(f"Network search failed: {exc}")
    if new_ip and _host(new_ip) != saved:
        if try_connect(new_ip, "Found the TV at a new address; connecting to"):
            res.summary = (f"Fixed it - your TV had moved to {res.new_ip}, "
                           f"and it's responding now. ✓")
            return res

    # ----- step 5: still broken - explain the most likely cause ----------
    # The summary is the conclusion, not a step: each caller surfaces it once (the
    # CLI prints it under the transcript, the GUI shows it on the status line), so
    # it is deliberately not echoed into the step log here.
    res.ok = False
    res.summary = _failure_summary(cfg, saved, pc_ips, saved_reachable, new_ip, res)
    return res


def _failure_summary(cfg, saved, pc_ips, saved_reachable, found_ip, res) -> str:
    """The single most useful sentence we can say about why repair failed."""
    if not pc_ips:
        return ("This PC doesn't appear to be on any network. Check its Wi-Fi or "
                "Ethernet connection, then try again.")
    if saved_reachable:
        # The address answered a TCP probe but wouldn't complete a session.
        if res.error and _looks_like_pairing_problem(Exception(res.error)):
            return ("Your TV answered but refused the connection - the pairing may "
                    "have been cleared on the TV. Use 'Re-run setup' to pair again.")
        return (f"Your TV answers at {saved} but didn't complete a connection "
                f"({res.error or 'unknown error'}). It may be busy or mid-update; "
                "try again in a moment, or 'Re-run setup' to re-pair.")
    note = netdiag.same_subnet_guess(pc_ips, saved)
    if note and "WARNING" in note:
        return ("Your TV and this PC look like they're on different networks, so "
                "they can't reach each other. Put both on the same router/Wi-Fi "
                "(see the details above), then try again.")
    if found_ip:
        return (f"Found a TV at {_host(found_ip)} but couldn't connect to it "
                f"({res.error or 'unknown error'}). Make sure it's the right TV and "
                "that network control is enabled on it.")
    return ("Couldn't find your TV on the network. It's most likely turned off, in "
            "deep standby, or on a different Wi-Fi. Switch it on and make sure its "
            "network control setting (LG Connect Apps / 'Mobile TV On') is enabled, "
            "then try again.")
