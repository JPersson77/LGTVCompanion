"""Graphical wizard and settings window (tkinter).

Design goal: a complete beginner can go from nothing to "my TV sleeps when I
walk away" in under a minute, using a familiar Windows-style window.

Two screens, switched in-place inside one window:

* SetupWizard  - shown until setup is complete: Find TV -> Pair -> Timeout.
* SettingsPanel - the everyday screen: a big On/Off switch and a slider for the
  idle timeout, plus a "Test my TV" button and a status line.

All TV/idle logic lives in the verified core modules; this file only wires
widgets to them and never blocks the UI thread (network work runs in threads).
"""
from __future__ import annotations

import queue
import threading
import tkinter as tk
from tkinter import messagebox, ttk
from typing import Optional

from . import __version__
from . import autostart as autostart_mod
from .config import Config, Device
from .daemon import Daemon
from . import idle as idle_mod
from .discovery import discover
from .netdiag import probe_tv, subnet_report
from .webos import WebOSClient, pair_with_fallback

PAD = 12


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("LGTV Companion Easy Mode")
        self.geometry("480x560")
        self.minsize(460, 520)
        try:
            self.tk.call("tk", "scaling", 1.2)
        except tk.TclError:
            pass

        self.cfg = Config.load()
        self.daemon: Optional[Daemon] = None
        # Thread -> UI message pump so worker threads never touch widgets.
        self._events: "queue.Queue" = queue.Queue()
        self.after(100, self._pump)

        self.container = ttk.Frame(self, padding=PAD)
        self.container.pack(fill="both", expand=True)
        self._style()
        self._show_initial()

    # ----- infrastructure ---------------------------------------------
    def _style(self):
        style = ttk.Style(self)
        try:
            style.theme_use("clam" if "clam" in style.theme_names() else
                            style.theme_use())
        except tk.TclError:
            pass
        style.configure("Title.TLabel", font=("Segoe UI", 16, "bold"))
        style.configure("Sub.TLabel", font=("Segoe UI", 10))
        style.configure("Big.TButton", font=("Segoe UI", 11, "bold"), padding=8)

    def post(self, fn):
        """Schedule ``fn`` to run on the UI thread from any thread."""
        self._events.put(fn)

    def _pump(self):
        try:
            while True:
                self._events.get_nowait()()
        except queue.Empty:
            pass
        self.after(100, self._pump)

    def _clear(self):
        for child in self.container.winfo_children():
            child.destroy()

    def _show_initial(self):
        if self.cfg.setup_complete and self.cfg.device.paired:
            self.show_settings()
        else:
            self.show_wizard()

    def show_wizard(self):
        self._clear()
        SetupWizard(self.container, self).pack(fill="both", expand=True)

    def show_settings(self):
        self._clear()
        SettingsPanel(self.container, self).pack(fill="both", expand=True)
        self.start_daemon()

    # ----- daemon lifecycle -------------------------------------------
    def start_daemon(self):
        if self.daemon:
            self.daemon.config = self.cfg
            return
        if self.cfg.device.paired:
            self.daemon = Daemon(self.cfg)
            self.daemon.start()

    def on_close(self):
        if self.daemon:
            self.daemon.stop()
        self.destroy()


class SetupWizard(ttk.Frame):
    """Three-step wizard: find -> pair -> timeout."""

    def __init__(self, parent, app: App):
        super().__init__(parent)
        self.app = app
        self.step = 0
        self.found = []
        self.selected_ip = tk.StringVar(value=app.cfg.device.ip)
        self.selected_name = tk.StringVar(value=app.cfg.device.name or "My LG TV")
        self.minutes = tk.DoubleVar(value=app.cfg.idle_minutes)
        self.client_key = app.cfg.device.key
        self.secure = app.cfg.device.secure
        self._build_step1()

    def _header(self, title, subtitle):
        ttk.Label(self, text=title, style="Title.TLabel").pack(anchor="w")
        ttk.Label(self, text=subtitle, style="Sub.TLabel",
                  wraplength=410, justify="left").pack(anchor="w", pady=(2, PAD))

    def _reset(self):
        for c in self.winfo_children():
            c.destroy()

    def _make_diag(self, height=6):
        """A read-only, scrollable text area for diagnostics, plus a thread-safe
        appender. Worker threads call the returned function via app.post()."""
        frame = ttk.Frame(self)
        frame.pack(fill="both", expand=True, pady=(6, 0))
        text = tk.Text(frame, height=height, wrap="word", font=("Consolas", 9),
                       state="disabled", background="#f4f4f4", relief="flat")
        sb = ttk.Scrollbar(frame, orient="vertical", command=text.yview)
        text.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        text.pack(side="left", fill="both", expand=True)

        def append(line):
            text.configure(state="normal")
            text.insert(tk.END, line + "\n")
            text.see(tk.END)
            text.configure(state="disabled")

        # Thread-safe wrapper so worker threads can log into it.
        return lambda line: self.app.post(lambda: append(line))

    # ----- step 1: find ------------------------------------------------
    def _build_step1(self):
        self._reset()
        self._header("Step 1 of 3:  Find your TV",
                     "Make sure your LG TV is switched on and on the same "
                     "network as this PC.")
        self.listbox = tk.Listbox(self, height=6)
        self.listbox.pack(fill="x")
        self.scan_status = ttk.Label(self, text="", style="Sub.TLabel")
        self.scan_status.pack(anchor="w", pady=(4, 0))

        row = ttk.Frame(self)
        row.pack(fill="x", pady=PAD)
        ttk.Button(row, text="Scan for TVs",
                   command=self._scan).pack(side="left")
        ttk.Label(row, text="  or type the IP: ").pack(side="left")
        ttk.Entry(row, textvariable=self.selected_ip, width=16).pack(side="left")

        ttk.Label(self, text="Details:", style="Sub.TLabel").pack(anchor="w")
        self.diag = self._make_diag(height=5)
        # Show which network this PC is on up front: a TV that won't be found is
        # most often simply on a different network/subnet than the PC.
        threading.Thread(target=lambda: subnet_report("", self.diag),
                         daemon=True).start()

        ttk.Button(self, text="Next  ▶", style="Big.TButton",
                   command=self._goto_pair).pack(side="bottom", anchor="e")

    def _scan(self):
        self.scan_status.config(text="Scanning the network...")
        self.listbox.delete(0, tk.END)

        def worker():
            results = discover(log=self.diag)
            self.app.post(lambda: self._scan_done(results))

        threading.Thread(target=worker, daemon=True).start()

    def _scan_done(self, results):
        self.found = results
        if not results:
            self.scan_status.config(
                text="No TVs found. Type the IP address manually above.")
            return
        for dev in results:
            self.listbox.insert(tk.END, f"{dev.name}   ({dev.ip})")
        self.listbox.selection_set(0)
        self.scan_status.config(text=f"Found {len(results)} TV(s).")

    def _goto_pair(self):
        sel = self.listbox.curselection()
        if sel and self.found:
            dev = self.found[sel[0]]
            self.selected_ip.set(dev.ip)
            self.selected_name.set(dev.name)
        if not self.selected_ip.get().strip():
            messagebox.showwarning("Pick a TV",
                                   "Choose a TV from the list or type its IP.")
            return
        self._build_step2()

    # ----- step 2: pair ------------------------------------------------
    def _build_step2(self):
        self._reset()
        self._header("Step 2 of 3:  Pair with the TV",
                     f"Connecting to {self.selected_ip.get()} ...")
        self.pair_status = ttk.Label(self, text="Connecting...",
                                     style="Sub.TLabel", wraplength=410)
        self.pair_status.pack(anchor="w", pady=PAD)
        self.progress = ttk.Progressbar(self, mode="indeterminate")
        self.progress.pack(fill="x")
        self.progress.start(12)
        ttk.Label(self, text="Details:", style="Sub.TLabel").pack(anchor="w",
                                                                   pady=(PAD, 0))
        self.diag = self._make_diag(height=6)
        nav = ttk.Frame(self)
        nav.pack(side="bottom", fill="x")
        ttk.Button(nav, text="◀  Back",
                   command=self._build_step1).pack(side="left")
        self._pair()

    def _pair(self):
        ip = self.selected_ip.get().strip()

        def worker():
            # Surface the subnet check immediately (incl. the Google/Nest Wifi
            # double-NAT hint) so a mismatch is obvious before any timeout.
            subnet_report(ip, self.diag)
            client = WebOSClient(ip)
            try:
                key = pair_with_fallback(
                    client,
                    client_key=self.client_key,
                    on_prompt=lambda: self.app.post(self._prompt_accept),
                    prompt_timeout=120.0, log=self.diag,
                    prefer_secure=self.secure)
                secure = client.secure
                self.app.post(lambda: self._pair_done(key, secure))
            except Exception as exc:  # noqa: BLE001
                probe_tv(ip, self.diag)
                self.app.post(lambda e=exc: self._pair_failed(e))
            finally:
                client.close()

        threading.Thread(target=worker, daemon=True).start()

    def _prompt_accept(self):
        self.pair_status.config(
            text="👉  Look at your TV: press OK / Accept on the pairing prompt "
                 "with the remote.")

    def _pair_done(self, key, secure=False):
        self.client_key = key
        self.secure = secure
        self.progress.stop()
        self._build_step3()

    def _pair_failed(self, exc):
        self.progress.stop()
        self.pair_status.config(
            text=f"Could not pair: {exc}\n\nCheck the TV is on and the IP is "
                 "correct, then try again.")

    # ----- step 3: timeout --------------------------------------------
    def _build_step3(self):
        self._reset()
        self._header("Step 3 of 3:  Sleep timeout",
                     "How long should the PC be idle before the TV screen "
                     "turns off?")
        self.minutes_label = ttk.Label(self, style="Title.TLabel")
        self.minutes_label.pack(anchor="w")
        scale = ttk.Scale(self, from_=0.5, to=60, variable=self.minutes,
                          command=lambda v: self._update_minutes_label())
        scale.pack(fill="x", pady=(0, PAD))
        self._update_minutes_label()
        ttk.Label(self, text="Tip: 7 minutes is a good default for a desk "
                             "monitor.", style="Sub.TLabel").pack(anchor="w")
        ttk.Button(self, text="Finish  ✓", style="Big.TButton",
                   command=self._finish).pack(side="bottom", anchor="e")

    def _update_minutes_label(self):
        self.minutes_label.config(
            text=f"{round(self.minutes.get())} minutes of inactivity")

    def _finish(self):
        cfg = self.app.cfg
        cfg.device = Device(name=self.selected_name.get(),
                            ip=self.selected_ip.get().strip(),
                            mac=cfg.device.mac, key=self.client_key,
                            secure=self.secure)
        cfg.idle_minutes = round(self.minutes.get())
        cfg.idle_enabled = True
        cfg.setup_complete = True
        cfg.save()
        self.app.show_settings()


class SettingsPanel(ttk.Frame):
    """The everyday screen: big On/Off switch + timeout slider."""

    def __init__(self, parent, app: App):
        super().__init__(parent)
        self.app = app
        cfg = app.cfg
        self.enabled = tk.BooleanVar(value=cfg.idle_enabled)
        self.minutes = tk.DoubleVar(value=cfg.idle_minutes)
        self.mute = tk.BooleanVar(value=cfg.mute_on_sleep)
        self.deep = tk.BooleanVar(value=cfg.deep_off_enabled)
        self.deep_minutes = tk.DoubleVar(value=cfg.deep_off_minutes)
        self.autostart = tk.BooleanVar(value=autostart_mod.is_enabled())
        self._build()

    def _build(self):
        cfg = self.app.cfg
        ttk.Label(self, text="LGTV Companion Easy Mode",
                  style="Title.TLabel").pack(anchor="w")
        ttk.Label(self, text=f"Connected to: {cfg.device.name} "
                             f"({cfg.device.ip})",
                  style="Sub.TLabel").pack(anchor="w", pady=(0, PAD))

        # Big on/off switch.
        ttk.Checkbutton(self, text="Turn the screen off when I'm away",
                        variable=self.enabled,
                        command=self._apply).pack(anchor="w")

        box = ttk.LabelFrame(self, text="Sleep after this many minutes idle",
                             padding=PAD)
        box.pack(fill="x", pady=PAD)
        self.minutes_label = ttk.Label(box, style="Title.TLabel")
        self.minutes_label.pack(anchor="w")
        ttk.Scale(box, from_=0.5, to=60, variable=self.minutes,
                  command=lambda v: self._slider_moved()).pack(fill="x")
        self._update_minutes_label()

        ttk.Checkbutton(self, text="Also mute the speakers when sleeping",
                        variable=self.mute,
                        command=self._apply).pack(anchor="w")

        energy = ttk.LabelFrame(self, text="Maximum energy saving", padding=PAD)
        energy.pack(fill="x", pady=PAD)
        ttk.Checkbutton(
            energy,
            text="Fully power the TV off after a longer idle (wakes via Wake-on-LAN)",
            variable=self.deep, command=self._apply).pack(anchor="w")
        drow = ttk.Frame(energy)
        drow.pack(fill="x", pady=(4, 0))
        ttk.Label(drow, text="Power off after (minutes):").pack(side="left")
        spin = ttk.Spinbox(drow, from_=2, to=240, width=6,
                           textvariable=self.deep_minutes, command=self._apply)
        spin.pack(side="left", padx=6)
        spin.bind("<Return>", lambda e: self._apply())
        spin.bind("<FocusOut>", lambda e: self._apply())

        ttk.Checkbutton(self, text="Start automatically when I log in",
                        variable=self.autostart,
                        command=self._apply_autostart).pack(anchor="w")

        self.status = ttk.Label(self, text="", style="Sub.TLabel",
                                wraplength=410)
        self.status.pack(anchor="w", pady=(PAD, 0))
        self._refresh_status()

        nav = ttk.Frame(self)
        nav.pack(side="bottom", fill="x", pady=(PAD, 0))
        ttk.Button(nav, text="Test my TV",
                   command=self._test).pack(side="left")
        ttk.Button(nav, text="Re-run setup",
                   command=self.app.show_wizard).pack(side="left", padx=6)
        ttk.Label(nav, text=f"v{__version__}",
                  style="Sub.TLabel").pack(side="right")

    def _update_minutes_label(self):
        self.minutes_label.config(text=f"{round(self.minutes.get())} minutes")

    def _slider_moved(self):
        self._update_minutes_label()
        self._apply()

    def _apply(self):
        cfg = self.app.cfg
        cfg.idle_enabled = self.enabled.get()
        cfg.idle_minutes = round(self.minutes.get())
        cfg.mute_on_sleep = self.mute.get()
        cfg.deep_off_enabled = self.deep.get()
        try:
            cfg.deep_off_minutes = max(2.0, float(self.deep_minutes.get()))
        except (tk.TclError, ValueError):
            pass
        cfg.save()
        self.app.start_daemon()
        self._refresh_status()

    def _apply_autostart(self):
        autostart_mod.set_enabled(self.autostart.get())
        self._refresh_status()

    def _refresh_status(self):
        cfg = self.app.cfg
        backend = idle_mod.idle_backend_name()
        warn = "" if idle_mod.is_real_backend() else \
            "  (warning: OS idle detection unavailable here)"
        state = "ON" if cfg.idle_enabled else "OFF"
        deep = (f" Full power-off after {round(cfg.deep_off_minutes)} min."
                if cfg.deep_off_enabled else "")
        self.status.config(
            text=f"Status: idle-sleep is {state}, after {round(cfg.idle_minutes)} "
                 f"min.{deep} Idle detection: {backend}.{warn}")

    def _test(self):
        cfg = self.app.cfg
        self.status.config(text="Testing: turning your screen off, then on...")

        def worker():
            ok, err = True, ""
            client = WebOSClient(cfg.device.ip, secure=cfg.device.secure)
            try:
                pair_with_fallback(client, client_key=cfg.device.key,
                                   prefer_secure=cfg.device.secure)
                client.screen_off()
                import time
                time.sleep(2)
                client.screen_on()
            except Exception as exc:  # noqa: BLE001
                ok, err = False, str(exc)
            finally:
                client.close()
            self.app.post(lambda: self._test_done(ok, err))

        threading.Thread(target=worker, daemon=True).start()

    def _test_done(self, ok, err):
        if ok:
            self.status.config(text="Test OK — your TV responded. ✓")
        else:
            self.status.config(text=f"Test failed: {err}")


def main() -> int:
    app = App()
    app.protocol("WM_DELETE_WINDOW", app.on_close)
    app.mainloop()
    return 0


if __name__ == "__main__":
    main()
