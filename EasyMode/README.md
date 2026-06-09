# LGTV Companion — Easy Mode

**Make your LG OLED TV behave like a normal PC monitor: the screen turns off
after a few minutes of inactivity and wakes the instant you touch the mouse or
keyboard.**

This is the *easy* front-end to LGTV Companion, built for one job and almost no
configuration. If you use an LG B-/C-/G-series OLED as your monitor and just
want it to sleep when you step away (to save power and prevent burn-in), this is
for you.

It is cross-platform (Windows **and** Ubuntu/Linux), has **zero third-party
runtime dependencies** (pure Python standard library; the optional GUI uses
`tkinter`, which ships with Python), and speaks the exact same WebOS protocol as
the original LGTV Companion, so the two are compatible.

---

## The fastest way to use it

The launchers live in the **repository root** with obvious names — you can't
miss them:

| Platform | File (at the repo root) |
|----------|-------------------------|
| Windows  | `LGTV-Easy-Mode-WINDOWS.bat` (double-click) — uses `LGTV-Easy-Mode-WINDOWS.ps1` |
| Ubuntu/Linux | `LGTV-Easy-Mode-UBUNTU.sh` |

### Windows
1. Download this repository.
2. **Double-click `LGTV-Easy-Mode-WINDOWS.bat`.**

That's it. The launcher installs Git and Python if needed, downloads the app,
runs a 3-step setup wizard, and then keeps your TV sleeping in the background —
restarting itself and pulling updates automatically from the default branch.

### Ubuntu / Linux
```bash
chmod +x LGTV-Easy-Mode-UBUNTU.sh
./LGTV-Easy-Mode-UBUNTU.sh            # set up, then run in the foreground
# or run it detached in the background:
./LGTV-Easy-Mode-UBUNTU.sh --background
```

The first run opens the setup wizard (graphical if a desktop is available,
otherwise a friendly text wizard). After that it just works.

---

## The setup wizard (3 steps)

| Step | What you do |
|------|-------------|
| **1. Find your TV** | Click **Scan** (or type the IP). Your TV appears in a list. |
| **2. Pair** | Press **OK** on the pairing prompt that pops up on the TV. |
| **3. Timeout** | Drag the slider to choose how many minutes of inactivity before the screen sleeps. 7 minutes is a good default. |

Then the everyday window is a single, Windows-style panel:

- a big **“Turn the screen off when I’m away”** switch,
- a **minutes** slider,
- an optional **“mute the speakers when sleeping”** checkbox,
- a **“Maximum energy saving”** box to **fully power the TV off** after a longer
  idle (waking it again via Wake-on-LAN), and
- a **Test my TV** button, and a **Re-run setup** button.

Screenshots of all three screens live in `docs/` of the project discussion.

---

## Using it from the command line

Everything the GUI does is also available headless:

```bash
lgtv-easy scan                 # discover LG TVs on the network
lgtv-easy pair 192.168.1.50    # pair with a TV by IP (accept on the TV)
lgtv-easy set --minutes 7      # blank the screen after 7 minutes idle
lgtv-easy set --enabled false  # temporarily disable
lgtv-easy set --mute true      # mute the TV speakers while the screen is off
lgtv-easy set --deep-off true --deep-off-minutes 30   # full power-off after 30 min
lgtv-easy status               # show current settings + idle detection backend
lgtv-easy test                 # blink the screen off/on to confirm it works
lgtv-easy run                  # run the idle-monitoring daemon in the foreground
lgtv-easy wizard               # interactive setup / settings wizard
lgtv-easy autostart enable     # start the watcher automatically at login
lgtv-easy autostart disable    # stop starting it at login
```

(Without an installed console script, use `python3 -m lgtv_easy <command>`.)

---

## How it works

- **Idle detection** uses the OS input timer: `GetLastInputInfo` on Windows;
  `xprintidle` or the X ScreenSaver extension on Linux. `lgtv-easy status` shows
  which backend is active.
- When you cross the timeout, it sends the WebOS `turnOffScreen` command (an
  OLED-friendly screen blank, not a full power-off). Any keypress/mouse move
  resets the OS idle timer, and the daemon sends `turnOnScreen`.
- **Two-stage energy saving (optional).** With “deep off” enabled, after a longer
  idle the daemon fully powers the TV off (`system/turnOff`, true ~0.5W standby)
  instead of just blanking it. This is like a monitor sleeping and then the PC
  powering down: the screen blanks first (instant wake, keeps the HDMI link so
  Windows doesn’t rearrange), then the TV switches off for the lowest power use.
- **Waking from full power-off** uses a Wake-on-LAN magic packet, so enable the
  TV’s “Turn on via Wi-Fi” / “Quick Start+” setting. The TV’s MAC is detected
  automatically at pairing (from the ARP table); set it manually if needed with
  `lgtv-easy set --mac AA:BB:CC:DD:EE:FF`.
- **Start at login** (the wizard asks, or `lgtv-easy autostart enable`) registers
  a per-user entry - a Startup-folder shortcut on Windows (run with `pythonw`, no
  console window), a `~/.config/autostart` desktop entry on Linux - that launches
  the watcher quietly each time you log in.
- **Only one watcher runs at a time.** A pidfile lock means the login auto-start,
  the supervising launcher, and a manual `run` never fight over the TV.

## The self-updating launchers

`LGTV-Easy-Mode-UBUNTU.sh` (Linux) and `LGTV-Easy-Mode-WINDOWS.ps1` /
`LGTV-Easy-Mode-WINDOWS.bat` (Windows) are supervisors that:

1. install dependencies,
2. clone/update the app from GitHub's **default branch** — **including updating
   the launcher script itself** (it re-executes the new version when a pull
   rewrites it),
3. run setup on first use, and
4. keep the idle daemon alive in the background, restarting it on crashes and
   checking for updates hourly.

All launcher and daemon activity is appended to a persistent log:

- Linux: `~/.config/lgtv-companion-easy/launcher.log`
- Windows: `%APPDATA%\LGTV Companion Easy Mode\launcher.log`

Stop a background supervisor with `./LGTV-Easy-Mode-UBUNTU.sh --stop`
(or `LGTV-Easy-Mode-WINDOWS.ps1 -Stop`).

## Files & settings

- Config (JSON): `~/.config/lgtv-companion-easy/config.json` (Linux) or
  `%APPDATA%\LGTV Companion Easy Mode\config.json` (Windows).
- Daemon log: `easy-mode.log` in the same folder.

## Development & tests

```bash
cd EasyMode
python3 -m pytest                 # unit + integration tests (no TV needed)
python3 tests/simulate_session.py # live end-to-end simulation against a mock TV
xvfb-run python3 tests/gui_smoke.py   # headless GUI smoke test
```

The tests include a built-in **mock WebOS TV** (`lgtv_easy/mock_tv.py`) so the
whole flow — discovery, pairing, idle-sleep, wake — is verified without any real
hardware.

## Compatibility with the original app

This shares the WebOS protocol, ports and pairing manifest with the original
Windows C++ LGTV Companion. You can run either; they store their settings
separately. Easy Mode focuses on the single most-requested monitor use case
(sleep when idle) with a tiny, approachable interface.
