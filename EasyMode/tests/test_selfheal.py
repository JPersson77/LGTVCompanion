"""Tests for the self-test / repair engine (``selfheal``).

These cover the exact situation the user reported: "Test my TV" fails with a bare
``[Errno 113] No route to host`` and nothing tries to fix it. The engine must
escalate - probe, relocate, reconnect, persist - and, whatever happens, end with a
clear human summary instead of a dead end.
"""
from lgtv_easy import cli, discovery, netdiag, recovery, selfheal
from lgtv_easy.config import Config, Device
from lgtv_easy.mock_tv import MockTV
from lgtv_easy.webos import PairingError


class _Args:
    pass


# ----- quick_health_check: cheap reachability probe ---------------------------

def test_quick_health_check_true_when_a_port_is_open(monkeypatch):
    monkeypatch.setattr(netdiag, "tcp_probe",
                        lambda ip, port, timeout=2.0: (port == 3000, "x"))
    cfg = Config()
    cfg.device = Device(ip="192.168.1.50")
    assert selfheal.quick_health_check(cfg) is True


def test_quick_health_check_false_when_unreachable(monkeypatch):
    monkeypatch.setattr(netdiag, "tcp_probe", lambda ip, port, timeout=2.0: (False, "x"))
    cfg = Config()
    cfg.device = Device(ip="10.0.0.5")
    assert selfheal.quick_health_check(cfg) is False


def test_quick_health_check_false_without_an_ip():
    assert selfheal.quick_health_check(Config()) is False


def test_quick_health_check_honours_explicit_port(monkeypatch):
    probed = []
    monkeypatch.setattr(netdiag, "tcp_probe",
                        lambda ip, port, timeout=2.0: (probed.append((ip, port)), (False, "x"))[1])
    cfg = Config()
    cfg.device = Device(ip="192.168.1.50:7777")
    selfheal.quick_health_check(cfg)
    assert probed == [("192.168.1.50", 7777)]  # only the explicit port, not 3000/3001


# ----- repair: connect in place when the saved address still works ------------

def test_repair_connects_in_place_when_reachable():
    with MockTV(require_pairing=False) as tv:
        cfg = Config()
        cfg.device = Device(name="t", ip=f"127.0.0.1:{tv.port}", key=tv.known_key,
                            mac="AA:BB:CC:DD:EE:FF")
        res = selfheal.repair(cfg, connect=True)
        assert res.ok is True
        assert res.repaired is False           # nothing moved; no config change
        assert res.client is not None
        assert "responding" in res.summary.lower()
        res.client.close()


def test_repair_blinks_the_screen_to_confirm_control():
    with MockTV(require_pairing=False) as tv:
        cfg = Config()
        cfg.device = Device(ip=f"127.0.0.1:{tv.port}", key=tv.known_key,
                            mac="AA:BB:CC:DD:EE:FF")
        res = selfheal.repair(cfg, connect=False, blink=True)
        assert res.ok is True
        assert res.client is None              # connect=False closes the client
        assert any(u.endswith("turnOffScreen") for u in tv.requests)
        assert any(u.endswith("turnOnScreen") for u in tv.requests)
        assert tv.screen_on is True            # ends back on


# ----- repair: relocate to a new address and persist the fix ------------------

def test_repair_relocates_and_persists_new_ip(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    # The saved address is dead; only the relocated one (the mock) answers.
    monkeypatch.setattr(netdiag, "tcp_probe", lambda ip, port, timeout=2.0: (False, "no route"))
    with MockTV(require_pairing=False) as tv:
        monkeypatch.setattr(discovery, "locate_tv",
                            lambda mac, timeout=3.0, log=None: f"127.0.0.1:{tv.port}")
        cfg = Config()
        cfg.device = Device(name="t", ip="10.0.0.5", mac="AA:BB:CC:DD:EE:FF",
                            key=tv.known_key)
        cfg.save()

        res = selfheal.repair(cfg, connect=False)
        assert res.ok is True
        assert res.repaired is True
        assert res.old_ip == "10.0.0.5"
        assert res.new_ip == "127.0.0.1"
        assert "moved" in res.summary.lower()
        # The corrected address is saved (bare host) and survives a reload.
        assert cfg.device.ip == "127.0.0.1"
        assert Config.load().device.ip == "127.0.0.1"


# ----- repair: clear, actionable failure summaries ----------------------------

def test_repair_reports_tv_probably_off_when_nothing_found(monkeypatch):
    monkeypatch.setattr(netdiag, "local_ipv4s", lambda: ["192.168.1.10"])
    monkeypatch.setattr(netdiag, "tcp_probe", lambda ip, port, timeout=2.0: (False, "no route"))
    monkeypatch.setattr(discovery, "locate_tv", lambda mac, timeout=3.0, log=None: None)
    cfg = Config()
    cfg.device = Device(ip="192.168.1.50", mac="AA:BB:CC:DD:EE:FF")  # same subnet
    res = selfheal.repair(cfg)
    assert res.ok is False
    assert "turned off" in res.summary.lower() or "find your tv" in res.summary.lower()


def test_repair_flags_a_subnet_mismatch(monkeypatch):
    monkeypatch.setattr(netdiag, "local_ipv4s", lambda: ["192.168.1.10"])
    monkeypatch.setattr(netdiag, "tcp_probe", lambda ip, port, timeout=2.0: (False, "no route"))
    monkeypatch.setattr(discovery, "locate_tv", lambda mac, timeout=3.0, log=None: None)
    cfg = Config()
    cfg.device = Device(ip="10.0.0.5", mac="AA:BB:CC:DD:EE:FF")  # different subnet
    res = selfheal.repair(cfg)
    assert res.ok is False
    assert "different network" in res.summary.lower()


def test_repair_reports_no_network_when_pc_offline(monkeypatch):
    monkeypatch.setattr(netdiag, "local_ipv4s", lambda: [])
    monkeypatch.setattr(netdiag, "tcp_probe", lambda ip, port, timeout=2.0: (False, "no route"))
    monkeypatch.setattr(discovery, "locate_tv", lambda mac, timeout=3.0, log=None: None)
    cfg = Config()
    cfg.device = Device(ip="10.0.0.5")
    res = selfheal.repair(cfg)
    assert res.ok is False
    assert "this pc" in res.summary.lower() and "network" in res.summary.lower()


def test_repair_detects_pairing_rejection_at_a_reachable_ip(monkeypatch):
    # The saved IP answers a TCP probe but the TV refuses the registration.
    monkeypatch.setattr(netdiag, "local_ipv4s", lambda: ["1.2.3.4"])
    monkeypatch.setattr(netdiag, "tcp_probe", lambda ip, port, timeout=2.0: (True, "open"))
    monkeypatch.setattr(discovery, "locate_tv", lambda mac, timeout=3.0, log=None: None)

    def reject(client, **kw):
        raise PairingError("registration error")

    monkeypatch.setattr(selfheal, "pair_with_fallback", reject)
    cfg = Config()
    cfg.device = Device(ip="1.2.3.4", key="STALE")
    res = selfheal.repair(cfg)
    assert res.ok is False
    assert "re-run setup" in res.summary.lower() or "refused" in res.summary.lower()


def test_repair_never_raises_on_unexpected_error(monkeypatch):
    def boom(*a, **k):
        raise RuntimeError("kaboom")

    monkeypatch.setattr(netdiag, "subnet_report", boom)
    res = selfheal.repair(Config())
    assert res.ok is False
    assert res.summary  # always a human-readable message, never an exception


def test_repair_records_full_transcript():
    logs = []
    with MockTV(require_pairing=False) as tv:
        cfg = Config()
        cfg.device = Device(ip=f"127.0.0.1:{tv.port}", key=tv.known_key,
                            mac="AA:BB:CC:DD:EE:FF")
        res = selfheal.repair(cfg, connect=True, log=logs.append)
        assert res.client is not None
        res.client.close()
    # Everything the user could read in a repair session is captured both in the
    # returned steps and streamed to the log callback.
    assert res.steps == logs
    assert any("network" in line.lower() for line in res.steps)


# ----- CLI: `lgtv-easy repair` and a self-healing `test` ----------------------

def test_cmd_repair_relocates_and_persists(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    monkeypatch.setattr(netdiag, "tcp_probe", lambda ip, port, timeout=2.0: (False, "no route"))
    with MockTV(require_pairing=False) as tv:
        monkeypatch.setattr(discovery, "locate_tv",
                            lambda mac, timeout=3.0, log=None: f"127.0.0.1:{tv.port}")
        cfg = Config()
        cfg.device = Device(name="t", ip="10.0.0.5", mac="AA:BB:CC:DD:EE:FF",
                            key=tv.known_key)
        cfg.save()
        assert cli.cmd_repair(_Args()) == 0
        assert Config.load().device.ip == "127.0.0.1"


def test_cmd_repair_without_a_tv_explains(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    assert cli.cmd_repair(_Args()) == 1


def test_cmd_test_self_heals_when_saved_ip_is_dead(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))

    # The fast path can't reach the saved address...
    def dead(*a, **k):
        raise OSError("[Errno 113] No route to host")

    monkeypatch.setattr(recovery, "connect_tv", dead)
    # ...but the repair escalation relocates to the mock and blinks it.
    monkeypatch.setattr(netdiag, "tcp_probe", lambda ip, port, timeout=2.0: (False, "no route"))
    with MockTV(require_pairing=False) as tv:
        monkeypatch.setattr(discovery, "locate_tv",
                            lambda mac, timeout=3.0, log=None: f"127.0.0.1:{tv.port}")
        cfg = Config()
        cfg.device = Device(name="t", ip="10.0.0.5", mac="AA:BB:CC:DD:EE:FF",
                            key=tv.known_key)
        cfg.save()
        assert cli.cmd_test(_Args()) == 0
        assert Config.load().device.ip == "127.0.0.1"
        assert any(u.endswith("turnOnScreen") for u in tv.requests)
