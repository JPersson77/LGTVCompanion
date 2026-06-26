"""Find-the-TV-by-MAC recovery: when DHCP moves the TV to a new IP, the saved
address goes dead but the MAC is forever, so Easy Mode looks the TV up by MAC and
rewrites the IP - automatically in the daemon, and on demand via `lgtv-easy find`.
"""
import logging

import pytest

from lgtv_easy import cli, discovery, netdiag, recovery
from lgtv_easy.config import Config, Device
from lgtv_easy.daemon import Daemon
from lgtv_easy.discovery import Discovered
from lgtv_easy.mock_tv import MockTV
from lgtv_easy.webos import WebOSClient


def _quiet():
    lg = logging.getLogger("test-relocate")
    lg.addHandler(logging.NullHandler())
    return lg


class _Fake:
    def __init__(self, out):
        self.stdout = out


# ----- netdiag: ARP table parsing + reverse lookup ----------------------------

def test_arp_table_parses_and_reverse_looks_up(monkeypatch):
    sample = (
        "192.168.86.43 dev eno1 lladdr b8:16:5f:72:64:c6 REACHABLE\n"
        "192.168.86.1 dev eno1 lladdr a0:b1:c2:d3:e4:f5 STALE\n"
        "192.168.86.99 dev eno1  FAILED\n"          # incomplete: no MAC -> skipped
    )
    monkeypatch.setattr(netdiag.subprocess, "run",
                        lambda *a, **k: _Fake(sample))
    table = netdiag.arp_table()
    assert ("192.168.86.43", "B8:16:5F:72:64:C6") in table
    assert all("FAILED" not in ip for ip, _ in table)
    # Accepts any spelling of the MAC and is case-insensitive.
    assert netdiag.ip_for_mac("b8-16-5f-72-64-c6") == "192.168.86.43"
    assert netdiag.ip_for_mac("00:00:00:00:00:01") == ""


def test_find_ip_by_mac_uses_cache_before_sweeping(monkeypatch):
    monkeypatch.setattr(netdiag, "ip_for_mac",
                        lambda mac, timeout=4.0: "192.168.86.7")

    def boom(settle=1.5):
        raise AssertionError("must not sweep when the MAC is already cached")

    monkeypatch.setattr(netdiag, "sweep_arp", boom)
    assert netdiag.find_ip_by_mac("aa:bb:cc:dd:ee:ff") == "192.168.86.7"


def test_find_ip_by_mac_sweeps_when_not_cached(monkeypatch):
    swept = {"v": False}
    answers = iter(["", "192.168.86.55"])  # miss, then a hit after the sweep
    monkeypatch.setattr(netdiag, "ip_for_mac",
                        lambda mac, timeout=4.0: next(answers))
    monkeypatch.setattr(netdiag, "sweep_arp",
                        lambda settle=1.5: swept.__setitem__("v", True))
    assert netdiag.find_ip_by_mac("B8:16:5F:72:64:C6") == "192.168.86.55"
    assert swept["v"] is True


# ----- discovery.locate_by_mac: layered, cheapest-first -----------------------

def test_locate_by_mac_prefers_arp_cache(monkeypatch):
    monkeypatch.setattr(netdiag, "ip_for_mac",
                        lambda mac, timeout=4.0: "192.168.86.9")
    monkeypatch.setattr(discovery, "discover",
                        lambda **k: (_ for _ in ()).throw(
                            AssertionError("should not discover when cached")))
    assert discovery.locate_by_mac("B8:16:5F:72:64:C6") == "192.168.86.9"


def test_locate_by_mac_via_ssdp_match(monkeypatch):
    monkeypatch.setattr(netdiag, "ip_for_mac", lambda mac, timeout=4.0: "")
    monkeypatch.setattr(discovery, "discover",
                        lambda timeout=3.0, log=None: [Discovered(ip="192.168.86.20"),
                                                       Discovered(ip="192.168.86.21")])
    macs = {"192.168.86.20": "AA:AA:AA:AA:AA:AA",
            "192.168.86.21": "B8:16:5F:72:64:C6"}
    monkeypatch.setattr(netdiag, "mac_for_ip",
                        lambda ip, timeout=4.0: macs.get(ip, ""))
    assert discovery.locate_by_mac("b8:16:5f:72:64:c6") == "192.168.86.21"


def test_locate_by_mac_falls_back_to_sweep(monkeypatch):
    monkeypatch.setattr(netdiag, "ip_for_mac", lambda mac, timeout=4.0: "")
    monkeypatch.setattr(discovery, "discover", lambda timeout=3.0, log=None: [])
    monkeypatch.setattr(netdiag, "find_ip_by_mac",
                        lambda mac, settle=1.5: "192.168.86.33")
    assert discovery.locate_by_mac("B8:16:5F:72:64:C6") == "192.168.86.33"


def test_locate_by_mac_returns_none_when_absent(monkeypatch):
    monkeypatch.setattr(netdiag, "ip_for_mac", lambda mac, timeout=4.0: "")
    monkeypatch.setattr(discovery, "discover", lambda timeout=3.0, log=None: [])
    monkeypatch.setattr(netdiag, "find_ip_by_mac", lambda mac, settle=1.5: "")
    assert discovery.locate_by_mac("B8:16:5F:72:64:C6") is None


# ----- daemon: auto-relocate when the saved IP stops answering ----------------

def test_daemon_relocates_to_new_ip_and_reconnects(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    with MockTV(require_pairing=False) as tv:
        cfg = Config(idle_minutes=1.0)
        cfg.device = Device(name="t", ip="10.0.0.5",
                            mac="B8:16:5F:72:64:C6", key="MOCK-KEY-0001")

        def factory():
            # The old address is dead; only the relocated IP reaches the TV.
            if cfg.device.ip != "127.0.0.1":
                raise OSError("No route to host")
            c = WebOSClient("127.0.0.1")
            c._url = lambda: tv.url
            return c

        d = Daemon(cfg, client_factory=factory,
                   locator_fn=lambda mac: "127.0.0.1",
                   idle_fn=lambda: 0.0, logger=_quiet())

        # A user-facing wake forces a connect: it fails at 10.0.0.5, finds the TV
        # by MAC, rewrites the IP, and reconnects - all without user action.
        assert d.wake_screen() is True
        assert cfg.device.ip == "127.0.0.1"
        assert d.relocations == 1
        assert tv.screen_on is True
        # The corrected address is persisted so it survives a restart.
        assert Config.load().device.ip == "127.0.0.1"


def test_relocate_is_eager_but_cooldown_throttles_background(monkeypatch):
    cfg = Config()
    cfg.device = Device(mac="B8:16:5F:72:64:C6", ip="10.0.0.5")
    monkeypatch.setattr(cfg, "save", lambda *a, **k: None)
    calls = []
    clock = {"t": 1000.0}
    d = Daemon(cfg, locator_fn=lambda mac: (calls.append(mac) or "127.0.0.1"),
               idle_fn=lambda: 0.0, clock_fn=lambda: clock["t"], logger=_quiet())

    # Eager: relocates on the very first failure (no "wait for a 2nd failure").
    assert d._relocate(force=False) is True
    assert calls == ["B8:16:5F:72:64:C6"]
    assert cfg.device.ip == "127.0.0.1"

    # Cooldown: an immediate background retry is throttled - no second search.
    cfg.device.ip = "10.0.0.5"
    assert d._relocate(force=False) is False
    assert calls == ["B8:16:5F:72:64:C6"]

    # A user-facing wake (force) bypasses the cooldown and searches again now.
    assert d._relocate(force=True) is True
    assert calls == ["B8:16:5F:72:64:C6", "B8:16:5F:72:64:C6"]
    assert cfg.device.ip == "127.0.0.1"


def test_relocate_noop_when_ip_unchanged():
    cfg = Config()
    cfg.device = Device(mac="B8:16:5F:72:64:C6", ip="192.168.86.43")
    d = Daemon(cfg, locator_fn=lambda mac: "192.168.86.43",
               idle_fn=lambda: 0.0, logger=_quiet())
    assert d._relocate(force=True) is False
    assert d.relocations == 0


def test_daemon_relocates_without_a_stored_mac(tmp_path, monkeypatch):
    # Fail-safe even before a MAC is known: adopt the TV the locator discovers,
    # then the connect learns and stores the MAC for next time.
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    with MockTV(require_pairing=False) as tv:
        cfg = Config(idle_minutes=1.0)
        cfg.device = Device(name="t", ip="10.0.0.5", mac="", key="MOCK-KEY-0001")

        def factory():
            if cfg.device.ip != "127.0.0.1":
                raise OSError("No route to host")
            c = WebOSClient("127.0.0.1")
            c._url = lambda: tv.url
            return c

        d = Daemon(cfg, client_factory=factory,
                   locator_fn=lambda mac: "127.0.0.1",  # stands in for SSDP discovery
                   idle_fn=lambda: 0.0, logger=_quiet())
        assert d.wake_screen() is True
        assert cfg.device.ip == "127.0.0.1"
        assert d.relocations == 1


# ----- discovery.locate_tv: MAC when known, else a single unambiguous LG ------

def test_locate_tv_uses_mac_when_known(monkeypatch):
    monkeypatch.setattr(discovery, "locate_by_mac",
                        lambda mac, timeout=3.0, log=None: "192.168.86.5")
    assert discovery.locate_tv("B8:16:5F:72:64:C6") == "192.168.86.5"


def test_locate_tv_adopts_single_lg_without_mac(monkeypatch):
    monkeypatch.setattr(
        discovery, "discover",
        lambda timeout=3.0, log=None: [Discovered(ip="192.168.86.20", is_lg=False),
                                       Discovered(ip="192.168.86.21", is_lg=True)])
    assert discovery.locate_tv("") == "192.168.86.21"


def test_locate_tv_refuses_to_guess_between_two_lgs(monkeypatch):
    monkeypatch.setattr(
        discovery, "discover",
        lambda timeout=3.0, log=None: [Discovered(ip="1.1.1.1", is_lg=True),
                                       Discovered(ip="2.2.2.2", is_lg=True)])
    assert discovery.locate_tv("") is None


def test_locate_tv_none_when_no_lg_present(monkeypatch):
    monkeypatch.setattr(
        discovery, "discover",
        lambda timeout=3.0, log=None: [Discovered(ip="3.3.3.3", is_lg=False)])
    monkeypatch.setattr(netdiag, "webos_hosts", lambda *a, **k: [])
    assert discovery.locate_tv("") is None


def test_locate_tv_mesh_fallback_probes_webos_ports(monkeypatch):
    # SSDP silent (a mesh that blocks multicast): fall back to port-probing and
    # adopt the only WebOS-speaking host.
    monkeypatch.setattr(discovery, "discover", lambda timeout=3.0, log=None: [])
    monkeypatch.setattr(netdiag, "webos_hosts", lambda *a, **k: ["192.168.86.33"])
    assert discovery.locate_tv("") == "192.168.86.33"


def test_locate_tv_mesh_fallback_refuses_when_ambiguous(monkeypatch):
    monkeypatch.setattr(discovery, "discover", lambda timeout=3.0, log=None: [])
    monkeypatch.setattr(netdiag, "webos_hosts",
                        lambda *a, **k: ["192.168.86.33", "192.168.86.40"])
    assert discovery.locate_tv("") is None


def test_discover_tvs_keeps_ssdp_results_without_probing(monkeypatch):
    monkeypatch.setattr(
        discovery, "discover",
        lambda timeout=3.0, log=None: [Discovered(ip="192.168.1.5", name="LG", is_lg=True)])
    monkeypatch.setattr(  # SSDP already found an LG TV -> must not port-probe
        netdiag, "webos_hosts",
        lambda *a, **k: (_ for _ in ()).throw(AssertionError("should not port-probe")))
    out = discovery.discover_tvs()
    assert [d.ip for d in out] == ["192.168.1.5"]


def test_discover_tvs_falls_back_to_port_probe_on_mesh(monkeypatch):
    monkeypatch.setattr(discovery, "discover", lambda timeout=3.0, log=None: [])
    monkeypatch.setattr(netdiag, "webos_hosts", lambda *a, **k: ["192.168.86.33"])
    out = discovery.discover_tvs()
    assert [d.ip for d in out] == ["192.168.86.33"]
    assert out[0].is_lg is True


def test_discover_tvs_merges_probe_without_duplicates(monkeypatch):
    # SSDP saw a non-LG responder; the port-probe adds the real TV, no dupes.
    monkeypatch.setattr(
        discovery, "discover",
        lambda timeout=3.0, log=None: [Discovered(ip="192.168.86.20", is_lg=False)])
    monkeypatch.setattr(netdiag, "webos_hosts",
                        lambda *a, **k: ["192.168.86.20", "192.168.86.33"])
    assert [d.ip for d in discovery.discover_tvs()] == ["192.168.86.20", "192.168.86.33"]


def test_webos_hosts_finds_open_control_ports(monkeypatch):
    monkeypatch.setattr(netdiag, "sweep_arp", lambda settle=1.5: None)
    monkeypatch.setattr(netdiag, "arp_table",
                        lambda timeout=4.0: [("192.168.86.33", "B8:16:5F:72:64:C6"),
                                             ("192.168.86.1", "A0:B1:C2:D3:E4:F5")])
    # Only the TV answers on a WebOS port.
    monkeypatch.setattr(
        netdiag, "tcp_probe",
        lambda ip, port, timeout=2.0: (ip == "192.168.86.33", "x"))
    assert netdiag.webos_hosts() == ["192.168.86.33"]


# ----- recovery.connect_tv: the app's on-demand self-heal --------------------

class _FakeClient:
    def __init__(self, ip, secure=False, timeout=10.0):
        self.ip = ip
        self.secure = secure
        self.connected = True
        self.closed = False

    def close(self):
        self.closed = True


def test_connect_tv_returns_client_when_saved_ip_works(monkeypatch):
    cfg = Config()
    cfg.device = Device(ip="1.2.3.4", key="k")
    monkeypatch.setattr(recovery, "WebOSClient", _FakeClient)
    monkeypatch.setattr(recovery, "pair_with_fallback", lambda client, **kw: "k")
    client = recovery.connect_tv(cfg)
    assert isinstance(client, _FakeClient) and client.ip == "1.2.3.4"


def test_connect_tv_relocates_when_ip_moved(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    cfg = Config()
    cfg.device = Device(ip="10.0.0.5", mac="B8:16:5F:72:64:C6", key="k")
    cfg.save()
    attempts = []

    def fake_pair(client, **kw):
        attempts.append(client.ip)
        if client.ip != "127.0.0.1":
            raise OSError("No route to host")
        return "k"

    monkeypatch.setattr(recovery, "WebOSClient", _FakeClient)
    monkeypatch.setattr(recovery, "pair_with_fallback", fake_pair)
    monkeypatch.setattr(discovery, "locate_tv",
                        lambda mac, timeout=3.0, log=None: "127.0.0.1")

    client = recovery.connect_tv(cfg)
    assert isinstance(client, _FakeClient) and client.ip == "127.0.0.1"
    assert attempts == ["10.0.0.5", "127.0.0.1"]   # tried saved IP, then the new one
    assert cfg.device.ip == "127.0.0.1"
    assert Config.load().device.ip == "127.0.0.1"  # and persisted the correction


def test_connect_tv_without_recover_raises(monkeypatch):
    cfg = Config()
    cfg.device = Device(ip="10.0.0.5", key="k")
    monkeypatch.setattr(recovery, "WebOSClient", _FakeClient)
    monkeypatch.setattr(
        recovery, "pair_with_fallback",
        lambda client, **kw: (_ for _ in ()).throw(OSError("down")))
    with pytest.raises(OSError):
        recovery.connect_tv(cfg, recover=False)


# ----- CLI: `lgtv-easy find` --------------------------------------------------

class _Args:
    pass


def test_cmd_find_updates_saved_ip(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    cfg = Config()
    cfg.device = Device(name="t", ip="10.0.0.5", mac="B8:16:5F:72:64:C6")
    cfg.save()
    monkeypatch.setattr(discovery, "locate_by_mac",
                        lambda mac, log=None: "192.168.86.77")
    assert cli.cmd_find(_Args()) == 0
    assert Config.load().device.ip == "192.168.86.77"


def test_cmd_find_without_mac_explains(tmp_path, monkeypatch):
    monkeypatch.setenv("LGTV_EASY_HOME", str(tmp_path))
    cfg = Config()
    cfg.device = Device(name="t", ip="10.0.0.5", mac="")
    cfg.save()
    assert cli.cmd_find(_Args()) == 1
