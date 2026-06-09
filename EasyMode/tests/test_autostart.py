"""Auto-start at login: enable creates an entry, disable removes it."""
from lgtv_easy import autostart


def test_enable_disable_roundtrip_linux(tmp_path, monkeypatch):
    # Force the Linux autostart location into a temp dir so the test is hermetic.
    monkeypatch.setattr(autostart.os, "name", "posix")
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path))

    assert autostart.is_enabled() is False
    path = autostart.enable()
    assert autostart.is_enabled() is True
    body = open(path, encoding="utf-8").read()
    assert "Desktop Entry" in body
    assert "lgtv_easy run" in body

    assert autostart.disable() is True
    assert autostart.is_enabled() is False
    # Disabling again is a harmless no-op.
    assert autostart.disable() is False


def test_set_enabled_reports_status(tmp_path, monkeypatch):
    monkeypatch.setattr(autostart.os, "name", "posix")
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path))
    msg_on = autostart.set_enabled(True)
    assert "ENABLED" in msg_on
    assert autostart.is_enabled() is True
    msg_off = autostart.set_enabled(False)
    assert "DISABLED" in msg_off
    assert autostart.is_enabled() is False
