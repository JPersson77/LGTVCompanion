# `EasyMode/` — the app

This folder is the LGTV Companion **Easy Mode** application: a small, pure
standard-library Python package (`lgtv_easy`) that makes an LG OLED TV sleep
when the PC is idle - or when the PC itself sleeps - and wake on input.

**Most people don't run anything in here directly.** Use the portable installer
at the repository root instead — `Windows Launch.bat` (Windows) or
`Linux Launch.sh` (Linux). See the top-level [`readme.txt`](../readme.txt).

## Run it directly (developers)

```bash
cd EasyMode
python3 -m lgtv_easy            # open the graphical control panel
python3 -m lgtv_easy --help     # all commands (scan, pair, set, status, test, repair, run, ...)
python3 -m lgtv_easy repair     # self-test the TV connection and auto-fix a moved/unreachable TV
```

## Layout

| Module | Responsibility |
|--------|----------------|
| `gui.py` | tkinter setup wizard + settings panel (the front door) |
| `wizard_text.py` | the text-mode wizard (fallback when there's no display) |
| `daemon.py` | the idle-watching loop that blanks/wakes the TV |
| `idle.py` | cross-platform "seconds since last input" detection |
| `system_sleep.py` | detect whole-PC suspend/resume so the TV follows it to sleep |
| `webos.py` / `_ws.py` | the WebOS WebSocket protocol + pairing |
| `discovery.py` | finding TVs on the network (SSDP) |
| `recovery.py` | quick on-demand reconnect, healing a stale IP by MAC |
| `selfheal.py` | the escalating self-test + repair engine (startup check, "Test my TV", `repair`) |
| `wol.py` | Wake-on-LAN magic packets |
| `netdiag.py` | network diagnostics (incl. the Google/Nest Wifi hint) |
| `config.py` | the tiny JSON settings file |
| `autostart.py` | start-at-login registration |
| `singleton.py` | one-watcher-at-a-time lock |
| `LGTV-Easy-Mode-WINDOWS.ps1` | the Windows launcher engine the root `.bat` runs |

## Tests

```bash
python3 -m pytest                    # unit + integration (uses a built-in mock TV)
python3 tests/simulate_session.py    # live end-to-end run against the mock TV
xvfb-run python3 tests/gui_smoke.py   # headless GUI test
```
