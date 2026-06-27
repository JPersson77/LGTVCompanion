"""Hot-reload: a settings change reaches the running daemon without a restart.

When a separate daemon process owns the watcher, the GUI/CLI save the config
and send SIGHUP; the daemon re-reads the file and applies it on the next loop
pass. These tests cover the reload itself (new timeouts applied, learned TV
identity preserved), the interruptible poll-sleep that makes it prompt, and the
single-instance signal helper that delivers the nudge.
"""
import logging
import signal
import time

import pytest

from lgtv_easy.config import Config, Device
from lgtv_easy.daemon import Daemon
from lgtv_easy.singleton import SingleInstance


def _quiet():
    lg = logging.getLogger("test-reload")
    lg.addHandler(logging.NullHandler())
    return lg


def _boom():
    # A client factory that fails fast, so run()'s startup connect never touches
    # the real network in tests.
    raise OSError("no TV in test")


def test_reload_picks_up_new_timeout(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    Config(idle_minutes=7.0, idle_enabled=True).save()
    d = Daemon(Config.load(), logger=_quiet())

    # The user moves the slider: the file now holds new values.
    nc = Config.load()
    nc.idle_minutes = 20.0
    nc.idle_enabled = False
    nc.save()

    d.reload_config()

    assert d.config.idle_minutes == 20.0
    assert d.config.idle_enabled is False
    assert d.config.idle_seconds == 20.0 * 60


def test_reload_preserves_learned_device_identity(tmp_path, monkeypatch):
    """A settings save (which carries no MAC) must not wipe a MAC the daemon
    learned at runtime - otherwise Wake-on-LAN would silently break."""
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    base = Config(idle_minutes=7.0)
    base.device = Device(name="TV", ip="192.168.1.5", key="K", mac="")
    base.save()

    cfg = Config.load()
    cfg.device.mac = "AA:BB:CC:DD:EE:FF"  # discovered live, not yet on disk
    d = Daemon(cfg, logger=_quiet())

    # GUI rewrites the file for a timeout change, still without the MAC.
    disk = Config.load()
    disk.idle_minutes = 15.0
    disk.save()

    d.reload_config()

    assert d.config.idle_minutes == 15.0
    assert d.config.device.mac == "AA:BB:CC:DD:EE:FF"  # preserved, not clobbered
    assert d.config.device.ip == "192.168.1.5"
    assert d.config.device.key == "K"


def test_run_loop_applies_a_requested_reload(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    Config(idle_minutes=7.0, idle_enabled=True).save()
    d = Daemon(Config.load(), client_factory=_boom,
               idle_fn=lambda: 0.0, locator_fn=lambda mac: None, logger=_quiet())

    step = {"n": 0}

    def fake_sleep(_seconds):
        step["n"] += 1
        if step["n"] == 1:
            nc = Config.load()
            nc.idle_minutes = 30.0
            nc.save()
            d.request_reload()  # what the SIGHUP handler does
        else:
            d._stop.set()       # exit on the second pass

    d._sleep_fn = fake_sleep
    d.run()  # blocks until stopped

    assert d.config.idle_minutes == 30.0


def test_interruptible_sleep_returns_immediately_when_nudged():
    d = Daemon(Config(), logger=_quiet())
    d.nudge()  # a settings change arrived while we were about to sleep
    t0 = time.monotonic()
    d._interruptible_sleep(5.0)
    assert time.monotonic() - t0 < 1.0
    assert not d._wake.is_set()  # the wakeup was consumed, not left latched


@pytest.mark.skipif(not hasattr(signal, "SIGHUP"),
                    reason="SIGHUP is POSIX-only; on Windows the reload nudge "
                           "uses a different mechanism and cli.py skips the "
                           "SIGHUP wiring (guarded by hasattr).")
def test_sighup_handler_requests_reload(tmp_path, monkeypatch):
    """The production signal wiring: an actual SIGHUP to this process flips the
    daemon's reload flag - no restart, no TV action."""
    import os

    from lgtv_easy.cli import _install_shutdown_hooks

    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    d = Daemon(Config(), logger=_quiet())

    sigs = [signal.SIGHUP, signal.SIGTERM, signal.SIGUSR1]
    saved = {s: signal.getsignal(s) for s in sigs}
    try:
        _install_shutdown_hooks(Config(), d, _quiet())
        assert d._reload_pending is False
        os.kill(os.getpid(), signal.SIGHUP)
        time.sleep(0.05)  # let the handler run
        assert d._reload_pending is True
    finally:
        for s, h in saved.items():
            signal.signal(s, h)


def test_singleinstance_signal_skips_self_and_empty(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    si = SingleInstance("daemon")
    # The self/empty-skip logic returns before ever calling os.kill, so the
    # signal value is immaterial here - use SIGTERM (exists on every platform)
    # rather than SIGHUP so this coverage also runs on Windows.
    assert si.signal(signal.SIGTERM) is False  # nobody holds it yet
    assert si.acquire() is True                # now we hold it (our PID)
    assert si.signal(signal.SIGTERM) is False  # never signal ourselves
    si.release()
