"""Find LG WebOS TVs on the local network via SSDP.

WebOS TVs answer SSDP M-SEARCH queries for the LG "second screen" service. We
broadcast a search, collect responders, and try to read a friendly name from
each device's description XML. No third-party libraries required.

The search is sent out of *every* local network interface (e.g. both a wired
Ethernet link and Wi-Fi). This matters on the common setup where the PC is wired
and the TV is on Wi-Fi: the operating system's default route might send the
multicast out of only one interface, so binding to each address in turn gives the
discovery the best chance of reaching the TV. An optional ``log`` callback lets
the wizard surface exactly what happened (interfaces tried, replies received).
"""
from __future__ import annotations

import re
import socket
import threading
from dataclasses import dataclass
from typing import Callable, List, Optional
from urllib.parse import urlparse
from urllib.request import urlopen

from .netdiag import local_ipv4s

SSDP_ADDR = "239.255.255.250"
SSDP_PORT = 1900
# WebOS TVs respond to this service type; "ssap" / upnp:rootdevice are fallbacks.
SEARCH_TARGETS = [
    "urn:lge-com:service:webos-second-screen:1",
    "urn:schemas-upnp-org:device:MediaRenderer:1",
    "ssdp:all",
]


@dataclass
class Discovered:
    ip: str
    name: str = "LG TV"
    location: str = ""


def _noop(_msg: str) -> None:
    pass


def _search_on(src_ip: Optional[str], targets: List[str],
               timeout: float) -> List[tuple]:
    """Send every M-SEARCH target out of one interface and gather replies.

    ``src_ip`` binds the socket to a specific local interface; ``None`` lets the
    OS pick the default route. All targets are sent up front, then we collect
    replies for ``timeout`` seconds, so the whole sweep for one interface takes
    roughly ``timeout`` regardless of how many targets we try.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    try:
        if src_ip:
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF,
                            socket.inet_aton(src_ip))
            sock.bind((src_ip, 0))
        sock.settimeout(timeout)
        for target in targets:
            msg = (
                "M-SEARCH * HTTP/1.1\r\n"
                f"HOST: {SSDP_ADDR}:{SSDP_PORT}\r\n"
                'MAN: "ssdp:discover"\r\n'
                "MX: 2\r\n"
                f"ST: {target}\r\n\r\n"
            ).encode()
            try:
                sock.sendto(msg, (SSDP_ADDR, SSDP_PORT))
            except OSError:
                continue
        responses = []
        while True:
            try:
                data, addr = sock.recvfrom(8192)
            except socket.timeout:
                break
            except OSError:
                break
            responses.append((data.decode("latin-1", "replace"), addr[0]))
        return responses
    finally:
        sock.close()


def _friendly_name(location: str, expected_ip: Optional[str] = None,
                   timeout: float = 2.0) -> Optional[str]:
    """Fetch a device's friendly name from its description URL.

    The URL comes from an SSDP ``LOCATION`` header, i.e. untrusted data from
    whatever answered on the LAN. To avoid being turned into a request forgery
    (e.g. ``file://`` reads or probing internal services), only plain http(s) is
    followed, only to the host that actually responded, and the response is size
    capped.
    """
    if not location:
        return None
    try:
        parsed = urlparse(location)
    except ValueError:
        return None
    if parsed.scheme not in ("http", "https"):
        return None
    if expected_ip and parsed.hostname != expected_ip:
        return None
    try:
        with urlopen(location, timeout=timeout) as resp:  # noqa: S310 - scheme checked
            xml = resp.read(65536).decode("utf-8", "replace")
    except Exception:
        return None
    match = re.search(r"<friendlyName>(.*?)</friendlyName>", xml, re.IGNORECASE)
    return match.group(1).strip() if match else None


def discover(timeout: float = 3.0,
             log: Optional[Callable[[str], None]] = None) -> List[Discovered]:
    """Return de-duplicated LG-looking devices found on the LAN.

    ``log`` (optional) receives human-readable progress lines so the wizard can
    show the user what discovery is doing and why it may have found nothing.
    """
    out = log or _noop
    sources = local_ipv4s() or [None]
    shown = ", ".join(s for s in sources if s) or "default route"
    out(f"Searching for TVs from {len([s for s in sources if s]) or 1} "
        f"network interface(s): {shown}")

    # Search every interface in parallel so total time stays near `timeout`.
    raw: List[tuple] = []
    lock = threading.Lock()

    def work(src):
        found = _search_on(src, SEARCH_TARGETS, timeout)
        with lock:
            raw.extend(found)

    threads = [threading.Thread(target=work, args=(s,), daemon=True)
               for s in sources]
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout + 2.0)

    seen: dict = {}
    lg_hosts = 0
    for text, ip in raw:
        lowered = text.lower()
        looks_lg = "lg" in lowered or "webos" in lowered or "lge-com" in lowered
        if ip in seen and not looks_lg:
            continue
        loc_match = re.search(r"location:\s*(\S+)", lowered)
        location = ""
        if loc_match:
            start = lowered.index(loc_match.group(1))
            location = text[start:start + len(loc_match.group(1))].strip()
        entry = seen.get(ip, Discovered(ip=ip))
        if location and not entry.location:
            entry.location = location
        if looks_lg:
            lg_hosts += 1
        seen[ip] = entry

    out(f"Received {len(raw)} SSDP reply/replies from {len(seen)} host(s); "
        f"{lg_hosts} look like LG/WebOS devices.")

    results = []
    for entry in seen.values():
        name = _friendly_name(entry.location, expected_ip=entry.ip)
        if name:
            entry.name = name
        results.append(entry)
    if not results:
        out("No devices answered the network search. The TV may be on a "
            "different network, asleep, or have network control disabled.")
    return results
