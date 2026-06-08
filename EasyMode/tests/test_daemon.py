"""Daemon behaviour: it sleeps the screen on idle and wakes it on activity."""
import logging

from lgtv_easy.config import Config, Device
from lgtv_easy.daemon import STATE_OFF, STATE_ON, Daemon
from lgtv_easy.mock_tv import MockTV
from lgtv_easy.webos import WebOSClient


def _quiet_logger():
    lg = logging.getLogger("test-daemon")
    lg.addHandler(logging.NullHandler())
    return lg


def _make(tv: MockTV, cfg: Config) -> Daemon:
    def factory():
        c = WebOSClient("127.0.0.1")
        c._url = lambda: tv.url
        return c

    idle_box = {"v": 0.0}
    d = Daemon(cfg, client_factory=factory,
               idle_fn=lambda: idle_box["v"], logger=_quiet_logger())
    d._idle_box = idle_box  # for the test to drive idle time
    return d


def _cfg(minutes=7.0, enabled=True, mute=False) -> Config:
    cfg = Config(idle_minutes=minutes, idle_enabled=enabled, mute_on_sleep=mute)
    cfg.device = Device(name="t", ip="127.0.0.1", key="MOCK-KEY-0001")
    return cfg


def test_sleeps_after_threshold_and_wakes_on_activity():
    with MockTV(require_pairing=False) as tv:
        d = _make(tv, _cfg(minutes=7.0))

        d._idle_box["v"] = 6 * 60  # under threshold
        d.tick()
        assert d.screen_state == STATE_ON
        assert tv.screen_on is True

        d._idle_box["v"] = 7 * 60 + 1  # crossed 7 minutes
        d.tick()
        assert d.screen_state == STATE_OFF
        assert tv.screen_on is False
        assert d.sleeps == 1

        d._idle_box["v"] = 0  # user moved the mouse
        d.tick()
        assert d.screen_state == STATE_ON
        assert tv.screen_on is True
        assert d.wakes == 1


def test_does_not_resleep_while_already_off():
    with MockTV(require_pairing=False) as tv:
        d = _make(tv, _cfg(minutes=1.0))
        d._idle_box["v"] = 120
        d.tick()
        d.tick()
        d.tick()
        assert d.sleeps == 1, "should only issue screen-off once per idle period"


def test_mute_on_sleep():
    with MockTV(require_pairing=False) as tv:
        d = _make(tv, _cfg(minutes=1.0, mute=True))
        d._idle_box["v"] = 120
        d.tick()
        assert tv.muted is True
        d._idle_box["v"] = 0
        d.tick()
        assert tv.muted is False


def test_disabled_never_sleeps():
    with MockTV(require_pairing=False) as tv:
        d = _make(tv, _cfg(minutes=1.0, enabled=False))
        d._idle_box["v"] = 9999
        d.tick()
        assert d.screen_state == STATE_ON
        assert tv.screen_on is True
        assert d.sleeps == 0


def test_disabling_after_sleep_restores_screen():
    with MockTV(require_pairing=False) as tv:
        cfg = _cfg(minutes=1.0)
        d = _make(tv, cfg)
        d._idle_box["v"] = 120
        d.tick()
        assert tv.screen_on is False
        # User flips the master switch off while the screen is asleep.
        cfg.idle_enabled = False
        d.tick()
        assert tv.screen_on is True


def test_survives_tv_disconnect():
    tv = MockTV(require_pairing=False).start()
    d = _make(tv, _cfg(minutes=1.0))
    d._idle_box["v"] = 120
    d.tick()
    assert tv.screen_on is False
    tv.stop()  # TV goes away
    d._drop_client()  # drop the cached socket so the next tick must reconnect
    d._idle_box["v"] = 0
    d.tick()  # must not raise even though the TV is unreachable
    assert d.last_error  # error recorded, loop alive
    assert d.screen_state == STATE_OFF  # could not wake; state left unchanged
