"""Daemon behaviour: it sleeps the screen on idle and wakes it on activity."""
import logging

from lgtv_easy.config import Config, Device
from lgtv_easy.daemon import STATE_OFF, STATE_ON, STATE_STANDBY, Daemon
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


def test_two_stage_screen_off_then_full_power_off_then_wake():
    with MockTV(require_pairing=False) as tv:
        cfg = _cfg(minutes=5.0)
        cfg.deep_off_enabled = True
        cfg.deep_off_minutes = 10.0
        cfg.device.mac = "AA:BB:CC:DD:EE:FF"  # WOL available, so deep-off allowed
        d = _make(tv, cfg)

        d._idle_box["v"] = 4 * 60       # working: on
        d.tick()
        assert d.screen_state == STATE_ON

        d._idle_box["v"] = 6 * 60       # past 5 min: screen blanks, TV still powered
        d.tick()
        assert d.screen_state == STATE_OFF
        assert tv.screen_on is False and tv.powered_on is True

        d._idle_box["v"] = 8 * 60       # between 5 and 10: no deep-off yet
        d.tick()
        assert d.screen_state == STATE_OFF
        assert tv.powered_on is True and d.deep_offs == 0

        d._idle_box["v"] = 11 * 60      # past 10 min: full power off
        d.tick()
        assert d.screen_state == STATE_STANDBY
        assert tv.powered_on is False and d.deep_offs == 1

        d._idle_box["v"] = 0            # activity: wake back on
        d.tick()
        assert d.screen_state == STATE_ON
        assert tv.screen_on is True and d.wakes == 1


def test_deep_off_ignored_when_not_beyond_screen_off_threshold():
    with MockTV(require_pairing=False) as tv:
        cfg = _cfg(minutes=5.0)
        cfg.deep_off_enabled = True
        cfg.deep_off_minutes = 5.0  # not strictly greater: must be ignored
        d = _make(tv, cfg)
        d._idle_box["v"] = 99 * 60
        d.tick()  # screen off
        d.tick()  # would deep-off if it applied
        assert tv.powered_on is True
        assert d.deep_offs == 0


def test_deep_off_skipped_without_wol_mac():
    # Full power-off with no MAC would leave the TV unwakeable; the daemon must
    # decline and just keep the screen blanked.
    with MockTV(require_pairing=False) as tv:
        cfg = _cfg(minutes=5.0)
        cfg.deep_off_enabled = True
        cfg.deep_off_minutes = 10.0  # but cfg.device.mac is "" (none)
        d = _make(tv, cfg)
        d._idle_box["v"] = 11 * 60
        d.tick()  # screen off
        d.tick()  # would deep-off, but no MAC -> must stay screen-off
        assert tv.powered_on is True
        assert d.deep_offs == 0


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
