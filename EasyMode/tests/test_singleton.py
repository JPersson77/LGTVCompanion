"""Single-instance lock: only one daemon drives the TV at a time."""
from lgtv_easy.singleton import SingleInstance


def test_second_instance_cannot_acquire_while_held(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    a = SingleInstance("daemon")
    assert a.acquire(wait=False) is True

    # A different process (simulated via a pidfile holding a live PID) blocks us.
    b = SingleInstance("daemon")
    # b sees a's PID (same process here), which counts as "ours" -> allowed.
    # To simulate a *different* live holder, point the holder at a known-live pid.
    import os
    monkeypatch.setattr(b, "_holder", lambda: os.getppid())
    assert b.acquire(wait=False) is False

    a.release()


def test_wait_acquires_once_holder_goes_away(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    lock = SingleInstance("daemon")

    # Holder is "alive" for the first two polls, then gone.
    calls = {"n": 0}

    def fake_holder():
        calls["n"] += 1
        return 999999 if calls["n"] < 3 else None

    monkeypatch.setattr(lock, "_holder", fake_holder)
    polls = {"n": 0}
    assert lock.acquire(wait=True, poll=0, sleep_fn=lambda s: polls.__setitem__("n", polls["n"] + 1)) is True
    assert polls["n"] >= 2  # it waited rather than giving up
    lock.release()


def test_release_is_safe_when_not_held(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    lock = SingleInstance("daemon")
    lock.release()  # must not raise
