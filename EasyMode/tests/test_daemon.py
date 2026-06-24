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


def _run_with_freeze(tv, cfg, idle_value, jump=999.0):
    """Drive ``Daemon.run()`` through one simulated suspend/resume freeze.

    The fake clock jumps far ahead on the first poll-sleep (the process was
    frozen while the machine slept); the loop exits on the second pass.
    """
    def factory():
        c = WebOSClient("127.0.0.1")
        c._url = lambda: tv.url
        return c

    idle_box = {"v": idle_value}
    clock = {"t": 0.0}
    step = {"n": 0}
    d = Daemon(cfg, client_factory=factory, idle_fn=lambda: idle_box["v"],
               clock_fn=lambda: clock["t"], logger=_quiet_logger())

    def fake_sleep(_seconds):
        step["n"] += 1
        if step["n"] == 1:
            clock["t"] += jump            # huge gap => the process was frozen
        else:
            d._stop.set()                 # exit after the second pass
            clock["t"] += cfg.poll_seconds

    d._sleep_fn = fake_sleep
    d.run()  # blocks until _stop is set
    return d


def test_resume_backstop_restores_tv_after_a_freeze():
    # No OS sleep watcher fired (e.g. a non-systemd Linux box). The PC suspended
    # before the idle timeout, so our state still reads ON, and the panel went
    # dark on its own. The wall-clock backstop must notice the freeze and a
    # present user, and re-light the TV - something the idle tick alone misses.
    with MockTV(require_pairing=False) as tv:
        tv.screen_on = False              # panel self-standbyed while PC slept
        d = _run_with_freeze(tv, _cfg(minutes=99.0), idle_value=0.0)
        assert tv.screen_on is True
        assert d.screen_state == STATE_ON
        assert d.wakes >= 1


def test_resume_backstop_ignores_autonomous_wake():
    # The machine resumed on its own (RTC alarm / scheduled task); nobody is
    # there, so idle stays high. The backstop must not light an empty room.
    with MockTV(require_pairing=False) as tv:
        d = _run_with_freeze(tv, _cfg(minutes=5.0), idle_value=9999.0)
        assert tv.screen_on is False
        assert d.wakes == 0


def test_resume_backstop_off_when_pc_sleep_disabled():
    with MockTV(require_pairing=False) as tv:
        cfg = _cfg(minutes=99.0)
        cfg.screen_off_on_pc_sleep = False
        tv.screen_on = False
        d = _run_with_freeze(tv, cfg, idle_value=0.0)
        assert d.wakes == 0  # user opted out of PC-sleep handling entirely


class _RecordingLogger:
    """A logger stand-in that records messages by level, for asserting on them."""

    def __init__(self):
        self.warnings, self.infos = [], []

    @staticmethod
    def _fmt(msg, args):
        return (msg % args) if args else msg

    def warning(self, msg, *args):
        self.warnings.append(self._fmt(msg, args))

    def info(self, msg, *args):
        self.infos.append(self._fmt(msg, args))

    def debug(self, *a, **k):
        pass

    def exception(self, *a, **k):
        pass


def test_unreachable_tv_backs_off_and_warns_once():
    # The TV is off, so every connect fails. The daemon must NOT retry on every
    # poll (that floods the log and burns CPU) - it backs off exponentially and
    # logs the outage exactly once.
    cfg = _cfg(minutes=1.0)
    cfg.poll_seconds = 5.0
    attempts = {"n": 0}

    def factory():
        attempts["n"] += 1
        raise OSError("No route to host")

    clock = {"t": 0.0}
    log = _RecordingLogger()
    d = Daemon(cfg, client_factory=factory, idle_fn=lambda: 120.0,
               clock_fn=lambda: clock["t"], logger=log)

    for _ in range(100):              # 100 polls, 5s apart, TV unreachable throughout
        d.tick()
        clock["t"] += cfg.poll_seconds

    assert d.screen_state == STATE_ON         # never blanked an already-off TV
    assert len(log.warnings) == 1, "the outage must be logged once, not per poll"
    assert attempts["n"] < 15, "exponential backoff must throttle the retries"


def test_unreachable_tv_recovers_and_logs_reconnect():
    # Once the TV is reachable again, the daemon reconnects, clears the backoff,
    # and the screen-off it had been unable to do now succeeds.
    with MockTV(require_pairing=False) as tv:
        cfg = _cfg(minutes=1.0)
        up = {"v": False}

        def factory():
            if not up["v"]:
                raise OSError("No route to host")
            c = WebOSClient("127.0.0.1")
            c._url = lambda: tv.url
            return c

        clock = {"t": 0.0}
        log = _RecordingLogger()
        d = Daemon(cfg, client_factory=factory, idle_fn=lambda: 120.0,
                   clock_fn=lambda: clock["t"], logger=log)

        d.tick()                              # TV down: connect fails, backoff armed
        assert d.screen_state == STATE_ON
        assert d._connect_failures >= 1

        up["v"] = True                        # TV comes back
        clock["t"] += 10.0                    # advance past the backoff window
        d.tick()
        assert d.screen_state == STATE_OFF    # the blank finally went through
        assert d._connect_failures == 0
        assert any("Reconnected" in m for m in log.infos)


def test_wake_bypasses_reconnect_backoff():
    # A backoff from a failed sleep must not delay a user-facing wake: the wake
    # path forces a connection attempt even inside the backoff window.
    with MockTV(require_pairing=False) as tv:
        cfg = _cfg(minutes=1.0)
        up = {"v": False}

        def factory():
            if not up["v"]:
                raise OSError("No route to host")
            c = WebOSClient("127.0.0.1")
            c._url = lambda: tv.url
            return c

        clock = {"t": 0.0}
        d = Daemon(cfg, client_factory=factory, idle_fn=lambda: 120.0,
                   clock_fn=lambda: clock["t"], logger=_quiet_logger())
        d.screen_state = STATE_OFF            # pretend we'd blanked it earlier
        d.tick()                              # idle high, but already OFF: no-op-ish
        d._note_connect_failure(OSError("x"))  # arm a long backoff window
        far = d._next_connect_at
        up["v"] = True
        d._idle_box = {"v": 0.0}
        d._idle_fn = lambda: 0.0              # user is active -> should wake now
        clock["t"] += 1.0                     # still well inside the backoff window
        assert clock["t"] < far
        d.tick()
        assert d.screen_state == STATE_ON     # forced through the backoff
        assert d.wakes == 1


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
