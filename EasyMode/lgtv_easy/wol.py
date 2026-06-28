"""Wake-on-LAN.

WebOS TVs power on from standby when they receive a magic packet (and "Turn on
via Wi-Fi" is enabled on the TV). This is the same mechanism the original app
uses to wake the display.
"""
from __future__ import annotations

import socket
import time


def normalize_mac(mac: str) -> bytes:
    """Turn 'AA:BB:CC:DD:EE:FF' (or with - or no separators) into 6 raw bytes."""
    cleaned = mac.replace(":", "").replace("-", "").replace(".", "").strip()
    if len(cleaned) != 12:
        raise ValueError(f"Invalid MAC address: {mac!r}")
    return bytes.fromhex(cleaned)


def magic_packet(mac: str) -> bytes:
    """Build the 102-byte magic packet: 6x 0xFF then the MAC repeated 16 times."""
    target = normalize_mac(mac)
    return b"\xff" * 6 + target * 16


def broadcast_targets(*ips: str) -> list:
    """Broadcast addresses to aim a magic packet at, best-first.

    Always includes the limited broadcast (255.255.255.255). For each given IPv4
    it also adds that address's /24-directed broadcast (e.g. 192.168.86.42 ->
    192.168.86.255). On a Google/Nest Wifi mesh the limited broadcast is not
    always forwarded between the wired and wireless segments, whereas the
    directed subnet broadcast usually is - so hitting both makes wake-on-LAN far
    more reliable when the PC and TV are on different parts of the same mesh.
    """
    targets = ["255.255.255.255"]
    for ip in ips:
        if not ip:
            continue
        host = ip.rpartition(":")[0] if ":" in ip else ip  # strip any :port
        parts = host.split(".")
        if len(parts) == 4 and all(p.isdigit() for p in parts):
            directed = ".".join(parts[:3] + ["255"])
            if directed not in targets:
                targets.append(directed)
    return targets


def wake_targets(ip: str) -> list:
    """Every address to aim a wake magic packet at: the broadcasts (see
    :func:`broadcast_targets`) plus a unicast to the TV's last-known IP, since
    some Wi-Fi WoL implementations only wake on a directed packet, not a
    broadcast."""
    targets = broadcast_targets(ip)
    if ip:
        host = ip.rpartition(":")[0] if ":" in ip else ip
        if host and host not in targets:
            targets.append(host)
    return targets


def send_wol(mac: str, broadcast="255.255.255.255",
             port: int = 9, repeat: int = 3, interval: float = 0.0,
             ports=None) -> None:
    """Broadcast a magic packet to wake the TV.

    Sent ``repeat`` rounds; with ``interval`` > 0 the rounds are spread over time
    - a *sustained* burst rather than an instant blip. This matters for a TV
    asleep on Wi-Fi behind a mesh router: a single blip is routinely dropped
    before the TV's radio is awake to see it, whereas spreading packets across a
    few seconds lands one in a forwarding window (verified against an LG TV on a
    Nest mesh, which would not wake from one burst but did from a sustained one).

    ``broadcast`` may be a single address or a list/tuple of them (see
    :func:`broadcast_targets`). Each magic packet goes to every target on every
    port in ``ports`` - both conventional WoL ports (9 and 7) by default, since
    different TVs/firmwares listen on one or the other - every round.
    """
    targets = [broadcast] if isinstance(broadcast, str) else list(broadcast)
    dst_ports = tuple(dict.fromkeys(ports if ports is not None else (port, 7)))
    packet = magic_packet(mac)
    rounds = max(1, repeat)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        for i in range(rounds):
            for target in targets:
                for dport in dst_ports:
                    try:
                        sock.sendto(packet, (target, dport))
                    except OSError:
                        continue
            if interval and i + 1 < rounds:
                time.sleep(interval)
