"""The TV follows the PC into sleep: blank on suspend, wake on resume.

These exercise the daemon's suspend/resume handlers directly (no real OS power
event needed) and check that the platform watcher factory is always safe to call.
"""
import logging

from lgtv_easy import system_sleep
from lgtv_easy.config import Config, Device
from lgtv_easy.daemon import STATE_OFF, STATE_ON, STATE_STANDBY, Daemon
from lgtv_easy.mock_tv import MockTV
from lgtv_easy.webos import WebOSClient


def _quiet_logger():
    lg = logging.getLogger("test-sleep")
    lg.addHandler(logging.NullHandler())
    return lg


def _wait_until(pred, timeout=2.0):
    """Poll ``pred`` until true or timeout. The PC-sleep screen-off is sent
    fire-and-forget (it doesn't wait for the TV's reply), so the mock TV updates
    its state a beat after the call returns - give it that beat."""
    import time
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if pred():
            return True
        time.sleep(0.01)
    return pred()


def _make(tv: MockTV, cfg: Config) -> Daemon:
    def factory():
        c = WebOSClient("127.0.0.1")
        c._url = lambda: tv.url
        return c

    idle_box = {"v": 0.0}
    d = Daemon(cfg, client_factory=factory,
               idle_fn=lambda: idle_box["v"],
               locator_fn=lambda mac: None,  # keep unit tests off the network
               logger=_quiet_logger())
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
        assert _wait_until(lambda: tv.screen_on is False)
        assert d.screen_state == STATE_OFF
        assert d.sleeps == 1


def test_pc_resume_wakes_the_tv_when_the_user_is_back():
    with MockTV(require_pairing=False) as tv:
        d = _make(tv, _cfg(minutes=7.0))
        d._on_system_sleep()
        assert _wait_until(lambda: tv.screen_on is False)

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
        assert _wait_until(lambda: tv.screen_on is False)
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


def test_pc_sleep_blanks_even_after_a_reconnect_backoff():
    # Regression: the PC-sleep path used to reuse the idle reconnect backoff and
    # silently return without a log whenever the TV wasn't already connected -
    # leaving the panel lit. It must now force a fresh connection and blank.
    with MockTV(require_pairing=False) as tv:
        d = _make(tv, _cfg(minutes=7.0))
        # Pretend a prior poll failed and armed a long backoff window.
        d._connect_failures = 3
        d._next_connect_at = d._clock() + 10_000
        d._idle_box["v"] = 0
        d._on_system_sleep()
        assert _wait_until(lambda: tv.screen_on is False)
        assert d.screen_state == STATE_OFF
        assert d.sleeps == 1


def test_pc_shutdown_powers_the_tv_off():
    with MockTV(require_pairing=False) as tv:
        d = _make(tv, _cfg(minutes=7.0))
        d._on_system_shutdown()  # the OS is shutting down / logging off
        assert tv.powered_on is False
        assert d.screen_state == STATE_STANDBY
        assert d._shutdown_handled is True


def test_pc_shutdown_respects_the_setting():
    with MockTV(require_pairing=False) as tv:
        cfg = _cfg(minutes=7.0)
        cfg.tv_off_on_shutdown = False
        d = _make(tv, cfg)
        d._on_system_shutdown()
        assert tv.powered_on is True  # honoured the setting, left the TV on
        # Still marked handled, so the SIGTERM fallback also stands down.
        assert d._shutdown_handled is True


def test_make_watcher_returns_a_usable_object():
    # Whatever platform CI runs on, the factory must hand back something with the
    # watcher interface and never raise while doing so - including with the
    # shutdown hook wired in.
    w = system_sleep.make_watcher(on_sleep=lambda: None, on_resume=lambda: None,
                                  on_shutdown=lambda: None)
    assert hasattr(w, "start") and hasattr(w, "stop")
    assert isinstance(w.backend_name, str)


def test_logind_watcher_routes_sleep_resume_and_shutdown_lines():
    # Unit-test the gdbus-monitor line parsing directly (no real bus needed): the
    # PrepareForShutdown signal on the same object path must trigger the shutdown
    # hook, distinct from PrepareForSleep true/false.
    calls = []
    w = system_sleep._LogindWatcher(
        on_sleep=lambda: calls.append("sleep"),
        on_resume=lambda: calls.append("resume"),
        logger=None, gdbus="gdbus", inhibit=None,
        on_shutdown=lambda: calls.append("shutdown"))

    import io

    class _Mon:
        stdout = io.StringIO(
            "/org/freedesktop/login1: ...Manager.PrepareForSleep (true,)\n"
            "/org/freedesktop/login1: ...Manager.PrepareForSleep (false,)\n"
            "/org/freedesktop/login1: ...Manager.PrepareForShutdown (true,)\n")

    w._mon = _Mon()
    w._run()
    assert calls == ["sleep", "resume", "shutdown"]


def test_null_watcher_is_a_safe_noop():
    w = system_sleep._NullWatcher()
    w.start()
    w.stop()  # must not raise
    assert w.backend_name == "none"
