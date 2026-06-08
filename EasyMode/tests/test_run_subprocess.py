"""Integration test of the supervised entry point.

Runs the real ``python -m lgtv_easy run`` in a subprocess (exactly what the
launcher's supervisor does) against an in-process MockTV, and verifies it
actually turns the screen off when idle and writes to the persistent log.
"""
import os
import subprocess
import sys
import time

from lgtv_easy.config import Config, Device
from lgtv_easy.config import log_path
from lgtv_easy.mock_tv import MockTV

PKG_PARENT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def test_run_entrypoint_sleeps_screen(tmp_path):
    with MockTV(require_pairing=False) as tv:
        # Config points the daemon at the mock TV via host:port.
        cfg = Config(idle_minutes=1.0, idle_enabled=True, poll_seconds=0.5,
                     setup_complete=True)
        cfg.device = Device(name="mock", ip=f"127.0.0.1:{tv.port}",
                            key="MOCK-KEY-0001")
        cfg.save(os.path.join(tmp_path, "config.json"))

        env = dict(os.environ)
        env["LGTV_EASY_HOME"] = str(tmp_path)
        env["LGTV_EASY_FAKE_IDLE"] = "120"  # 2 min idle -> over the 1 min limit
        env["PYTHONPATH"] = PKG_PARENT + os.pathsep + env.get("PYTHONPATH", "")

        proc = subprocess.Popen(
            [sys.executable, "-m", "lgtv_easy", "run"],
            env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            cwd=PKG_PARENT)
        try:
            # Give the daemon a few poll cycles to connect and blank the screen.
            deadline = time.time() + 10
            while time.time() < deadline and tv.screen_on:
                time.sleep(0.3)
            assert tv.screen_on is False, "daemon should have turned the screen off"
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()

        # The persistent log should exist and mention the daemon starting.
        lp = os.path.join(str(tmp_path), os.path.basename(log_path()))
        assert os.path.exists(lp), "log file should be written"
        assert "daemon started" in open(lp).read().lower()
