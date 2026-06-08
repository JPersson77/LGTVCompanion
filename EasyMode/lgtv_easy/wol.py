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


def send_wol(mac: str, broadcast: str = "255.255.255.255",
             port: int = 9, repeat: int = 3) -> None:
    """Broadcast a magic packet to wake the TV. Sent a few times for reliability."""
    packet = magic_packet(mac)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        for _ in range(max(1, repeat)):
            sock.sendto(packet, (broadcast, port))
