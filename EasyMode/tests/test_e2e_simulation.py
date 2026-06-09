"""Full simulated user journey, end to end, with no real TV or display.

Simulates exactly what the user described: first-time setup via the wizard,
then leaving the desk so the screen sleeps after the chosen timeout, then
returning so it wakes. This is the "simulated use" of the app.
"""
import logging

from lgtv_easy.config import Config
from lgtv_easy.daemon import STATE_OFF, STATE_ON, Daemon
from lgtv_easy.discovery import Discovered
from lgtv_easy.mock_tv import MockTV
from lgtv_easy.webos import WebOSClient
from lgtv_easy.wizard_text import run_text_wizard


def test_full_journey_setup_then_idle_sleep_wake(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path))  # keep autostart in tmp

    with MockTV(require_pairing=True) as tv:
        # ---- Scripted wizard interaction (as if the user were typing) ----
        answers = iter([
            "y",        # scan automatically?
            "1",        # pick the first discovered TV
            "7",        # 7 minute screen-off timeout (the user's request)
            "",         # energy step: don't mute
            "",         # energy step: don't fully power off
            "n",        # don't start automatically at login
        ])
        transcript = []

        def fake_discover(*a, **k):
            return [Discovered(ip="127.0.0.1", name="LG OLED B-series")]

        def client_factory(ip):
            c = WebOSClient(ip)
            c._url = lambda: tv.url
            return c

        cfg = Config()
        rc = run_text_wizard(
            input_fn=lambda prompt: next(answers),
            output_fn=transcript.append,
            discover_fn=fake_discover,
            client_factory=client_factory,
            config=cfg,
        )
        assert rc == 0
        assert tv.pair_prompts == 1, "wizard triggered the on-TV pairing prompt"
        joined = "\n".join(transcript)
        assert "All set!" in joined
        assert "7 minutes" in joined

        # ---- Settings were persisted ----
        saved = Config.load()
        assert saved.setup_complete is True
        assert saved.idle_minutes == 7
        assert saved.device.ip == "127.0.0.1"
        assert saved.device.paired is True

        # ---- Now simulate using the PC over time ----
        idle = {"v": 0.0}
        daemon = Daemon(
            saved,
            client_factory=lambda: client_factory(saved.device.ip),
            idle_fn=lambda: idle["v"],
            logger=logging.getLogger("e2e"),
        )

        # Working at the PC: screen stays on.
        for minutes in (1, 3, 6):
            idle["v"] = minutes * 60
            daemon.tick()
            assert daemon.screen_state == STATE_ON
            assert tv.screen_on is True

        # Stepped away past 7 minutes: screen sleeps.
        idle["v"] = 7 * 60 + 5
        daemon.tick()
        assert daemon.screen_state == STATE_OFF
        assert tv.screen_on is False

        # Came back and touched the mouse: screen wakes.
        idle["v"] = 0
        daemon.tick()
        assert daemon.screen_state == STATE_ON
        assert tv.screen_on is True

        assert daemon.sleeps == 1 and daemon.wakes == 1


def test_cli_status_after_setup(tmp_path, monkeypatch, capsys):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    from lgtv_easy import cli

    cli.main(["set", "--minutes", "7", "--enabled", "true"])
    cli.main(["status"])
    out = capsys.readouterr().out
    assert "Easy Mode" in out
    assert "7" in out
