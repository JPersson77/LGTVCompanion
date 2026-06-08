"""Idle backend selection and Wake-on-LAN packet construction."""
import pytest

from lgtv_easy import idle as idle_mod
from lgtv_easy.wol import magic_packet, normalize_mac


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
