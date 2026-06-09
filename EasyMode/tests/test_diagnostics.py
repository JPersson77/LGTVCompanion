"""Tests for the wizard diagnostics, secure-port fallback and retry-on-failure.

These cover the user-reported scenario: the TV can't be found/reached, and we
need the wizard to explain why (instead of silently failing and the window
closing) and let the user try again.
"""
from lgtv_easy.config import Config, Device
from lgtv_easy.discovery import Discovered
from lgtv_easy.mock_tv import MockTV
from lgtv_easy.netdiag import (env_summary, mac_for_ip, same_subnet_guess,
                               tcp_probe)
from lgtv_easy.webos import WebOSClient, pair_with_fallback
from lgtv_easy.wizard_text import run_text_wizard


def test_env_summary_has_key_fields():
    text = "\n".join(env_summary())
    assert "App version" in text
    assert "Python" in text
    assert "This PC IPs" in text


def test_same_subnet_guess():
    assert "same network" in same_subnet_guess(["192.168.1.10"], "192.168.1.50")
    assert "WARNING" in same_subnet_guess(["192.168.1.10"], "10.0.0.5")
    assert same_subnet_guess([], "") == ""


def test_same_subnet_guess_flags_google_wifi_double_nat():
    # PC upstream, TV behind Google/Nest Wifi (192.168.86.x).
    note = same_subnet_guess(["192.168.0.104"], "192.168.86.43")
    assert "WARNING" in note
    assert "Google" in note and "192.168.86.x" in note


def test_tcp_probe_unreachable_is_graceful():
    ok, detail = tcp_probe("203.0.113.5", 3000, timeout=0.5)
    assert ok is False
    assert detail  # a human-readable reason, never an exception


def test_pair_with_fallback_succeeds_on_first_attempt():
    with MockTV(require_pairing=False) as tv:
        client = WebOSClient("127.0.0.1")
        client._url = lambda: tv.url
        logs = []
        key = pair_with_fallback(client, log=logs.append)
        assert key == tv.known_key
        assert any("Attempting plain ws" in line for line in logs)
        client.close()


def test_wizard_emits_diagnostics_then_retries_to_success(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path))
    with MockTV(require_pairing=True) as tv:
        answers = iter([
            "n",              # don't scan
            "203.0.113.5",    # unreachable IP -> pairing fails
            "Bad TV",         # name
            "y",              # retry?
            "n",              # don't scan
            "127.0.0.1",      # good IP (the mock)
            "Good TV",        # name
            "7",              # screen-off minutes
            "",               # energy: don't mute
            "",               # energy: don't fully power off
            "n",              # don't start at login
        ])
        transcript = []

        def client_factory(ip):
            c = WebOSClient(ip, timeout=0.5)
            if ip == "127.0.0.1":
                c._url = lambda: tv.url
            return c

        rc = run_text_wizard(
            input_fn=lambda prompt: next(answers),
            output_fn=transcript.append,
            client_factory=client_factory,
            config=Config(),
        )
        joined = "\n".join(transcript)
        assert rc == 0, joined
        assert "Connection diagnostics" in joined
        assert "All set!" in joined

        saved = Config.load()
        assert saved.device.ip == "127.0.0.1"
        assert saved.setup_complete is True


def test_mac_for_ip_is_graceful_on_loopback():
    # Loopback has no ARP entry; must return "" rather than raise.
    assert mac_for_ip("127.0.0.1") == ""


def test_friendly_name_rejects_non_http_and_foreign_hosts():
    from lgtv_easy.discovery import _friendly_name
    # file:// and other schemes must never be fetched (SSRF / local file read).
    assert _friendly_name("file:///etc/passwd") is None
    assert _friendly_name("ftp://192.0.2.10/x") is None
    # An http URL whose host isn't the device that answered is refused.
    assert _friendly_name("http://10.0.0.1/desc.xml", expected_ip="192.0.2.10") is None


def test_wizard_saves_energy_options(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path))
    with MockTV(require_pairing=True) as tv:
        # n scan, IP, name, 3-min screen off, mute=y, deep=y, deep=20 min, autostart=n
        answers = iter(["n", "127.0.0.1", "TV", "3", "y", "y", "20", "n"])

        def cf(ip):
            c = WebOSClient(ip)
            c._url = lambda: tv.url
            return c

        rc = run_text_wizard(input_fn=lambda p: next(answers),
                             output_fn=lambda m: None, client_factory=cf,
                             config=Config())
        assert rc == 0
        saved = Config.load()
        assert saved.idle_minutes == 3
        assert saved.mute_on_sleep is True
        assert saved.deep_off_enabled is True
        assert saved.deep_off_minutes == 20


def test_wizard_settings_mode_skips_pairing(tmp_path, monkeypatch):
    """An already-paired config can change settings without re-pairing."""
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path))
    cfg = Config()
    cfg.setup_complete = True
    cfg.device = Device(name="Lounge", ip="192.0.2.7", key="KEY", mac="")

    # keep TV? y; 9 min; mute n; deep n; autostart n
    answers = iter(["y", "9", "n", "n", "n"])
    transcript = []

    def boom(ip):  # pairing must NOT be attempted in settings mode
        raise AssertionError("settings mode should not pair")

    rc = run_text_wizard(input_fn=lambda p: next(answers),
                         output_fn=transcript.append, client_factory=boom,
                         config=cfg)
    assert rc == 0
    saved = Config.load()
    assert saved.device.ip == "192.0.2.7"  # unchanged
    assert saved.idle_minutes == 9
    assert "Already set up" in "\n".join(transcript)


def test_wizard_cancels_cleanly_when_user_declines_retry(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    answers = iter([
        "n",              # don't scan
        "203.0.113.5",    # unreachable IP
        "Bad TV",         # name
        "n",              # decline retry
    ])
    transcript = []
    rc = run_text_wizard(
        input_fn=lambda prompt: next(answers),
        output_fn=transcript.append,
        client_factory=lambda ip: WebOSClient(ip, timeout=0.5),
        config=Config(),
    )
    assert rc == 1
    assert "Setup not completed" in "\n".join(transcript)
