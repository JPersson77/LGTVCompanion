"""Graphical wizard and settings window (tkinter).

Design goal: a complete beginner can go from nothing to "my TV sleeps when I
walk away" in under a minute, using a clean, modern-looking window.

Two screens, switched in-place inside one window:

* SetupWizard  - shown until setup is complete: Find TV -> Pair -> Timeout.
* SettingsPanel - the everyday screen: a big On/Off switch and a slider for the
  idle timeout, plus a "Test my TV" button and a status line.

All TV/idle logic lives in the verified core modules; this file only wires
widgets to them and never blocks the UI thread (network work runs in threads).

The look is a flat dark theme built on ttk's "clam" engine plus a couple of small
hand-drawn widgets (a pill toggle switch, an accent rule), so it stays dependency
-free and renders the same on Windows and Linux.
"""
from __future__ import annotations

import queue
import threading
import tkinter as tk
from tkinter import font as tkfont
from tkinter import messagebox, ttk
from typing import Optional

from . import __version__
from . import autostart as autostart_mod
from .config import Config, Device, fmt_timeout
from .daemon import Daemon
from . import idle as idle_mod
from .discovery import discover_tvs
from .netdiag import probe_tv, subnet_report
from .webos import WebOSClient, pair_with_fallback

PAD = 14

# Flat dark palette. Kept in one place so every widget pulls the same colours.
PALETTE = {
    "bg":        "#15171C",   # window background
    "surface":   "#1E2128",   # cards
    "inset":     "#262A33",   # fields: entry, listbox, text, slider trough
    "border":    "#343A45",
    "text":      "#ECEEF2",
    "muted":     "#98A0AD",
    "accent":    "#5B8CFF",
    "accent_hi": "#7AA2FF",
    "accent_lo": "#4377F0",
    "danger":    "#FF6B6B",
    "ok":        "#48D597",
}

# Populated by ``_apply_theme`` with the palette plus the resolved font families,
# so the hand-drawn widgets (which aren't ttk-styled) can read them too.
THEME: dict = dict(PALETTE, ui="Helvetica", mono="Courier")


def _apply_theme(root: tk.Misc) -> dict:
    """Configure ttk styles for the whole app and return the resolved THEME."""
    style = ttk.Style(root)
    try:
        style.theme_use("clam")
    except tk.TclError:
        pass

    families = set(tkfont.families(root))

    def pick(prefs, default):
        for name in prefs:
            if name in families:
                return name
        return default

    ui = pick(["Segoe UI", "Inter", "SF Pro Text", "Ubuntu", "Cantarell",
               "Noto Sans", "DejaVu Sans"], "Helvetica")
    mono = pick(["Cascadia Mono", "Cascadia Code", "Consolas", "SF Mono",
                 "Ubuntu Mono", "DejaVu Sans Mono", "Noto Sans Mono"], "Courier")
    THEME.update(PALETTE)
    THEME["ui"], THEME["mono"] = ui, mono
    P = PALETTE

    root.configure(bg=P["bg"])
    style.configure(".", background=P["bg"], foreground=P["text"],
                    fieldbackground=P["inset"], bordercolor=P["border"],
                    lightcolor=P["bg"], darkcolor=P["bg"], font=(ui, 10))
    style.map(".", foreground=[("disabled", P["muted"])])

    style.configure("TFrame", background=P["bg"])
    style.configure("Card.TFrame", background=P["surface"])

    style.configure("TLabel", background=P["bg"], foreground=P["text"], font=(ui, 10))
    style.configure("Brand.TLabel", background=P["bg"], foreground=P["text"],
                    font=(ui, 13, "bold"))
    style.configure("Title.TLabel", background=P["bg"], foreground=P["text"],
                    font=(ui, 19, "bold"))
    style.configure("Sub.TLabel", background=P["bg"], foreground=P["muted"], font=(ui, 10))
    style.configure("Card.TLabel", background=P["surface"], foreground=P["text"], font=(ui, 10))
    style.configure("CardTitle.TLabel", background=P["surface"], foreground=P["text"],
                    font=(ui, 11, "bold"))
    style.configure("CardMuted.TLabel", background=P["surface"], foreground=P["muted"],
                    font=(ui, 9))
    style.configure("Value.TLabel", background=P["surface"], foreground=P["accent"],
                    font=(ui, 24, "bold"))

    style.configure("TButton", background=P["inset"], foreground=P["text"],
                    bordercolor=P["border"], lightcolor=P["inset"], darkcolor=P["inset"],
                    borderwidth=1, relief="flat", focusthickness=0,
                    padding=(14, 9), font=(ui, 10))
    style.map("TButton", background=[("active", P["border"]), ("pressed", P["border"])],
              bordercolor=[("focus", P["accent"])])
    style.configure("Accent.TButton", background=P["accent"], foreground="#FFFFFF",
                    bordercolor=P["accent"], lightcolor=P["accent"], darkcolor=P["accent"],
                    borderwidth=0, relief="flat", padding=(18, 10), font=(ui, 10, "bold"))
    style.map("Accent.TButton",
              background=[("active", P["accent_hi"]), ("pressed", P["accent_lo"])],
              foreground=[("disabled", "#FFFFFF")])
    style.configure("Ghost.TButton", background=P["bg"], foreground=P["muted"],
                    bordercolor=P["bg"], lightcolor=P["bg"], darkcolor=P["bg"],
                    borderwidth=0, relief="flat", padding=(12, 9), font=(ui, 10))
    style.map("Ghost.TButton", background=[("active", P["surface"])],
              foreground=[("active", P["text"])])

    style.configure("TEntry", fieldbackground=P["inset"], foreground=P["text"],
                    bordercolor=P["border"], insertcolor=P["text"], relief="flat",
                    padding=6)
    style.map("TEntry", bordercolor=[("focus", P["accent"])])
    style.configure("TSpinbox", fieldbackground=P["inset"], foreground=P["text"],
                    background=P["inset"], bordercolor=P["border"], arrowcolor=P["muted"],
                    relief="flat", padding=5)
    style.map("TSpinbox", bordercolor=[("focus", P["accent"])])

    # gripcount=0 drops clam's default "barcode" dashes on the slider handle for a
    # clean solid grip; a defined sliderlength keeps it a comfortable target.
    style.configure("Horizontal.TScale", background=P["accent"], troughcolor=P["inset"],
                    bordercolor=P["surface"], lightcolor=P["accent"], darkcolor=P["accent"],
                    gripcount=0, sliderlength=24)
    style.map("Horizontal.TScale", background=[("active", P["accent_hi"])])
    style.configure("TProgressbar", background=P["accent"], troughcolor=P["inset"],
                    bordercolor=P["surface"], lightcolor=P["accent"], darkcolor=P["accent"])
    style.configure("Vertical.TScrollbar", background=P["inset"], troughcolor=P["bg"],
                    bordercolor=P["bg"], arrowcolor=P["muted"], relief="flat")
    style.map("Vertical.TScrollbar", background=[("active", P["border"])])
    return THEME


class ToggleSwitch(tk.Canvas):
    """A small pill on/off switch bound to a ``tk.BooleanVar`` (hand-drawn).

    ttk's checkbutton indicator can't be themed cleanly across platforms, so the
    boolean options use this instead - it reads as a modern toggle and follows
    the variable both ways (clicking flips it; setting the var redraws it)."""

    WIDTH, HEIGHT = 48, 26

    def __init__(self, parent, variable: tk.BooleanVar, command=None, bg=None):
        super().__init__(parent, width=self.WIDTH, height=self.HEIGHT,
                         highlightthickness=0, bd=0,
                         bg=bg or THEME["surface"], cursor="hand2")
        self.var = variable
        self.command = command
        self.bind("<Button-1>", self._clicked)
        self.var.trace_add("write", lambda *a: self._draw())
        self._draw()

    def _clicked(self, _event=None):
        self.var.set(not bool(self.var.get()))
        if self.command:
            self.command()

    def _draw(self):
        self.delete("all")
        on = bool(self.var.get())
        track = THEME["accent"] if on else THEME["inset"]
        knob = "#FFFFFF" if on else THEME["muted"]
        h, w = self.HEIGHT, self.WIDTH
        self.create_oval(0, 0, h, h, fill=track, outline=track)
        self.create_oval(w - h, 0, w, h, fill=track, outline=track)
        self.create_rectangle(h / 2, 0, w - h / 2, h, fill=track, outline=track)
        pad = 3
        d = h - 2 * pad
        x = (w - h + pad) if on else pad
        self.create_oval(x, pad, x + d, pad + d, fill=knob, outline=knob)


def _build_steps(*ranges) -> "list":
    """Distinct values from (start, stop, step) ranges, in order.

    Used to build a non-linear timeout scale: fine steps where small values
    matter, coarse steps higher up - so the slider is precise at 10 seconds and
    still reaches 2 hours without a thousand positions in between.
    """
    vals: list = []
    for start, stop, step in ranges:
        v = start
        while v <= stop + 1e-9:
            iv = int(round(v))
            if iv not in vals:
                vals.append(iv)
            v += step
    return vals


# Sleep (screen-off): 10s->1min by 10s, 1->10min by 1min, 10->60min by 5min,
# 60->120min by 10min.
SLEEP_STEPS_SEC = _build_steps((10, 60, 10), (60, 600, 60),
                               (600, 3600, 300), (3600, 7200, 600))
# Deep power-off is "a longer idle", so it starts at 1 minute (no sub-minute);
# its upper range matches sleep (5-minute steps to 60, then 10-minute steps).
DEEP_STEPS_SEC = _build_steps((60, 600, 60), (600, 3600, 300), (3600, 7200, 600))


class SteppedSlider(ttk.Frame):
    """A slider that snaps to a fixed list of values, with a live value label.

    tkinter's Scale is linear; driving it over indices into ``values`` gives the
    non-linear feel we want and sidesteps the flaky ttk.Spinbox (whose mouse-wheel
    / typing handling misbehaves on Linux). ``fmt`` renders a value for the label;
    ``command`` (optional) fires once each time the snapped value changes.
    """

    def __init__(self, parent, *, values, initial, fmt, command=None):
        super().__init__(parent, style="Card.TFrame")
        self.values = list(values)
        self._fmt = fmt
        self._command = command
        self._idx = self._nearest(initial)
        self._busy = False
        self.label = ttk.Label(self, style="Value.TLabel")
        self.label.pack(anchor="w")
        self.scale = ttk.Scale(self, from_=0, to=len(self.values) - 1,
                               command=self._on_move)
        # Quantise to a step while dragging (so the value/label are always a real
        # step), but only snap the handle itself once the drag ends - setting the
        # scale value mid-motion can fight the drag gesture on some Tk builds.
        self.scale.bind("<ButtonRelease-1>", self._snap)
        self.scale.bind("<KeyRelease>", self._snap)
        self.scale.pack(fill="x", pady=(6, 0))
        self._busy = True            # set initial position without firing command
        self.scale.set(self._idx)
        self._busy = False
        self._refresh()

    def _nearest(self, value) -> int:
        return min(range(len(self.values)),
                   key=lambda i: abs(self.values[i] - value))

    def _on_move(self, raw):
        if self._busy:
            return
        idx = max(0, min(len(self.values) - 1, int(round(float(raw)))))
        changed = idx != self._idx
        self._idx = idx
        self._refresh()
        if changed and self._command:
            self._command()

    def _snap(self, _event=None):
        """Rest the handle exactly on the selected step once the drag ends."""
        self._busy = True
        self.scale.set(self._idx)
        self._busy = False

    def _refresh(self):
        self.label.config(text=self._fmt(self.value()))

    def value(self):
        """The currently selected raw value (seconds)."""
        return self.values[self._idx]

    def set_value(self, value):
        """Set programmatically to the nearest step (no command fired)."""
        self._idx = self._nearest(value)
        self._busy = True
        self.scale.set(self._idx)
        self._busy = False
        self._refresh()


def make_diag(app: "App", parent: tk.Misc, height: int = 6):
    """A read-only, scrollable diagnostics text area + a thread-safe appender.

    Shared by the setup wizard and the repair dialog. The returned callable can
    be handed straight to worker threads (and to ``selfheal``/``discovery`` as a
    ``log``): it marshals each line back onto the UI thread via ``app.post`` and
    ignores writes to a widget that has since been destroyed, so a still-running
    worker can never freeze the UI by logging into a closed screen.
    """
    frame = ttk.Frame(parent, style="Card.TFrame")
    frame.pack(fill="both", expand=True, pady=(6, 0))
    text = tk.Text(frame, height=height, wrap="word", font=(THEME["mono"], 9),
                   state="disabled", background=THEME["inset"],
                   foreground=THEME["muted"], relief="flat", borderwidth=0,
                   highlightthickness=1, highlightbackground=THEME["border"],
                   highlightcolor=THEME["border"], padx=8, pady=6,
                   insertbackground=THEME["text"])
    sb = ttk.Scrollbar(frame, orient="vertical", command=text.yview)
    text.configure(yscrollcommand=sb.set)
    sb.pack(side="right", fill="y")
    text.pack(side="left", fill="both", expand=True)

    def append(line):
        try:
            text.configure(state="normal")
            text.insert(tk.END, line + "\n")
            text.see(tk.END)
            text.configure(state="disabled")
        except tk.TclError:
            pass

    return lambda line: app.post(lambda: append(line))


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("LGTV Companion Easy Mode")
        self.geometry("540x715")
        self.minsize(520, 640)
        _apply_theme(self)
        try:
            self.tk.call("tk", "scaling", 1.2)
        except tk.TclError:
            pass

        self.cfg = Config.load()
        self.daemon: Optional[Daemon] = None
        self._lock = None  # singleton guard held while WE run the watcher
        # Thread -> UI message pump so worker threads never touch widgets.
        self._events: "queue.Queue" = queue.Queue()
        self._pump_id = self.after(100, self._pump)

        self._install_reload_signal()
        self._build_chrome()
        self.container = ttk.Frame(self, padding=(PAD + 4, 0, PAD + 4, PAD + 4))
        self.container.pack(fill="both", expand=True)
        self._show_initial()

    # ----- infrastructure ---------------------------------------------
    def _install_reload_signal(self):
        """Handle SIGHUP so a `lgtv-easy set` from a terminal applies live when
        this window owns the watcher - and, just as importantly, so that nudge
        never falls through to SIGHUP's default action of killing the window.
        POSIX only; harmless when we don't own the daemon."""
        import signal
        if not hasattr(signal, "SIGHUP"):
            return

        def _on_hup(_signum=None, _frame=None):
            if self.daemon is not None:
                self.daemon.request_reload()

        try:
            signal.signal(signal.SIGHUP, _on_hup)
        except (ValueError, OSError):
            pass
    def _build_chrome(self):
        """A persistent brand bar + accent rule across the top of the window."""
        bar = ttk.Frame(self, padding=(PAD + 4, 16, PAD + 4, 12))
        bar.pack(fill="x")
        dot = tk.Canvas(bar, width=12, height=12, highlightthickness=0, bd=0,
                        bg=THEME["bg"])
        dot.create_oval(1, 1, 11, 11, fill=THEME["accent"], outline=THEME["accent"])
        dot.pack(side="left", padx=(0, 9))
        ttk.Label(bar, text="LGTV Companion", style="Brand.TLabel").pack(side="left")
        ttk.Label(bar, text="Easy Mode", style="Sub.TLabel").pack(
            side="left", padx=(8, 0))
        tk.Frame(self, height=2, bg=THEME["accent"]).pack(fill="x")

    def post(self, fn):
        """Schedule ``fn`` to run on the UI thread from any thread."""
        self._events.put(fn)

    def _pump(self):
        # Drain queued UI callbacks. Each is isolated in try/except: one failing
        # callback (e.g. writing to a widget the wizard just destroyed) must not
        # stop the pump, or the whole window would freeze. We always reschedule.
        try:
            while True:
                fn = self._events.get_nowait()
                try:
                    fn()
                except Exception:  # noqa: BLE001 - keep the pump alive
                    pass
        except queue.Empty:
            pass
        self._pump_id = self.after(100, self._pump)

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
        """Watch for idle while the window is open - but only if nobody else is.

        A background supervisor (the launcher) or a login auto-start entry may
        already be driving the TV. Exactly one watcher must own it, so we take
        the same single-instance lock the headless ``run`` command uses. If it's
        already held, we leave the running watcher alone and just act as a
        settings panel; the status line says so.
        """
        if self.daemon:
            # We own the watcher in-process: the daemon already shares this very
            # config object, so the edit is visible; nudge it to apply now.
            self.daemon.config = self.cfg
            self.daemon.nudge()
            return
        if not self.cfg.device.paired:
            return
        from .singleton import SingleInstance
        if self._lock is None:
            self._lock = SingleInstance("daemon")
        if not self._lock.acquire(wait=False):
            self._lock = None  # someone else owns the watcher; don't compete
            return
        self.daemon = Daemon(self.cfg)
        self.daemon.start()

    def watcher_holder(self):
        """PID of whatever process currently owns the watcher lock (or None)."""
        from .singleton import SingleInstance
        return SingleInstance("daemon").holder()

    def notify_running_daemon(self):
        """Tell a *separate* background watcher to re-read the settings we just
        saved, so the change applies at once instead of on its next restart.

        A no-op when this window owns the watcher (it already shares the config
        object and was nudged directly) or when the OS has no SIGHUP (Windows):
        ``signal`` never targets our own process.
        """
        import signal
        sig = getattr(signal, "SIGHUP", None)
        if sig is None:
            return
        from .singleton import SingleInstance
        SingleInstance("daemon").signal(sig)

    def on_close(self):
        if self.daemon:
            self.daemon.stop()
            self.daemon = None
        if self._lock:
            self._lock.release()
            self._lock = None
        # Cancel the pending pump callback so it can't fire on a destroyed window.
        if getattr(self, "_pump_id", None) is not None:
            try:
                self.after_cancel(self._pump_id)
            except tk.TclError:
                pass
            self._pump_id = None
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
        self.client_key = app.cfg.device.key
        self.secure = app.cfg.device.secure
        self._build_step1()

    def _header(self, title, subtitle):
        ttk.Label(self, text=title, style="Title.TLabel").pack(anchor="w", pady=(8, 0))
        ttk.Label(self, text=subtitle, style="Sub.TLabel",
                  wraplength=440, justify="left").pack(anchor="w", pady=(4, PAD))

    def _step_badge(self, n):
        ttk.Label(self, text=f"STEP {n} OF 3", style="Sub.TLabel").pack(anchor="w")

    def _reset(self):
        for c in self.winfo_children():
            c.destroy()

    def _card(self):
        card = ttk.Frame(self, style="Card.TFrame", padding=PAD)
        card.pack(fill="x", pady=(0, PAD))
        return card

    def _make_diag(self, parent=None, height=6):
        """A read-only, scrollable text area for diagnostics, plus a thread-safe
        appender. Worker threads call the returned function via app.post()."""
        return make_diag(self.app, parent or self, height)

    # ----- step 1: find ------------------------------------------------
    def _build_step1(self):
        self._reset()
        self._step_badge(1)
        self._header("Find your TV",
                     "Make sure your LG TV is switched on and on the same "
                     "network as this PC.")

        card = self._card()
        self.listbox = tk.Listbox(card, height=5, background=THEME["inset"],
                                  foreground=THEME["text"],
                                  selectbackground=THEME["accent"],
                                  selectforeground="#FFFFFF", relief="flat",
                                  borderwidth=0, highlightthickness=1,
                                  highlightbackground=THEME["border"],
                                  highlightcolor=THEME["accent"],
                                  font=(THEME["ui"], 10), activestyle="none")
        self.listbox.pack(fill="x")
        self.scan_status = ttk.Label(card, text="", style="CardMuted.TLabel")
        self.scan_status.pack(anchor="w", pady=(8, 0))

        row = ttk.Frame(card, style="Card.TFrame")
        row.pack(fill="x", pady=(PAD, 0))
        ttk.Button(row, text="Scan for TVs", command=self._scan).pack(side="left")
        ttk.Label(row, text="or type the IP:", style="CardMuted.TLabel").pack(
            side="left", padx=(10, 6))
        ttk.Entry(row, textvariable=self.selected_ip, width=16).pack(
            side="left", fill="x", expand=True)

        ttk.Label(self, text="Details", style="Sub.TLabel").pack(anchor="w")
        self.diag = self._make_diag(height=5)
        # Show which network this PC is on up front: a TV that won't be found is
        # most often simply on a different network/subnet than the PC.
        threading.Thread(target=lambda: subnet_report("", self.diag),
                         daemon=True).start()

        ttk.Button(self, text="Next  →", style="Accent.TButton",
                   command=self._goto_pair).pack(side="bottom", anchor="e",
                                                 pady=(PAD, 0))

    def _scan(self):
        self.scan_status.config(text="Scanning the network…")
        self.listbox.delete(0, tk.END)

        def worker():
            results = discover_tvs(log=self.diag)
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
        self._step_badge(2)
        self._header("Pair with the TV",
                     f"Connecting to {self.selected_ip.get()} …")

        card = self._card()
        self.pair_status = ttk.Label(card, text="Connecting…",
                                     style="Card.TLabel", wraplength=440)
        self.pair_status.pack(anchor="w")
        self.progress = ttk.Progressbar(card, mode="indeterminate")
        self.progress.pack(fill="x", pady=(PAD, 0))
        self.progress.start(12)

        ttk.Label(self, text="Details", style="Sub.TLabel").pack(anchor="w")
        self.diag = self._make_diag(height=6)
        nav = ttk.Frame(self)
        nav.pack(side="bottom", fill="x", pady=(PAD, 0))
        ttk.Button(nav, text="←  Back", style="Ghost.TButton",
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
        self._step_badge(3)
        self._header("Sleep timeout",
                     "How long should the PC be idle before the TV screen "
                     "turns off?")

        card = self._card()
        self.sleep_slider = SteppedSlider(
            card, values=SLEEP_STEPS_SEC, initial=self.app.cfg.idle_minutes * 60,
            fmt=lambda s: f"{fmt_timeout(s)} of inactivity")
        self.sleep_slider.pack(fill="x")
        ttk.Label(self, text="Tip: 7 minutes is a good default for a desk "
                             "monitor.", style="Sub.TLabel").pack(anchor="w")
        ttk.Button(self, text="Finish  ✓", style="Accent.TButton",
                   command=self._finish).pack(side="bottom", anchor="e",
                                              pady=(PAD, 0))

    def _finish(self):
        cfg = self.app.cfg
        cfg.device = Device(name=self.selected_name.get(),
                            ip=self.selected_ip.get().strip(),
                            mac=cfg.device.mac, key=self.client_key,
                            secure=self.secure)
        cfg.idle_minutes = self.sleep_slider.value() / 60.0
        cfg.idle_enabled = True
        cfg.setup_complete = True
        cfg.save()
        self.app.show_settings()
        self.app.notify_running_daemon()


class SettingsPanel(ttk.Frame):
    """The everyday screen: big On/Off switch + timeout slider."""

    def __init__(self, parent, app: App):
        super().__init__(parent)
        self.app = app
        cfg = app.cfg
        self.enabled = tk.BooleanVar(value=cfg.idle_enabled)
        self.mute = tk.BooleanVar(value=cfg.mute_on_sleep)
        self.follow_sleep = tk.BooleanVar(value=cfg.screen_off_on_pc_sleep)
        self.deep = tk.BooleanVar(value=cfg.deep_off_enabled)
        self.autostart = tk.BooleanVar(value=autostart_mod.is_enabled())
        self._status_dot = None
        self._build()

    # ----- small builders ---------------------------------------------
    def _card(self, title=None):
        card = ttk.Frame(self, style="Card.TFrame", padding=PAD)
        card.pack(fill="x", pady=(0, PAD - 4))
        if title:
            ttk.Label(card, text=title, style="CardTitle.TLabel").pack(
                anchor="w", pady=(0, 6))
        return card

    def _switch_row(self, parent, text, variable, command, desc=None):
        row = ttk.Frame(parent, style="Card.TFrame")
        row.pack(fill="x", pady=4)
        labels = ttk.Frame(row, style="Card.TFrame")
        labels.pack(side="left", fill="x", expand=True)
        ttk.Label(labels, text=text, style="Card.TLabel").pack(anchor="w")
        if desc:
            ttk.Label(labels, text=desc, style="CardMuted.TLabel",
                      wraplength=360, justify="left").pack(anchor="w")
        ToggleSwitch(row, variable, command=command,
                     bg=THEME["surface"]).pack(side="right", padx=(10, 0))
        return row

    def _build(self):
        cfg = self.app.cfg

        # Footer first, pinned to the bottom so the actions stay visible no matter
        # how tall the cards above end up.
        nav = ttk.Frame(self)
        nav.pack(side="bottom", fill="x", pady=(PAD, 0))
        ttk.Button(nav, text="Test my TV", command=self._test).pack(side="left")
        ttk.Button(nav, text="Re-run setup", style="Ghost.TButton",
                   command=self.app.show_wizard).pack(side="left", padx=6)
        ttk.Label(nav, text=f"v{__version__}", style="Sub.TLabel").pack(side="right")

        statusrow = ttk.Frame(self)
        statusrow.pack(side="bottom", fill="x", pady=(PAD - 4, 0))
        self._status_dot = tk.Canvas(statusrow, width=10, height=10,
                                     highlightthickness=0, bd=0, bg=THEME["bg"])
        self._status_dot.pack(side="left", padx=(0, 8), pady=(3, 0), anchor="n")
        self.status = ttk.Label(statusrow, text="", style="Sub.TLabel",
                                wraplength=420, justify="left")
        self.status.pack(side="left", fill="x", expand=True)

        # Compact "connected to" line instead of a whole card. Kept on the panel
        # so the startup self-test / repair can update the address if the TV moved.
        self._conn_label = ttk.Label(
            self, text=self._conn_text(), style="Sub.TLabel")
        self._conn_label.pack(anchor="w", pady=(0, PAD - 2))

        # Hero: the big switch + timeout slider.
        hero = self._card()
        top = ttk.Frame(hero, style="Card.TFrame")
        top.pack(fill="x")
        tl = ttk.Frame(top, style="Card.TFrame")
        tl.pack(side="left", fill="x", expand=True)
        ttk.Label(tl, text="Turn the screen off when I'm away",
                  style="CardTitle.TLabel").pack(anchor="w")
        ttk.Label(tl, text="Any key or mouse move wakes it.",
                  style="CardMuted.TLabel").pack(anchor="w")
        ToggleSwitch(top, self.enabled, command=self._apply,
                     bg=THEME["surface"]).pack(side="right", padx=(10, 0))

        ttk.Label(hero, text="Sleep after", style="CardMuted.TLabel").pack(
            anchor="w", pady=(PAD - 2, 0))
        self.sleep_slider = SteppedSlider(
            hero, values=SLEEP_STEPS_SEC, initial=cfg.idle_minutes * 60,
            fmt=fmt_timeout, command=self._apply)
        self.sleep_slider.pack(fill="x")

        # When it sleeps.
        opts = self._card("When it sleeps")
        self._switch_row(opts, "Also mute the speakers", self.mute, self._apply)
        self._switch_row(opts, "Sleep the TV when the PC sleeps",
                         self.follow_sleep, self._apply,
                         desc="Follows the PC into and back out of suspend.")

        # More options: energy saving + start at login. The "Power off after"
        # slider only makes sense once deep power-off is on, so it's revealed with
        # the toggle (progressive disclosure) rather than sitting there inert while
        # the feature is off. The reveal is an in-place show/hide - no rebuild - so
        # it's instant, and the slider applies its timing live as it's dragged.
        more = self._card("More options")
        self._switch_row(
            more, "Fully power the TV off after a longer idle", self.deep,
            self._apply_deep, desc="Maximum energy saving; wakes over Wake-on-LAN.")
        self._deep_row = ttk.Frame(more, style="Card.TFrame")
        ttk.Label(self._deep_row, text="Power off after",
                  style="CardMuted.TLabel").pack(anchor="w")
        self.deep_slider = SteppedSlider(
            self._deep_row, values=DEEP_STEPS_SEC, initial=cfg.deep_off_minutes * 60,
            fmt=fmt_timeout, command=self._apply)
        self.deep_slider.pack(fill="x")
        # Keep the login row's handle so the slider can be inserted just above it
        # (pack with `before=`) when revealed, preserving the card's order.
        self._autostart_row = self._switch_row(
            more, "Start automatically when I log in",
            self.autostart, self._apply_autostart)
        self._sync_deep_row()

        self._refresh_status()
        self._kickoff_selftest()

    def _sync_deep_row(self):
        """Show the power-off timing slider iff deep power-off is enabled."""
        if self.deep.get():
            self._deep_row.pack(fill="x", pady=(4, 8), before=self._autostart_row)
        else:
            self._deep_row.pack_forget()

    def _apply_deep(self):
        """Toggle handler for deep power-off: apply, then reveal/hide its slider."""
        self._apply()
        self._sync_deep_row()

    def _apply(self):
        cfg = self.app.cfg
        cfg.idle_enabled = self.enabled.get()
        cfg.idle_minutes = self.sleep_slider.value() / 60.0
        cfg.mute_on_sleep = self.mute.get()
        cfg.screen_off_on_pc_sleep = self.follow_sleep.get()
        cfg.deep_off_enabled = self.deep.get()
        cfg.deep_off_minutes = self.deep_slider.value() / 60.0
        cfg.save()
        self.app.start_daemon()
        self.app.notify_running_daemon()
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
        deep = (f" Full power-off after {fmt_timeout(cfg.deep_off_minutes * 60)}."
                if cfg.deep_off_enabled else "")
        # Who is actually watching for idle right now: this window, or an
        # already-running background watcher we deliberately didn't duplicate.
        if self.app.daemon is not None:
            who = " Watching now."
        else:
            holder = self.app.watcher_holder()
            who = (f" Running in the background (pid {holder})."
                   if holder else " Watcher will start when you close this window.")
        if self._status_dot is not None:
            colour = THEME["ok"] if cfg.idle_enabled else THEME["muted"]
            self._status_dot.delete("all")
            self._status_dot.create_oval(1, 1, 9, 9, fill=colour, outline=colour)
        self.status.config(
            text=f"Idle-sleep is {state}, after {fmt_timeout(cfg.idle_minutes * 60)}."
                 f"{deep}{who} Idle detection: {backend}.{warn}")

    def _test(self):
        cfg = self.app.cfg
        self.status.config(text="Testing: turning your screen off, then on…")

        def worker():
            from .recovery import connect_tv
            ok, err = True, ""
            client = None
            try:
                # connect_tv heals a stale IP (DHCP moved the TV) before testing.
                client = connect_tv(cfg, log=lambda _m: None)
                client.screen_off()
                import time
                time.sleep(2)
                client.screen_on()
            except Exception as exc:  # noqa: BLE001
                ok, err = False, str(exc)
            finally:
                if client is not None:
                    client.close()
            self.app.post(lambda: self._test_done(ok, err))

        threading.Thread(target=worker, daemon=True).start()

    def _test_done(self, ok, err):
        if ok:
            # cfg.device.ip may have just been corrected by the recovery step.
            self._refresh_conn_label()
            self.status.config(
                text=f"Test OK — your TV responded at {self.app.cfg.device.ip}. ✓")
            self._refresh_status()
        else:
            # Don't dead-end on the raw error (the reported bug): open a repair
            # session that diagnoses and fixes it - relocating the TV, reconnecting
            # and blinking the screen - with the full details shown live.
            self.status.config(
                text=f"Couldn't reach your TV ({err}). Starting repair…")
            RepairDialog(self.app, self)

    # ----- connection self-test / repair ------------------------------
    def _conn_text(self) -> str:
        cfg = self.app.cfg
        return f"Connected to  {cfg.device.name}  ·  {cfg.device.ip}"

    def _refresh_conn_label(self):
        """Re-render the 'Connected to … · IP' line (the IP can change on repair)."""
        label = getattr(self, "_conn_label", None)
        if label is not None:
            try:
                label.config(text=self._conn_text())
            except tk.TclError:
                pass

    def _kickoff_selftest(self):
        """On startup, quietly verify the TV is reachable and self-heal if not.

        A fast TCP health check decides whether anything is wrong; only if it is
        do we run a background repair (relocate by MAC/discovery and persist the
        corrected address - no screen blink, so it's invisible when all is well).
        Gated by LGTV_EASY_NO_SELFTEST so tests and headless CI stay hermetic.
        """
        import os
        if os.environ.get("LGTV_EASY_NO_SELFTEST") == "1":
            return
        if not self.app.cfg.device.paired:
            return

        def worker():
            from . import selfheal
            cfg = self.app.cfg
            try:
                if selfheal.quick_health_check(cfg):
                    self.app.post(self._refresh_status)
                    return
                res = selfheal.repair(cfg, connect=False, blink=False)
            except Exception:  # noqa: BLE001 - a self-test must never crash the app
                return
            self.app.post(lambda: self._selftest_done(res))

        threading.Thread(target=worker, daemon=True).start()

    def _selftest_done(self, res):
        self._refresh_conn_label()
        if res.repaired and res.ok:
            self.status.config(
                text=f"Reconnected — your TV had moved to {self.app.cfg.device.ip}. ✓")
        elif not res.ok:
            # Unreachable at startup: say so plainly and point at the repair button.
            self.status.config(
                text=f"{res.summary}  (Press “Test my TV” to run a full repair.)")
        else:
            self._refresh_status()


class RepairDialog(tk.Toplevel):
    """A live 'repair session' window opened when the TV can't be reached.

    Runs :func:`selfheal.repair` in a worker thread - probing the network,
    relocating the TV by MAC/discovery, reconnecting and blinking the screen -
    and narrates every step into a scrollable log, ending with a clear outcome
    and a 'Try again' button. On success it refreshes the parent panel, whose
    saved address may have just been corrected.
    """

    def __init__(self, app: App, panel: "SettingsPanel"):
        super().__init__(app)
        self.app = app
        self.panel = panel
        self._running = False
        self.title("Repair TV connection")
        self.configure(bg=THEME["bg"])
        self.geometry("520x470")
        self.minsize(460, 400)
        try:
            self.transient(app)
        except tk.TclError:
            pass
        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self._build()
        self.start()

    def _build(self):
        frame = ttk.Frame(self, padding=(PAD + 4, PAD, PAD + 4, PAD + 4))
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="Repairing the TV connection",
                  style="Title.TLabel").pack(anchor="w")
        self.status = ttk.Label(frame, text="Looking for your TV…",
                                style="Sub.TLabel", wraplength=460, justify="left")
        self.status.pack(anchor="w", pady=(6, PAD))
        self.progress = ttk.Progressbar(frame, mode="indeterminate")
        self.progress.pack(fill="x")
        ttk.Label(frame, text="Details", style="Sub.TLabel").pack(
            anchor="w", pady=(PAD, 0))
        self.diag = make_diag(self.app, frame, height=8)
        nav = ttk.Frame(frame)
        nav.pack(fill="x", pady=(PAD, 0))
        self.close_btn = ttk.Button(nav, text="Close", style="Ghost.TButton",
                                    command=self._on_close)
        self.close_btn.pack(side="right")
        self.retry_btn = ttk.Button(nav, text="Try again", style="Accent.TButton",
                                    command=self.start)
        self.retry_btn.pack(side="right", padx=(0, 6))
        self.retry_btn.state(["disabled"])

    def start(self):
        if self._running:
            return
        self._running = True
        try:
            self.retry_btn.state(["disabled"])
            self.status.config(text="Looking for your TV…")
            self.progress.start(12)
        except tk.TclError:
            pass
        diag = self.diag

        def worker():
            from . import selfheal
            res = selfheal.repair(
                self.app.cfg, log=diag, connect=True, blink=True,
                on_prompt=lambda: self.app.post(self._on_prompt),
                prompt_timeout=20.0)
            if res.client is not None:
                try:
                    res.client.close()
                except Exception:  # noqa: BLE001
                    pass
            self.app.post(lambda: self._done(res))

        threading.Thread(target=worker, daemon=True).start()

    def _on_prompt(self):
        try:
            self.status.config(
                text="👉  Look at your TV and press OK / Accept on the pairing "
                     "prompt with the remote.")
        except tk.TclError:
            pass

    def _done(self, res):
        self._running = False
        try:
            self.progress.stop()
            self.status.config(text=res.summary)
            self.retry_btn.state(["!disabled"])
        except tk.TclError:
            pass
        # Reflect the result on the parent panel (the IP may have moved).
        try:
            self.panel._refresh_conn_label()
            self.panel.status.config(text=res.summary)
        except tk.TclError:
            pass

    def _on_close(self):
        # The worker may still be mid-connect; its UI posts no-op against a
        # destroyed window (every callback is TclError-guarded), so closing now
        # is safe - the diagnostics simply stop updating.
        try:
            self.progress.stop()
        except tk.TclError:
            pass
        self.destroy()


def main() -> int:
    app = App()
    app.protocol("WM_DELETE_WINDOW", app.on_close)
    app.mainloop()
    return 0


if __name__ == "__main__":
    main()
