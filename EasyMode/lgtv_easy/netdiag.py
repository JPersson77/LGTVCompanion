"""Small, dependency-free network diagnostics helpers.

These exist so the setup wizard (and the launchers) can show a beginner exactly
*why* finding or reaching their TV failed - which network the PC is on, whether
the TV's ports answer, and so on - instead of a bare "timed out". Everything here
is best-effort and never raises; on any error it returns empty/safe values.
"""
from __future__ import annotations

import platform
import socket
import time
from typing import List, Tuple

# WebOS control ports: plain WebSocket (3000) and TLS WebSocket (3001).
WEBOS_PORTS = (3000, 3001)


def local_ipv4s() -> List[str]:
    """Return this PC's usable IPv4 addresses (one per active interface).

    Beginners on a desktop often have several interfaces (e.g. a wired Ethernet
    link plus Wi-Fi). Knowing them all lets discovery send its search out of each
    one, and lets the diagnostics tell the user which network the PC is actually
    on - the single most common reason a TV "can't be found" is the PC and TV
    being on different subnets.
    """
    ips = set()
    # The address used to reach the internet is the most reliable single answer
    # (a UDP "connect" only sets the route; it sends nothing).
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.connect(("8.8.8.8", 80))
            ips.add(s.getsockname()[0])
        finally:
            s.close()
    except OSError:
        pass
    # Everything the hostname resolves to catches additional NICs (Wi-Fi etc.).
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            ips.add(info[4][0])
    except OSError:
        pass
    usable = [
        ip for ip in ips
        if ip and not ip.startswith("127.") and not ip.startswith("169.254.")
        and ip != "0.0.0.0"
    ]
    return sorted(usable)


def tcp_probe(host: str, port: int, timeout: float = 2.0) -> Tuple[bool, str]:
    """Try to open a TCP connection; return (reachable, human description)."""
    start = time.monotonic()
    try:
        with socket.create_connection((host, port), timeout=timeout):
            ms = (time.monotonic() - start) * 1000.0
            return True, f"open ({ms:.0f} ms)"
    except socket.timeout:
        ms = (time.monotonic() - start) * 1000.0
        return False, f"no response / timed out after {ms:.0f} ms (firewall or wrong IP?)"
    except OSError as exc:
        ms = (time.monotonic() - start) * 1000.0
        return False, f"{type(exc).__name__}: {exc} ({ms:.0f} ms)"


def _is_google_wifi(ip: str) -> bool:
    """192.168.86.x is the default LAN that Google/Nest Wifi routers hand out."""
    return ip.startswith("192.168.86.")


def same_subnet_guess(pc_ips: List[str], tv_ip: str) -> str:
    """A friendly note about whether the TV looks like it's on the PC's network.

    Uses a naive /24 comparison (the common home-router case). It is only a hint,
    so the wording stays soft.
    """
    if not tv_ip:
        return ""
    tv_prefix = tv_ip.rsplit(".", 1)[0]
    for ip in pc_ips:
        if ip.rsplit(".", 1)[0] == tv_prefix:
            return f"TV {tv_ip} looks like it's on the same network as {ip}. Good."
    if pc_ips:
        joined = ", ".join(pc_ips)
        msg = (f"WARNING: TV {tv_ip} does not look like it's on the same network "
               f"as this PC ({joined}). They must share a subnet to talk to each "
               f"other - check both are on the same router/SSID.")
        # Catch the very common Google/Nest Wifi double-NAT case, where one side
        # sits behind the Google router (192.168.86.x) and the other is upstream.
        if _is_google_wifi(tv_ip) and not any(_is_google_wifi(p) for p in pc_ips):
            msg += ("\n  NOTE: The TV's 192.168.86.x address means it's on a "
                    "Google/Nest Wifi network, but this PC is not. Connect the PC "
                    "to the same Google Wifi (plug its Ethernet into a Google Wifi "
                    "LAN port, or join that Wi-Fi) so both get a 192.168.86.x "
                    "address, then run setup again.")
        elif any(_is_google_wifi(p) for p in pc_ips) and not _is_google_wifi(tv_ip):
            msg += ("\n  NOTE: This PC is on a Google/Nest Wifi network "
                    "(192.168.86.x) but the TV is not. Put the TV on the same "
                    "Google Wifi network so they share a subnet.")
        return msg
    return ""


def probe_tv(tv_ip: str, log) -> None:
    """Run and report the standard reachability checks for a TV IP."""
    pc_ips = local_ipv4s()
    log(f"This PC's network address(es): {', '.join(pc_ips) if pc_ips else '(none detected!)'}")
    note = same_subnet_guess(pc_ips, tv_ip)
    if note:
        log(note)
    for port in WEBOS_PORTS:
        kind = "plain ws" if port == 3000 else "secure wss"
        ok, detail = tcp_probe(tv_ip, port)
        mark = "OK  " if ok else "FAIL"
        log(f"  [{mark}] TCP {tv_ip}:{port} ({kind}) - {detail}")
    log("If both ports FAIL: confirm the TV's IP (Settings > Network), and that")
    log("the TV setting 'LG Connect Apps' / 'Mobile TV On' / network control is enabled.")


def env_summary() -> List[str]:
    """A compact environment fingerprint to paste into a bug report."""
    from . import __version__
    ips = local_ipv4s()
    return [
        f"App version : {__version__}",
        f"OS          : {platform.platform()}",
        f"Python      : {platform.python_version()}",
        f"Hostname    : {socket.gethostname()}",
        f"This PC IPs : {', '.join(ips) if ips else '(none detected)'}",
    ]
