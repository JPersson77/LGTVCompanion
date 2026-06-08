"""Live, end-to-end simulation driving the *real* programs.

Unlike the unit tests (which inject fakes), this starts an actual MockTV on a
socket and runs the genuine CLI/wizard/daemon code paths against it, then prints
a human-readable transcript. Run with:  python3 tests/simulate_session.py
"""
import io
import os
import sys
import tempfile
from contextlib import redirect_stdout

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from lgtv_easy.config import Config  # noqa: E402
from lgtv_easy.daemon import Daemon  # noqa: E402
from lgtv_easy.discovery import Discovered  # noqa: E402
from lgtv_easy.mock_tv import MockTV  # noqa: E402
from lgtv_easy.webos import WebOSClient  # noqa: E402
from lgtv_easy.wizard_text import run_text_wizard  # noqa: E402


def banner(title):
    print("\n" + "#" * 64)
    print("# " + title)
    print("#" * 64)


def main():
    home = tempfile.mkdtemp(prefix="lgtv-easy-sim-")
    os.environ["LGTV_EASY_HOME"] = home
    print(f"[sim] Using throwaway config home: {home}")

    tv = MockTV(require_pairing=True, host="127.0.0.1").start()
    print(f"[sim] Mock LG OLED B-series listening at {tv.url}")
    print(f"[sim] TV state: screen_on={tv.screen_on}")

    def client_factory(ip):
        c = WebOSClient(ip)
        c._url = lambda: tv.url
        return c

    # ---- 1. The user runs first-time setup ----
    banner("STEP 1: First-time setup wizard (as the user would experience it)")
    answers = iter(["y", "1", "7"])  # scan, pick TV #1, 7-minute timeout
    cfg = Config()
    run_text_wizard(
        input_fn=lambda p: _ask(p, answers),
        output_fn=print,
        discover_fn=lambda *a, **k: [Discovered(ip="127.0.0.1",
                                                name="LG OLED B-series")],
        client_factory=client_factory,
        config=cfg,
    )
    assert tv.pair_prompts == 1, "TV should have shown a pairing prompt once"

    # ---- 2. A day at the desk: the daemon manages the screen ----
    banner("STEP 2: Using the PC - daemon watches idle time")
    saved = Config.load()
    print(f"[sim] Loaded config: {saved.device.name} @ {saved.device.ip}, "
          f"sleep after {saved.idle_minutes:g} min")
    idle = {"v": 0.0}
    daemon = Daemon(saved, client_factory=lambda: client_factory(saved.device.ip),
                    idle_fn=lambda: idle["v"])

    timeline = [
        (0, "just sat down, typing"),
        (60, "1 minute in, still working"),
        (6 * 60, "6 minutes idle (reading)"),
        (7 * 60 + 10, "stepped away - past 7 minutes"),
        (7 * 60 + 30, "still away"),
        (0, "came back, moved the mouse"),
    ]
    for seconds, note in timeline:
        idle["v"] = seconds
        daemon.tick()
        state = "ON " if tv.screen_on else "OFF"
        print(f"[sim] idle={seconds//60:>2}m{seconds%60:02d}s  "
              f"screen={state}  ({note})")

    # ---- 3. Verify outcome ----
    banner("STEP 3: Result")
    print(f"[sim] Times screen slept : {daemon.sleeps}")
    print(f"[sim] Times screen woke  : {daemon.wakes}")
    assert daemon.sleeps == 1 and daemon.wakes == 1
    assert tv.screen_on is True

    # ---- 4. The 'test' action a user clicks to confirm their TV ----
    banner("STEP 4: 'Test my TV' button (blink off then on)")
    c = client_factory(saved.device.ip)
    c.connect(client_key=saved.device.key)
    c.screen_off()
    print(f"[sim] screen_on after OFF command: {tv.screen_on}")
    c.screen_on()
    print(f"[sim] screen_on after ON command : {tv.screen_on}")
    c.close()

    tv.stop()
    print("\n[sim] SUCCESS - the full journey works end to end. ✓")
    return 0


def _ask(prompt, answers):
    """Echo the prompt with the scripted answer, mimicking a real terminal."""
    val = next(answers)
    print(f"{prompt}{val}")
    return val


if __name__ == "__main__":
    sys.exit(main())
