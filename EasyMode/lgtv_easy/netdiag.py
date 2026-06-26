"""Small, dependency-free network diagnostics helpers.

These exist so the setup wizard (and the launchers) can show a beginner exactly
*why* finding or reaching their TV failed - which network the PC is on, whether
the TV's ports answer, and so on - instead of a bare "timed out". Everything here
is best-effort and never raises; on any error it returns empty/safe values.
"""
from __future__ import annotations

import platform
import re
import socket
import subprocess
import sys
import time
from typing import List, Optional, Tuple

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


def subnet_report(tv_ip: str, log) -> None:
    """Report the PC's network address(es) and whether the TV shares the subnet.

    Fast and non-blocking (no TCP connections), so callers can show it the moment
    a TV IP is known - the subnet mismatch is the most common reason a TV can't be
    reached, and the user shouldn't have to wait for a timeout to find out.
    """
    pc_ips = local_ipv4s()
    log(f"This PC's network address(es): {', '.join(pc_ips) if pc_ips else '(none detected!)'}")
    note = same_subnet_guess(pc_ips, tv_ip)
    if note:
        log(note)


def probe_tv(tv_ip: str, log) -> None:
    """Run and report the standard reachability checks for a TV IP."""
    subnet_report(tv_ip, log)
    for port in WEBOS_PORTS:
        kind = "plain ws" if port == 3000 else "secure wss"
        ok, detail = tcp_probe(tv_ip, port)
        mark = "OK  " if ok else "FAIL"
        log(f"  [{mark}] TCP {tv_ip}:{port} ({kind}) - {detail}")
    log("If both ports FAIL: confirm the TV's IP (Settings > Network), and that")
    log("the TV setting 'LG Connect Apps' / 'Mobile TV On' / network control is enabled.")


_MAC_RE = re.compile(r"([0-9a-fA-F]{2}(?:[:-][0-9a-fA-F]{2}){5})")
_IP_RE = re.compile(r"\b(\d{1,3}(?:\.\d{1,3}){3})\b")


def canon_mac(mac: str) -> str:
    """Normalise a MAC to upper-case colon form (e.g. 'B8:16:5F:72:64:C6').

    Accepts the ':' , '-' or bare-hex spellings the OS tools and users produce so
    a stored address always compares equal to one freshly read from the system.
    Returns '' if the input doesn't contain a MAC.
    """
    m = _MAC_RE.search(mac or "")
    return m.group(1).replace("-", ":").upper() if m else ""


def _warm_arp(ip: str) -> None:
    """Provoke an ARP entry by briefly touching the host (the SYN resolves the
    MAC even if the port is closed), so the lookup below has something to read."""
    for port in (3001, 3000, 80):
        try:
            socket.create_connection((ip, port), timeout=0.6).close()
            return
        except OSError:
            continue


def mac_for_ip(ip: str, timeout: float = 4.0) -> str:
    """Best-effort lookup of a device's MAC from the OS ARP/neighbour table.

    Lets Easy Mode store the TV's hardware address automatically and use
    Wake-on-LAN to power it back on from deep standby - no manual MAC hunting.
    Returns "" if it can't be determined; never raises.
    """
    if not ip or ip.startswith("127.") or ip in ("localhost", "::1"):
        return ""
    _warm_arp(ip)
    for out in _arp_outputs(ip, timeout):
        # Only trust a MAC found on a line that names the target IP, so we never
        # pick up a neighbouring device's address from a full table dump.
        for line in out.splitlines():
            if ip not in line:
                continue
            m = _MAC_RE.search(line)
            if m:
                return m.group(1).replace("-", ":").upper()
    return ""


def _arp_commands(ip: str) -> "list":
    if sys.platform.startswith("win"):
        return [["arp", "-a", ip], ["arp", "-a"]]
    return [["ip", "neigh", "show", ip], ["arp", "-n", ip],
            ["ip", "neigh"], ["arp", "-n"]]


def _arp_outputs(ip: str, timeout: float = 4.0):
    for cmd in _arp_commands(ip):
        try:
            yield subprocess.run(cmd, capture_output=True, text=True,
                                 timeout=timeout).stdout or ""
        except Exception:  # noqa: BLE001 - tool missing, timeout, etc.
            continue


def arp_dump(ip: str) -> str:
    """Raw ARP/neighbour output for the target IP, for diagnostics."""
    _warm_arp(ip)
    lines = []
    for out in _arp_outputs(ip):
        for line in out.splitlines():
            if ip in line:
                lines.append(line.strip())
    return "\n".join(lines) if lines else "(no ARP entry found for this IP)"


def _arp_dump_commands() -> "list":
    """Commands that print the *whole* ARP/neighbour table (no IP filter)."""
    if sys.platform.startswith("win"):
        return [["arp", "-a"]]
    return [["ip", "neigh"], ["arp", "-n"], ["arp", "-a"]]


def arp_table(timeout: float = 4.0) -> "List[Tuple[str, str]]":
    """Parse the OS ARP/neighbour table into ``(ip, mac)`` pairs.

    The reverse of :func:`mac_for_ip`, this is what lets Easy Mode follow a TV
    that DHCP has moved to a new address: the saved IP stops answering, but the
    MAC is forever, so we look the MAC up here to learn its current IP. MACs come
    back upper-cased and colon-separated. Best-effort: returns ``[]`` if no ARP
    tool is available, and never raises.
    """
    pairs: List[Tuple[str, str]] = []
    seen = set()
    for cmd in _arp_dump_commands():
        try:
            out = subprocess.run(cmd, capture_output=True, text=True,
                                 timeout=timeout).stdout or ""
        except Exception:  # noqa: BLE001 - tool missing, timeout, etc.
            continue
        # One ARP entry per line: an IP and the MAC it resolved to belong
        # together. Lines without both (incomplete/FAILED entries) are skipped.
        for line in out.splitlines():
            mac_m = _MAC_RE.search(line)
            ip_m = _IP_RE.search(line)
            if not mac_m or not ip_m:
                continue
            mac = mac_m.group(1).replace("-", ":").upper()
            if mac == "00:00:00:00:00:00" or mac.lower() == "ff:ff:ff:ff:ff:ff":
                continue
            entry = (ip_m.group(1), mac)
            if entry not in seen:
                seen.add(entry)
                pairs.append(entry)
        if pairs:
            break  # the first tool that produced entries is enough
    return pairs


def ip_for_mac(mac: str, timeout: float = 4.0) -> str:
    """Return the IP currently bound to ``mac`` per the OS ARP table, or ''.

    Only reads what the table already knows; call :func:`find_ip_by_mac` to also
    sweep the subnet when the TV isn't cached yet.
    """
    target = canon_mac(mac)
    if not target:
        return ""
    for ip, found in arp_table(timeout):
        if found == target:
            return ip
    return ""


def sweep_arp(settle: float = 1.5) -> None:
    """Provoke ARP entries for every host on this PC's /24(s).

    When the TV moves to a new DHCP address nothing in the ARP table points at it
    until a packet is sent there. A tiny UDP datagram to each host on the subnet
    forces the kernel to resolve - and cache - the MAC of whoever is online, so a
    follow-up :func:`ip_for_mac` can find the TV at its new address. Hosts that
    are off simply never answer. Best-effort and silent; never raises.
    """
    prefixes = set()
    for ip in local_ipv4s():
        parts = ip.split(".")
        if len(parts) == 4 and all(p.isdigit() for p in parts):
            prefixes.add(".".join(parts[:3]))
    if not prefixes:
        return
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    except OSError:
        return
    try:
        for prefix in prefixes:
            for host in range(1, 255):
                try:
                    sock.sendto(b"", (f"{prefix}.{host}", 9))
                except OSError:
                    continue
    finally:
        sock.close()
    # Give the kernel a moment to receive the ARP replies before we read them.
    time.sleep(max(0.0, settle))


def find_ip_by_mac(mac: str, settle: float = 1.5) -> str:
    """Best-effort current IP for ``mac`` on the LAN, '' if it can't be found.

    Checks the ARP table first (instant); if the MAC isn't cached - e.g. the TV
    just took a new DHCP lease - it sweeps the local subnet to repopulate the
    table and looks again. Never raises.
    """
    target = canon_mac(mac)
    if not target:
        return ""
    found = ip_for_mac(target)
    if found:
        return found
    try:
        sweep_arp(settle=settle)
    except Exception:  # noqa: BLE001 - best effort
        pass
    return ip_for_mac(target)


def webos_hosts(probe_timeout: float = 0.6) -> "List[str]":
    """Live hosts on the LAN that answer on a WebOS control port.

    A MAC-free way to locate the TV when SSDP discovery is blocked - which is the
    norm on Google/Nest Wifi and other mesh routers that don't forward multicast.
    Sweeps the subnet to learn which hosts are up, then probes the WebOS ports on
    each. Returns the matching IP(s) (usually just the TV). Best-effort: returns
    ``[]`` on any error and never raises.
    """
    try:
        sweep_arp()
    except Exception:  # noqa: BLE001 - best effort
        pass
    live: List[str] = []
    seen = set()
    for ip, _mac in arp_table():
        if ip not in seen:
            seen.add(ip)
            live.append(ip)
    found: List[str] = []
    for ip in live:
        for port in WEBOS_PORTS:
            ok, _ = tcp_probe(ip, port, timeout=probe_timeout)
            if ok:
                found.append(ip)
                break
    return found


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
