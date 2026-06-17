"""Wake-on-LAN.

WebOS TVs power on from standby when they receive a magic packet (and "Turn on
via Wi-Fi" is enabled on the TV). This is the same mechanism the original app
uses to wake the display.
"""
from __future__ import annotations

import socket


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


def _subnet_broadcasts() -> "list[str]":
    """Subnet-directed broadcast address(es) for this PC, e.g. 192.168.86.255.

    The global 255.255.255.255 broadcast is sometimes dropped on mesh/segmented
    networks (Google/Nest Wifi among them), whereas the per-subnet directed
    broadcast usually still reaches the TV. We derive one per local interface
    (assuming the common /24) so a wake works whether the PC is on Ethernet or
    Wi-Fi. Best-effort: returns [] if the local addresses can't be determined.
    """
    try:
        from .netdiag import local_ipv4s
        nets = []
        for ip in local_ipv4s():
            parts = ip.split(".")
            if len(parts) == 4:
                nets.append(".".join(parts[:3]) + ".255")
        return nets
    except Exception:  # noqa: BLE001 - never let WOL fail over address discovery
        return []


def send_wol(mac: str, broadcast: str = "255.255.255.255",
             port: int = 9, repeat: int = 3) -> None:
    """Broadcast a magic packet to wake the TV. Sent a few times for reliability.

    Sends to the global broadcast plus each interface's subnet-directed broadcast
    (so a TV on a Google/Nest Wifi mesh is reached whether the PC is wired or on
    Wi-Fi), on both common WOL ports (9 and 7).
    """
    packet = magic_packet(mac)
    # De-duplicate while keeping the explicit broadcast first.
    targets = [broadcast] + [b for b in _subnet_broadcasts() if b != broadcast]
    ports = {port, 9, 7}
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        for _ in range(max(1, repeat)):
            for dest in targets:
                for p in ports:
                    try:
                        sock.sendto(packet, (dest, p))
                    except OSError:
                        continue
