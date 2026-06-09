"""Auto-start at login: enable creates an entry, disable removes it."""
from lgtv_easy import autostart


def test_enable_disable_roundtrip_linux(tmp_path, monkeypatch):
    # Force the Linux autostart location into a temp dir so the test is hermetic.
    monkeypatch.setattr(autostart.os, "name", "posix")
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path))

    assert autostart.is_enabled() is False
    autostart.enable()
    assert autostart.is_enabled() is True
    body = autostart._linux_target().read_text(encoding="utf-8")
    assert "Desktop Entry" in body
    assert "lgtv_easy run" in body

    assert autostart.disable() is True
    assert autostart.is_enabled() is False
    # Disabling again is a harmless no-op.
    assert autostart.disable() is False


def test_set_enabled_reports_status(tmp_path, monkeypatch):
    monkeypatch.setattr(autostart.os, "name", "posix")
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path))
    msg_on = autostart.set_enabled(True)
    assert "ENABLED" in msg_on
    assert autostart.is_enabled() is True
    msg_off = autostart.set_enabled(False)
    assert "DISABLED" in msg_off
    assert autostart.is_enabled() is False


def test_task_method_creates_logon_task(tmp_path, monkeypatch):
    # Exercise the Scheduled Task path directly (faking os.name='nt' would make
    # pathlib build WindowsPath, which can't instantiate on Linux). Stub schtasks.
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    calls = []

    def fake_run(args):
        calls.append(args)
        return (0, "")

    monkeypatch.setattr(autostart, "_run", fake_run)

    label = autostart._enable_task()
    assert "Scheduled Task" in label

    # The wrapper .cmd the task points at must exist and run the daemon.
    wrapper = autostart._task_wrapper_path()
    assert wrapper.exists()
    assert "lgtv_easy run" in wrapper.read_text(encoding="utf-8")

    # schtasks was asked to create a logon-triggered task.
    create = [c for c in calls if "/Create" in c]
    assert create, "schtasks /Create should have been called"
    assert "ONLOGON" in create[0]
    assert autostart.TASK_NAME in create[0]


def test_task_create_args_quotes_the_path():
    from pathlib import PurePath
    args = autostart._task_create_args(PurePath("/tmp/a b/run.cmd"))
    tr = args[args.index("/TR") + 1]
    assert tr.startswith('"') and tr.endswith('"')  # quoted for spaces


def test_windows_run_cmd_content_uses_module_run():
    body = autostart._windows_run_cmd_content()
    assert "-m lgtv_easy run" in body
    assert body.lower().startswith("@echo off")
