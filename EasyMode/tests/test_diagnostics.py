"""Tests for the wizard diagnostics, secure-port fallback and retry-on-failure.

These cover the user-reported scenario: the TV can't be found/reached, and we
need the wizard to explain why (instead of silently failing and the window
closing) and let the user try again.
"""
from lgtv_easy.config import Config
from lgtv_easy.discovery import Discovered
from lgtv_easy.mock_tv import MockTV
from lgtv_easy.netdiag import env_summary, same_subnet_guess, tcp_probe
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
    with MockTV(require_pairing=True) as tv:
        answers = iter([
            "n",              # don't scan
            "203.0.113.5",    # unreachable IP -> pairing fails
            "Bad TV",         # name
            "y",              # retry?
            "n",              # don't scan
            "127.0.0.1",      # good IP (the mock)
            "Good TV",        # name
            "7",              # minutes
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
