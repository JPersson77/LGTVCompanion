# LGTV Companion — Easy Mode

**Make your LG OLED TV sleep like a normal PC monitor.** The screen turns off
after a few minutes of inactivity and wakes the instant you move the mouse or
press a key — saving power and preventing burn-in.

That is the whole app. One job, almost no configuration, a simple window.

It runs on **Windows and Ubuntu/Linux**, needs no third-party packages (pure
Python standard library; the graphical window uses `tkinter`, which ships with
Python), and speaks the same WebOS protocol as the original LGTV Companion.

---

## Get started — just run the installer for your system

Download this repository (green **Code ▸ Download ZIP** button, then unzip), and:

| Your computer | What to do |
|---------------|------------|
| **Windows** | Double-click **`LGTV-Easy-Mode-WINDOWS.bat`** |
| **Ubuntu / Linux** | Run **`./LGTV-Easy-Mode-UBUNTU.sh`** in a terminal |

The launcher is a self-contained portable installer. The first time you run it,
it:

1. installs what it needs (Git + Python; `tkinter` for the window),
2. downloads the app and keeps itself up to date from GitHub,
3. opens a **simple setup window**, and
4. keeps your TV sleeping in the background — even after you close the window.

Closing the window does **not** stop it. To stop the background watcher:

```text
Windows:  LGTV-Easy-Mode-WINDOWS.bat   →  or run the .ps1 with  -Stop
Linux:    ./LGTV-Easy-Mode-UBUNTU.sh --stop
```

---

## The setup window (3 steps)

1. **Find your TV** — click **Scan** (or type its IP). It appears in a list.
2. **Pair** — press **OK / Accept** on the prompt that pops up on the TV.
3. **Timeout** — drag the slider to choose the idle minutes before sleep.
   **7 minutes is a good default.**

After that, the everyday screen is a single Windows-style panel: a big
**“Turn the screen off when I’m away”** switch, a **minutes** slider, optional
**mute the speakers when sleeping**, an optional **Maximum energy saving** box
that fully powers the TV off after a longer idle (waking it again via
Wake-on-LAN), a **Test my TV** button, and **Start automatically when I log in**.

If there’s no graphical desktop (e.g. a server over SSH), the same steps run as
a friendly text wizard instead — the launcher falls back automatically.

---

## Works with a Google/Nest Wifi mesh

A TV on a Google Wifi mesh and a PC on the same mesh (by Ethernet **or** Wi-Fi)
work fine, as long as both are on the **same network** (not a separate “guest”
network or a double-NAT). The setup window shows your PC’s network and warns you
if the TV looks like it’s on a different one. On the TV, enable
**“Turn on via Wi-Fi”** (a.k.a. “Quick Start+” / “Always Ready”) so it can be
woken over the network — needed for Wi-Fi *and* Ethernet.

---

## Using it from the command line (optional)

Everything the window does is also scriptable:

```text
lgtv-easy gui                  # open the graphical control panel
lgtv-easy scan                 # discover LG TVs on the network
lgtv-easy pair 192.168.1.50    # pair with a TV by IP (accept on the TV)
lgtv-easy set --minutes 7      # blank the screen after 7 minutes idle
lgtv-easy status               # show settings + which idle backend is active
lgtv-easy test                 # blink the screen off/on to confirm it works
lgtv-easy run                  # run the idle watcher in the foreground
```

(Without an installed console script, use `python3 -m lgtv_easy <command>` from
the `EasyMode/` folder.)

## Where it keeps things

| | Windows | Linux |
|--|---------|-------|
| Settings | `%APPDATA%\LGTV Companion Easy Mode\config.json` | `~/.config/lgtv-companion-easy/config.json` |
| Launcher log | `…\launcher.log` | `…/launcher.log` |
| Watcher activity log | `…\easy-mode.log` | `…/easy-mode.log` |
| Watcher raw output | `…\watcher.log` | (folded into `launcher.log`) |

If something doesn’t work, the launcher keeps its window open and writes a
persistent `launcher.log` you can read or share.

## A note on auto-update

The launcher pulls the latest code from GitHub over HTTPS and runs it (and
re-checks hourly). That means whoever controls the source repo can run code on
your machine at your user privilege the next time it updates — the normal
trade-off for any self-updater. To freeze to the code already on disk, set
`LGTV_EASY_NO_UPDATE=1` before launching.

## Develop / test

```bash
cd EasyMode
python3 -m pytest                      # unit + integration tests (no TV needed)
python3 tests/simulate_session.py      # live end-to-end run against a mock TV
xvfb-run python3 tests/gui_smoke.py     # headless GUI test
```

A built-in mock WebOS TV (`lgtv_easy/mock_tv.py`) exercises the whole flow —
discovery, pairing, idle-sleep, wake — without any real hardware.

## Credits & license

Easy Mode is a beginner-friendly front end to
[LGTV Companion](https://github.com/JPersson77/LGTVCompanion) by Jörgen Persson,
and reuses its WebOS protocol so the two are compatible. Released under the MIT
License — see [LICENSE](LICENSE).
