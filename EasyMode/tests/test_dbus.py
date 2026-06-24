"""Unit tests for the tiny hand-rolled D-Bus client (lgtv_easy._dbus).

These cover the pure marshalling/parsing helpers and the never-raise contract
without needing a live session bus (CI has none). The live wire format is
exercised end-to-end by the idle integration on a real GNOME box.
"""
import struct

import pytest

from lgtv_easy import _dbus


def test_pad_to_alignment():
    assert _dbus._pad(0, 8) == 0
    assert _dbus._pad(1, 8) == 7
    assert _dbus._pad(8, 8) == 0
    assert _dbus._pad(5, 4) == 3


def test_marshal_string_and_signature():
    assert _dbus._marshal_string("ab") == struct.pack("<I", 2) + b"ab\x00"
    assert _dbus._marshal_signature("t") == b"\x01t\x00"
    assert _dbus._marshal_signature("") == b"\x00\x00"


def test_decode_uint_handles_each_supported_type():
    dec = _dbus._Connection._decode_uint
    assert dec("u", struct.pack("<I", 4242)) == 4242
    assert dec("t", struct.pack("<Q", 2 ** 40)) == 2 ** 40
    assert dec("i", struct.pack("<i", -5)) == -5
    assert dec("x", struct.pack("<q", -7)) == -7


def test_decode_uint_rejects_unknown_signature():
    with pytest.raises(_dbus._DBusErrorReply):
        _dbus._Connection._decode_uint("s", b"\x00\x00\x00\x00")


def test_parse_fields_extracts_signature_and_reply_serial():
    # Build a header-field array exactly as a bus would: REPLY_SERIAL (code 5,
    # type 'u') = 42 followed by SIGNATURE (code 8, type 'g') = 't'.
    raw = struct.pack("<B", 5) + _dbus._marshal_signature("u")
    raw += b"\x00" * _dbus._pad(len(raw), 4)
    raw += struct.pack("<I", 42)
    raw += b"\x00" * _dbus._pad(len(raw), 8)          # next field struct is 8-aligned
    raw += struct.pack("<B", 8) + _dbus._marshal_signature("g")
    raw += _dbus._marshal_signature("t")             # the variant value is a signature

    sig, reply_serial, error = _dbus._Connection._parse_fields(raw)
    assert sig == "t"
    assert reply_serial == 42
    assert error is None


def test_session_get_uint_never_raises_and_disables_without_a_bus(monkeypatch):
    # Point at a socket that doesn't exist: every call must return None (not
    # raise), and after a few failures the native path disables itself so it
    # stops adding cost on a system where it simply can't connect.
    monkeypatch.setenv("DBUS_SESSION_BUS_ADDRESS",
                       "unix:path=/nonexistent/lgtv-test-bus-socket")
    conn = _dbus._Connection()
    for _ in range(_dbus._MAX_CONN_FAILURES):
        assert conn.get_uint("a.b", "/a", "a.b", "C") is None
    assert conn._disabled is True
    assert conn.get_uint("a.b", "/a", "a.b", "C") is None
