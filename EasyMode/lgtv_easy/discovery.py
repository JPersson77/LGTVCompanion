"""Find LG WebOS TVs on the local network via SSDP.

WebOS TVs answer SSDP M-SEARCH queries for the LG "second screen" service. We
broadcast a search, collect responders, and try to read a friendly name from
each device's description XML. No third-party libraries required.
"""
from __future__ import annotations

import re
import socket
from dataclasses import dataclass
from typing import List, Optional
from urllib.request import urlopen

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


def _msearch(target: str, timeout: float) -> List[tuple]:
    msg = (
        "M-SEARCH * HTTP/1.1\r\n"
        f"HOST: {SSDP_ADDR}:{SSDP_PORT}\r\n"
        'MAN: "ssdp:discover"\r\n'
        "MX: 2\r\n"
        f"ST: {target}\r\n\r\n"
    ).encode()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    sock.settimeout(timeout)
    responses = []
    try:
        sock.sendto(msg, (SSDP_ADDR, SSDP_PORT))
        while True:
            try:
                data, addr = sock.recvfrom(8192)
            except socket.timeout:
                break
            responses.append((data.decode("latin-1", "replace"), addr[0]))
    finally:
        sock.close()
    return responses


def _friendly_name(location: str, timeout: float = 2.0) -> Optional[str]:
    if not location:
        return None
    try:
        with urlopen(location, timeout=timeout) as resp:
            xml = resp.read().decode("utf-8", "replace")
    except Exception:
        return None
    match = re.search(r"<friendlyName>(.*?)</friendlyName>", xml, re.IGNORECASE)
    return match.group(1).strip() if match else None


def discover(timeout: float = 3.0) -> List[Discovered]:
    """Return de-duplicated LG-looking devices found on the LAN."""
    seen: dict = {}
    for target in SEARCH_TARGETS:
        for text, ip in _msearch(target, timeout):
            lowered = text.lower()
            looks_lg = "lg" in lowered or "webos" in lowered or \
                "lge-com" in lowered
            if ip in seen and not looks_lg:
                continue
            loc_match = re.search(r"location:\s*(\S+)", lowered)
            location = ""
            if loc_match:
                # Recover original-case URL from the raw text.
                start = lowered.index(loc_match.group(1))
                location = text[start:start + len(loc_match.group(1))].strip()
            entry = seen.get(ip, Discovered(ip=ip))
            if location and not entry.location:
                entry.location = location
            if looks_lg or ip not in seen:
                seen[ip] = entry
        if seen:
            # Stop at the first target that yields LG devices to keep it snappy.
            if any("lge" in t for t in [target]):
                break
    results = []
    for entry in seen.values():
        name = _friendly_name(entry.location)
        if name:
            entry.name = name
        results.append(entry)
    return results
