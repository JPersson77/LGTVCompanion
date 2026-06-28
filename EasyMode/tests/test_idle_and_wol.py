"""Idle backend selection and Wake-on-LAN packet construction."""
import pytest

from lgtv_easy import idle as idle_mod
from lgtv_easy.wol import (broadcast_targets, magic_packet, normalize_mac,
                           send_wol, wake_targets)


def test_idle_returns_float_and_never_raises():
    val = idle_mod.get_idle_seconds()
    assert isinstance(val, float)
    assert val >= 0.0
    assert isinstance(idle_mod.idle_backend_name(), str)


def test_manual_idle_source_is_controllable():
    src = idle_mod.ManualIdle()
    src.set_idle(42)
    assert src.get() >= 41.0
    src.mark_active()
    assert src.get() < 1.0


def test_fake_idle_env_override(monkeypatch):
    monkeypatch.setenv("LGTV_EASY_FAKE_IDLE", "123")
    src = idle_mod.ManualIdle()
    assert src.get() == 123.0


def test_parse_uint_from_gdbus_reply():
    assert idle_mod._parse_uint("(uint64 12345,)") == 12345
    assert idle_mod._parse_uint("(uint32 7,)") == 7
    assert idle_mod._parse_uint("nonsense") is None
    assert idle_mod._parse_uint(None) is None


def test_mutter_backend_absent_without_dbus(monkeypatch):
    # No D-Bus access at all (neither the in-process client nor gdbus answers):
    # neither GNOME nor the freedesktop backend may claim the session.
    monkeypatch.setattr(idle_mod, "_session_uint", lambda *a, **k: None)
    assert idle_mod._mutter_idle_backend() is None
    assert idle_mod._freedesktop_idle_backend() is None


def test_wayland_session_prefers_gnome_idlemonitor(monkeypatch):
    monkeypatch.setattr(idle_mod.sys, "platform", "linux")
    monkeypatch.setenv("XDG_SESSION_TYPE", "wayland")
    monkeypatch.setenv("WAYLAND_DISPLAY", "wayland-0")
    monkeypatch.delenv("DISPLAY", raising=False)

    # Pretend GNOME's IdleMonitor answers with 12.3s of idle (12300 ms).
    def fake(dest, path, interface, member):
        return 12300 if dest == "org.gnome.Mutter.IdleMonitor" else None

    monkeypatch.setattr(idle_mod, "_session_uint", fake)
    name, getter = idle_mod._select_backend()
    assert name == "gnome-idlemonitor"
    assert abs(getter() - 12.3) < 0.5


def test_non_gnome_wayland_uses_freedesktop_screensaver(monkeypatch):
    # KDE Plasma (etc.) Wayland: GNOME's Mutter is absent, but the freedesktop
    # ScreenSaver interface answers - we must pick it, not fall to manual.
    monkeypatch.setattr(idle_mod.sys, "platform", "linux")
    monkeypatch.setenv("XDG_SESSION_TYPE", "wayland")
    monkeypatch.setenv("WAYLAND_DISPLAY", "wayland-0")
    monkeypatch.delenv("DISPLAY", raising=False)

    def fake(dest, path, interface, member):
        if dest == "org.freedesktop.ScreenSaver" and member == "GetSessionIdleTime":
            return 8000  # 8s idle, reported in milliseconds
        return None

    monkeypatch.setattr(idle_mod, "_session_uint", fake)
    name, getter = idle_mod._select_backend()
    assert name == "freedesktop-screensaver"
    assert abs(getter() - 8.0) < 0.5


def test_wayland_without_gnome_falls_back_to_manual_not_x11(monkeypatch):
    # On a non-GNOME Wayland session the X11 tools would lie, so we must NOT pick
    # them even if present - report 'manual' honestly instead.
    monkeypatch.setattr(idle_mod.sys, "platform", "linux")
    monkeypatch.setenv("XDG_SESSION_TYPE", "wayland")
    monkeypatch.setenv("DISPLAY", ":0")          # XWayland present
    monkeypatch.setattr(idle_mod.shutil, "which", lambda name: "/usr/bin/xprintidle")
    # No Mutter and no freedesktop ScreenSaver answering.
    monkeypatch.setattr(idle_mod, "_session_uint", lambda *a, **k: None)
    name, _ = idle_mod._select_backend()
    assert name == "manual"


def test_x11_session_uses_xprintidle(monkeypatch):
    monkeypatch.setattr(idle_mod.sys, "platform", "linux")
    monkeypatch.setenv("XDG_SESSION_TYPE", "x11")
    monkeypatch.delenv("WAYLAND_DISPLAY", raising=False)
    monkeypatch.setenv("DISPLAY", ":0")
    monkeypatch.setattr(idle_mod.shutil, "which", lambda name: "/usr/bin/xprintidle")
    monkeypatch.setattr(idle_mod.subprocess, "check_output", lambda *a, **k: b"5000")
    name, getter = idle_mod._select_backend()
    assert name == "xprintidle"
    assert abs(getter() - 5.0) < 0.1


def test_magic_packet_format():
    pkt = magic_packet("AA:BB:CC:DD:EE:FF")
    assert len(pkt) == 102
    assert pkt[:6] == b"\xff" * 6
    assert pkt[6:12] == bytes.fromhex("AABBCCDDEEFF")
    # MAC repeated 16 times.
    assert pkt[6:12] == pkt[12:18] == pkt[-6:]


def test_normalize_mac_accepts_common_formats():
    expected = bytes.fromhex("AABBCCDDEEFF")
    assert normalize_mac("AA:BB:CC:DD:EE:FF") == expected
    assert normalize_mac("aa-bb-cc-dd-ee-ff") == expected
    assert normalize_mac("AABBCCDDEEFF") == expected


def test_normalize_mac_rejects_bad():
    with pytest.raises(ValueError):
        normalize_mac("not-a-mac")


def test_broadcast_targets_adds_directed_subnet_broadcast():
    # The TV's /24-directed broadcast is added alongside the limited broadcast,
    # which is what makes wake-on-LAN reliable across a Google Wifi mesh.
    targets = broadcast_targets("192.168.86.42")
    assert "255.255.255.255" in targets
    assert "192.168.86.255" in targets
    # A host:port form is tolerated (the port is stripped).
    assert "192.168.86.255" in broadcast_targets("192.168.86.42:3001")
    # De-duplicated, and junk is ignored without raising.
    assert broadcast_targets("", "nonsense", "10.0.0.5", "10.0.0.9") == [
        "255.255.255.255", "10.0.0.255"]


class _FakeSock:
    def __init__(self, sent):
        self._sent = sent

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def setsockopt(self, *a):
        pass

    def sendto(self, _packet, dest):
        self._sent.append(dest)


def _capture_sends(**kwargs):
    sent = []
    import lgtv_easy.wol as wol_mod
    orig = wol_mod.socket.socket
    wol_mod.socket.socket = lambda *a, **k: _FakeSock(sent)
    try:
        send_wol("AA:BB:CC:DD:EE:FF", **kwargs)
    finally:
        wol_mod.socket.socket = orig
    return sent


def test_wake_targets_adds_unicast_to_the_tv():
    # The broadcasts plus a unicast to the TV's own IP (some Wi-Fi WoL only wakes
    # on a directed packet). Port suffixes are stripped; no IP -> broadcasts only.
    targets = wake_targets("192.168.86.39")
    assert targets == ["255.255.255.255", "192.168.86.255", "192.168.86.39"]
    assert wake_targets("192.168.86.39:3001")[-1] == "192.168.86.39"
    assert wake_targets("") == ["255.255.255.255"]


def test_send_wol_hits_every_broadcast_and_both_ports_each_round():
    sent = _capture_sends(broadcast=["255.255.255.255", "192.168.86.255"],
                          repeat=2)
    # 2 rounds x 2 broadcasts x 2 ports (9 and 7) = 8 sends.
    assert len(sent) == 8
    assert {host for host, _port in sent} == {"255.255.255.255", "192.168.86.255"}
    # Both conventional WoL ports are covered, since TVs differ on which they use.
    assert {port for _host, port in sent} == {9, 7}


def test_send_wol_sustained_spreads_rounds_over_time(monkeypatch):
    # A sustained burst sleeps `interval` between rounds so the packets land in a
    # sleeping Wi-Fi TV's forwarding windows instead of arriving as one blip.
    naps = []
    import lgtv_easy.wol as wol_mod
    monkeypatch.setattr(wol_mod.time, "sleep", lambda s: naps.append(s))
    _capture_sends(broadcast="255.255.255.255", repeat=4, interval=0.25)
    assert naps == [0.25, 0.25, 0.25]  # between rounds only (3 gaps for 4 rounds)
