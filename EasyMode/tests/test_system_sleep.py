"""The TV follows the PC into sleep: blank on suspend, wake on resume.

These exercise the daemon's suspend/resume handlers directly (no real OS power
event needed) and check that the platform watcher factory is always safe to call.
"""
import logging

from lgtv_easy import system_sleep
from lgtv_easy.config import Config, Device
from lgtv_easy.daemon import STATE_OFF, STATE_ON, Daemon
from lgtv_easy.mock_tv import MockTV
from lgtv_easy.webos import WebOSClient


def _quiet_logger():
    lg = logging.getLogger("test-sleep")
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
    d._idle_box = idle_box
    return d


def _cfg(minutes=7.0) -> Config:
    cfg = Config(idle_minutes=minutes)
    cfg.device = Device(name="t", ip="127.0.0.1", key="MOCK-KEY-0001")
    return cfg


def test_pc_sleep_blanks_the_tv_even_before_the_idle_timeout():
    with MockTV(require_pairing=False) as tv:
        d = _make(tv, _cfg(minutes=7.0))
        d._idle_box["v"] = 0  # user is active; the idle timeout is nowhere near
        d.tick()
        assert tv.screen_on is True

        d._on_system_sleep()  # the user hits "Sleep" on the PC
        assert tv.screen_on is False
        assert d.screen_state == STATE_OFF
        assert d.sleeps == 1


def test_pc_resume_wakes_the_tv_when_the_user_is_back():
    with MockTV(require_pairing=False) as tv:
        d = _make(tv, _cfg(minutes=7.0))
        d._on_system_sleep()
        assert tv.screen_on is False

        d._idle_box["v"] = 0  # the user woke the PC with a keypress
        d._on_system_resume()
        assert tv.screen_on is True
        assert d.screen_state == STATE_ON
        assert d.wakes == 1


def test_pc_resume_without_a_user_leaves_the_tv_asleep():
    # A scheduled/automatic wake (no input) must not light the TV in an empty room.
    with MockTV(require_pairing=False) as tv:
        d = _make(tv, _cfg(minutes=7.0))
        d._on_system_sleep()
        d._idle_box["v"] = 9999  # resumed on its own; nobody touched the input
        d._on_system_resume()
        assert tv.screen_on is False
        assert d.screen_state == STATE_OFF


def test_pc_sleep_respects_the_setting():
    with MockTV(require_pairing=False) as tv:
        cfg = _cfg(minutes=7.0)
        cfg.screen_off_on_pc_sleep = False
        d = _make(tv, cfg)
        d._on_system_sleep()
        assert tv.screen_on is True
        assert d.sleeps == 0


def test_pc_sleep_is_a_noop_when_already_dark():
    with MockTV(require_pairing=False) as tv:
        d = _make(tv, _cfg(minutes=1.0))
        d._idle_box["v"] = 120
        d.tick()  # idle -> screen already off
        assert d.screen_state == STATE_OFF
        before = d.sleeps
        d._on_system_sleep()  # PC sleeps while the TV is already blanked
        assert d.sleeps == before  # no redundant screen-off issued


def test_make_watcher_returns_a_usable_object():
    # Whatever platform CI runs on, the factory must hand back something with the
    # watcher interface and never raise while doing so.
    w = system_sleep.make_watcher(on_sleep=lambda: None, on_resume=lambda: None)
    assert hasattr(w, "start") and hasattr(w, "stop")
    assert isinstance(w.backend_name, str)


def test_null_watcher_is_a_safe_noop():
    w = system_sleep._NullWatcher()
    w.start()
    w.stop()  # must not raise
    assert w.backend_name == "none"
