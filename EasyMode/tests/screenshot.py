"""Render each GUI screen and save a PNG (run under Xvfb).

Usage: screenshot.py <screen> <outfile>
  screens: wizard1, wizard3, settings
"""
import os
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
os.environ.setdefault("LGTV_EASY_HOME", tempfile.mkdtemp(prefix="lgtv-shot-"))

from lgtv_easy import gui  # noqa: E402
from lgtv_easy.config import Config, Device  # noqa: E402
from lgtv_easy.discovery import Discovered  # noqa: E402


def grab(app, outfile):
    for _ in range(30):
        app.update_idletasks(); app.update()
    time.sleep(0.4)
    wid = app.winfo_id()
    # Grab the whole root window by id via xwd -> convert to png.
    xwd = subprocess.run(["xwd", "-silent", "-id", str(wid)],
                         capture_output=True)
    subprocess.run(["convert", "xwd:-", outfile], input=xwd.stdout, check=True)
    print("wrote", outfile)


def main():
    screen, outfile = sys.argv[1], sys.argv[2]
    if screen == "settings":
        cfg = Config(idle_minutes=7, idle_enabled=True, setup_complete=True)
        cfg.device = Device(name="LG OLED B-series", ip="192.168.1.50",
                            key="paired")
        cfg.save()
    app = gui.App()
    app.update_idletasks(); app.update()
    root = app.container.winfo_children()[0]
    if screen == "wizard1":
        root.found = [Discovered(ip="192.168.1.50", name="LG OLED B-series"),
                      Discovered(ip="192.168.1.51", name="LG C2 Living Room")]
        root.listbox.delete(0, "end")
        for d in root.found:
            root.listbox.insert("end", f"{d.name}   ({d.ip})")
        root.listbox.selection_set(0)
        root.scan_status.config(text="Found 2 TV(s).")
    elif screen == "wizard3":
        root.selected_ip.set("192.168.1.50")
        root.selected_name.set("LG OLED B-series")
        root.client_key = "paired"
        root._build_step3()
        root.minutes.set(7)
        root._update_minutes_label()
    grab(app, outfile)
    app.destroy()


if __name__ == "__main__":
    main()
