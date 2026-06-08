"""Config round-trip and defaults."""
import os

from lgtv_easy.config import Config, Device


def test_defaults_match_monitor_use_case():
    cfg = Config()
    assert cfg.idle_minutes == 7.0  # the user's stated target
    assert cfg.idle_enabled is True
    assert cfg.idle_seconds == 7 * 60


def test_save_and_load_roundtrip(tmp_path):
    path = os.path.join(tmp_path, "config.json")
    cfg = Config(idle_minutes=12.5, mute_on_sleep=True, setup_complete=True)
    cfg.device = Device(name="Office C2", ip="192.168.1.5",
                        mac="AA:BB:CC:DD:EE:FF", key="abc123")
    cfg.save(path)

    loaded = Config.load(path)
    assert loaded.idle_minutes == 12.5
    assert loaded.mute_on_sleep is True
    assert loaded.setup_complete is True
    assert loaded.device.name == "Office C2"
    assert loaded.device.ip == "192.168.1.5"
    assert loaded.device.paired is True


def test_load_missing_file_returns_defaults(tmp_path):
    loaded = Config.load(os.path.join(tmp_path, "nope.json"))
    assert loaded.idle_minutes == 7.0
    assert loaded.device.ip == ""


def test_load_ignores_unknown_keys(tmp_path):
    path = os.path.join(tmp_path, "config.json")
    with open(path, "w") as fh:
        fh.write('{"idle_minutes": 3, "some_future_key": 99, '
                 '"device": {"ip": "1.2.3.4", "future": 1}}')
    loaded = Config.load(path)
    assert loaded.idle_minutes == 3
    assert loaded.device.ip == "1.2.3.4"


def test_idle_seconds_has_floor():
    cfg = Config(idle_minutes=0)
    assert cfg.idle_seconds >= 1.0
