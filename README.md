# LGTV Linux Companion

A Linux port of **[LGTV Companion](https://github.com/JPersson77/LGTVCompanion)** by
**Jörgen Persson** — free software that controls LG webOS TVs used as PC displays,
turning them off and on in response to system power events to help prevent OLED
pixel wear.

> This is a community fork. All credit for the original application, the reverse
> engineered webOS protocol work and the design belongs to Jörgen Persson and the
> upstream contributors. Please consider
> [supporting the original author](https://www.paypal.me/jpersson77).

---

## Status

**Work in progress.** The portable core is ported and tested; the daemon, CLI and
Qt user interface are still being built.

| Component | State |
|---|---|
| Portable core (device model, webOS API, config, logging, IPC) | ✅ Ported, builds clean, smoke tested |
| Background daemon (logind power events, WOL, Wayland idle) | 🚧 In progress |
| Command line interface | ⏳ Planned |
| Qt user interface | ⏳ Planned |

## Differences from the Windows original

This is a Linux-only fork; the Windows code has been removed rather than kept
behind `#ifdef` guards.

- **Wayland only.** X11 is out of scope.
- **One `systemd --user` service** replaces the Windows SYSTEM service plus
  per-user desktop daemon. No root is required to install or run it.
- **logind over D-Bus** replaces the Windows service control handler for suspend,
  resume and shutdown notification. A side benefit: logind reports shutdown versus
  reboot directly, so the original's localised event-log word dictionary — which
  required non-English users to hand-configure "restart words" — is not needed.
- **Unix domain socket** replaces the Windows named pipe for IPC; messages are
  newline delimited UTF-8 rather than UTF-16.
- **XDG paths**: `~/.config/lgtv-companion/config.json`,
  `~/.local/state/lgtv-companion/log.txt`.
- The `config.json` schema is unchanged and round-trips with the Windows build.
- **Not carried over:** the ARP-override wake-on-LAN fallback (needs
  `CAP_NET_ADMIN`), fullscreen detection (no portable Wayland API), and the
  auto-updater (use your distribution's package manager).

## Building

Dependencies: a C++17 compiler, CMake ≥ 3.20, Boost (headers), OpenSSL,
nlohmann-json, and Qt 6 for the user interface.

On Arch:

```bash
sudo pacman -S --needed base-devel cmake boost nlohmann-json openssl qt6-base
```

Then:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

Run the tests:

```bash
ctest --test-dir build --output-on-failure
```

## Prerequisites on the TV

Unchanged from upstream, and still essential:

- Enable **"TV On With Mobile" → "Turn on via Wi-Fi"** on the TV. This is required
  whether you use Wi-Fi or Ethernet.
- Give the TV a static DHCP lease on your router.
- The TV must be on the same subnet as the PC — wake-on-LAN magic packets do not
  cross subnets.

See the [upstream documentation](https://github.com/JPersson77/LGTVCompanion) for
the full setup guide and troubleshooting.

## License

MIT. See [LICENSE](LICENSE).

Copyright © 2021-2026 Jörgen Persson

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in the
Software without restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Thanks to

Carried over from upstream, with thanks:

- Jörgen Persson — original author of LGTV Companion
- Boost libs — Boost and Beast https://www.boost.org/
- @nlohmann — Niels Lohmann, author of JSON for Modern CPP https://github.com/nlohmann/json
- @chros73 — for thorough documentation of the api https://github.com/chros73
- @Maassoft — for initial help with understanding the WebOS comms https://github.com/Maassoft
- OpenSSL — https://www.openssl.org/
- Upstream contributors, donors and supporters
