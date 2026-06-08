"""Headless GUI smoke test (run under Xvfb).

Constructs the real tkinter App, drives the wizard through all three steps
against a MockTV, then verifies the settings panel builds and reflects config.
Renders to an off-screen X display so no human interaction is needed.
"""
import os
import sys
import tempfile

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

os.environ["LGTV_EASY_HOME"] = tempfile.mkdtemp(prefix="lgtv-gui-")

from lgtv_easy import gui  # noqa: E402
from lgtv_easy.mock_tv import MockTV  # noqa: E402
from lgtv_easy.discovery import Discovered  # noqa: E402
from lgtv_easy.webos import WebOSClient  # noqa: E402


def pump(app, n=20):
    for _ in range(n):
        app.update_idletasks()
        app.update()


def main():
    tv = MockTV(require_pairing=True).start()

    # Point discovery and the client at the mock TV.
    gui.discover = lambda *a, **k: [Discovered(ip="127.0.0.1", name="LG B-series")]

    orig_client = gui.WebOSClient

    def patched_client(ip, *a, **k):
        c = orig_client(ip, *a, **k)
        c._url = lambda: tv.url
        return c

    gui.WebOSClient = patched_client

    app = gui.App()
    pump(app)
    assert app.cfg.setup_complete is False
    wizard = app.container.winfo_children()[0]
    assert isinstance(wizard, gui.SetupWizard), "wizard shown on first run"
    print("[gui] Wizard step 1 rendered:", wizard.winfo_children()[0].cget("text"))

    # Step 1: select discovered TV and continue.
    wizard.found = [Discovered(ip="127.0.0.1", name="LG B-series")]
    wizard.selected_ip.set("127.0.0.1")
    wizard._goto_pair()
    pump(app)
    print("[gui] Advanced to pairing; waiting for mock pairing...")

    # Wait for the pairing worker thread to finish and post back.
    for _ in range(100):
        pump(app)
        if wizard.client_key:
            break
        import time
        time.sleep(0.05)
    assert wizard.client_key == tv.known_key, "wizard obtained a client key"
    assert tv.pair_prompts == 1
    print("[gui] Paired, key:", wizard.client_key)

    # Step 3 should now be visible; set timeout and finish.
    wizard.minutes.set(7)
    pump(app)
    wizard._finish()
    pump(app)

    # Settings panel should now be showing.
    panel = app.container.winfo_children()[0]
    assert isinstance(panel, gui.SettingsPanel), "settings panel after finish"
    assert app.cfg.setup_complete and app.cfg.idle_minutes == 7
    assert app.cfg.device.ip == "127.0.0.1"
    print("[gui] Settings panel rendered. Status:",
          panel.status.cget("text"))

    # Toggle the timeout slider and the on/off switch; config should persist.
    panel.minutes.set(15)
    panel._slider_moved()
    pump(app)
    assert app.cfg.idle_minutes == 15
    panel.enabled.set(False)
    panel._apply()
    pump(app)
    assert app.cfg.idle_enabled is False
    print("[gui] Live edits persisted (minutes=15, enabled=False).")

    app.on_close()
    tv.stop()
    print("[gui] SUCCESS — GUI builds, wizard pairs, settings persist. ✓")
    return 0


if __name__ == "__main__":
    sys.exit(main())
