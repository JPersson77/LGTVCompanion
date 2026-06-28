"""The idle-monitoring daemon: the heart of Easy Mode.

Every ``poll_seconds`` it asks the OS how long the user has been idle. Cross the
configured threshold and the TV screen is blanked; touch the keyboard or mouse
and it comes straight back on. That is the entire job.

The loop is written with injectable dependencies (idle source, client factory,
clock, stop event) so the whole behaviour can be stepped deterministically in
tests without a real TV or a real wait.
"""
from __future__ import annotations

import os
import threading
import time
from typing import Callable, Optional

from . import idle as idle_mod
from . import system_sleep
from .applog import get_logger
from .config import Config, fmt_timeout
from .webos import WebOSClient
from .wol import send_wol

# Screen state as tracked by the daemon.
STATE_ON = "on"
STATE_OFF = "off"          # panel blanked, TV still powered and on the network
STATE_STANDBY = "standby"  # TV fully powered off (deep energy saving)

# If a single poll iteration takes far longer in wall-clock time than we asked it
# to sleep, the process was frozen - i.e. the machine suspended and just resumed.
# This is the OS-API-free resume backstop for when no sleep watcher caught it.
RESUME_GAP_SECONDS = 30.0

# When the TV can't be reached (it's off, or on a different network), don't hammer
# a reconnect on every single poll - that just burns CPU/network and floods the log
# with one identical warning per poll, forever. Instead back off exponentially from
# the poll interval up to this cap, and log the failure only once until it recovers.
RECONNECT_BACKOFF_MAX = 300.0  # seconds (5 min between attempts at most)

# How long to keep trying to wake a fully-powered-off TV (with Wake-on-LAN) on
# user activity before concluding WoL can't reach it on this network - e.g. a
# Wi-Fi TV behind a mesh router that won't forward magic packets to a sleeping
# client. A real WoL wake completes in well under this; past it we disable deep
# power-off so the TV is never left unreachable again (screen-off, which needs
# no WoL, stays on). Generous so a merely slow wake never trips it.
DEEP_WAKE_GIVEUP_SECONDS = 90.0


class Daemon:
    def __init__(
        self,
        config: Config,
        client_factory: Optional[Callable[[], WebOSClient]] = None,
        idle_fn: Optional[Callable[[], float]] = None,
        sleep_fn: Optional[Callable[[float], None]] = None,
        clock_fn: Optional[Callable[[], float]] = None,
        sleep_watcher_factory: Optional[Callable[..., object]] = None,
        locator_fn: Optional[Callable[[str], Optional[str]]] = None,
        wol_fn: Optional[Callable[[bool], None]] = None,
        logger=None,
    ):
        self.config = config
        self.logger = logger or get_logger()
        self._idle_fn = idle_fn or idle_mod.get_idle_seconds
        # Fire Wake-on-LAN to wake the TV. Injectable so tests don't broadcast
        # real packets (or sleep through a sustained burst). Takes one arg: True
        # when waking from full standby (sustained burst), False for screen-off.
        self._wol_fn = wol_fn or self._default_wol
        # The default poll-sleep waits on an event so a settings change can cut
        # it short and apply at once (see _interruptible_sleep). Tests inject a
        # deterministic sleep_fn and bypass this.
        self._sleep_fn = sleep_fn or self._interruptible_sleep
        self._clock = clock_fn or time.monotonic
        self._client_factory = client_factory or self._default_client_factory
        self._sleep_watcher_factory = sleep_watcher_factory or system_sleep.make_watcher
        # Given the TV's MAC, return its current IP on the LAN (or None). Used to
        # recover automatically when DHCP moves the TV to a new address.
        self._locator_fn = locator_fn or self._default_locator
        self._sleep_watcher: Optional[object] = None
        self._client: Optional[WebOSClient] = None
        self.screen_state = STATE_ON  # assume the screen is on at startup
        self._stop = threading.Event()
        # Set to wake the poll-sleep early (a settings change came in, or we're
        # stopping). _reload_pending asks the loop to re-read config from disk.
        self._wake = threading.Event()
        self._reload_pending = False
        # Serialise the TV actions: the idle loop and the suspend/resume watcher
        # both drive the screen, from different threads.
        self._action_lock = threading.Lock()
        self._thread: Optional[threading.Thread] = None
        # Counters make tests and the status command observable.
        self.sleeps = 0
        self.wakes = 0
        self.deep_offs = 0
        self.last_error = ""
        self._warned_no_wol = False
        # Reconnect backoff state, so an unreachable TV doesn't cause a per-poll
        # connect storm (see RECONNECT_BACKOFF_MAX). All touched only from the
        # action lock or the single daemon thread.
        self._connect_failures = 0       # consecutive failed connection attempts
        self._next_connect_at = 0.0      # monotonic time before which we don't retry
        self._connect_warned = False     # have we logged the current outage yet?
        # Auto-relocate state: the heavier "find the TV by MAC" sweep is rate
        # limited separately so it can't run on every failed poll.
        self._next_relocate_at = 0.0
        self.relocations = 0             # times we adopted a new IP (observable)
        # Deep-off recovery: track how long we've been failing to wake the TV
        # from full standby so we can give up (and disable deep-off) rather than
        # strand it forever if WoL can't reach it. None = not currently failing.
        self._standby_wake_since: Optional[float] = None
        self._gave_up_deep_wake = False

    # ----- TV connection ----------------------------------------------
    def _default_client_factory(self) -> WebOSClient:
        return WebOSClient(self.config.device.ip, secure=self.config.device.secure)

    def _default_locator(self, mac: str) -> Optional[str]:
        from .discovery import locate_tv
        return locate_tv(
            mac, log=lambda m: self.logger.debug("relocate: %s", m))

    def _ensure_client(self, force: bool = False) -> Optional[WebOSClient]:
        if self._client and self._client.connected:
            return self._client
        # An unreachable TV (off, or on another network) must not trigger a fresh
        # connect on every poll. While inside the backoff window, skip the attempt
        # entirely - unless this is a user-facing wake (force), which should always
        # try so the screen comes back promptly.
        if (not force and self._connect_failures
                and self._clock() < self._next_connect_at):
            return None
        try:
            return self._connect_once()
        except Exception as exc:  # noqa: BLE001 - network errors are expected
            # The saved IP didn't answer. DHCP routinely re-addresses the TV, so
            # before giving up, try to find it again (by its unchanging MAC, or
            # by discovery) and retry at the new address if it has moved.
            if self._relocate(force):
                try:
                    return self._connect_once()
                except Exception as exc2:  # noqa: BLE001
                    exc = exc2
            self.last_error = f"connect: {exc}"
            self._note_connect_failure(exc)
            self._client = None
            return None

    def _connect_once(self) -> WebOSClient:
        """Open and register one connection to the TV at the configured IP.

        Raises on any failure; on success persists anything newly learned about
        the TV (the working port, its MAC) and clears the reconnect backoff.
        """
        client = self._client_factory()
        # Try the port the TV actually accepts (3000 vs secure 3001),
        # preferring whichever worked before. Newer panels only allow 3001.
        from .webos import pair_with_fallback
        pair_with_fallback(client, client_key=self.config.device.key,
                           on_prompt=None, prompt_timeout=client.timeout,
                           prefer_secure=self.config.device.secure)
        # Remember (and persist) what we learned about the TV: the port that
        # worked, and its MAC (asked straight from the TV) for Wake-on-LAN.
        changed = False
        if client.secure != self.config.device.secure:
            self.config.device.secure = client.secure
            changed = True
        if not self.config.device.mac:
            mac = client.get_mac()
            if not mac:
                from .netdiag import mac_for_ip
                host = (client.ip.rpartition(":")[0]
                        if ":" in client.ip else client.ip)
                mac = mac_for_ip(host)
            if mac:
                self.config.device.mac = mac
                changed = True
                self.logger.info("Detected TV MAC for Wake-on-LAN: %s", mac)
        if changed:
            try:
                self.config.save()
            except Exception:  # noqa: BLE001 - persistence is best-effort
                pass
        if self._connect_failures:
            self.logger.info("Reconnected to the TV after %d failed "
                             "attempt(s).", self._connect_failures)
        self._connect_failures = 0
        self._connect_warned = False
        self._next_connect_at = 0.0
        self._client = client
        return client

    def _relocate(self, force: bool = False) -> bool:
        """Find the TV again and adopt a new IP if it has moved.

        This is the automatic "no TV present -> fix it" recovery. We only get
        here after a connect to the saved IP has just failed, so it is safe to
        run eagerly - including at startup and on the very first failure - which
        is what makes recovery feel seamless. The only throttle is a cooldown so
        a TV that is genuinely off (and so can't be found) doesn't trigger a LAN
        scan on every retry. Returns True (and updates/persists ``device.ip``)
        when the address changed, so the caller should retry the connection.
        """
        if not self._locator_fn:
            return False
        now = self._clock()
        # A user-facing wake (force) always gets to look right away; background
        # polls obey a cooldown so a TV that's genuinely off doesn't trigger a
        # LAN scan on every retry. The cooldown tracks the connect backoff.
        if not force and now < self._next_relocate_at:
            return False
        self._next_relocate_at = now + max(self.config.poll_seconds, 30.0)
        mac = self.config.device.mac
        try:
            # An empty MAC is fine: the locator falls back to adopting the only
            # LG TV it can discover, and we learn the MAC on the next connect.
            new_ip = self._locator_fn(mac)
        except Exception as exc:  # noqa: BLE001 - locating is best-effort
            self.logger.debug("Relocate failed: %s", exc)
            return False
        if not new_ip:
            return False
        host = new_ip.rpartition(":")[0] if ":" in new_ip else new_ip
        old = self.config.device.ip
        if host == old:
            return False
        how = f"by MAC {mac}" if mac else "by discovery"
        self.logger.info("TV not reachable at %s; found it at %s %s - "
                         "updating the saved address.", old or "(unset)", host, how)
        self.config.device.ip = host
        self._drop_client()
        try:
            self.config.save()
        except Exception:  # noqa: BLE001 - persistence is best-effort
            pass
        self.relocations += 1
        return True

    def _note_connect_failure(self, exc: Exception) -> None:
        """Record a failed connection: schedule the next retry with exponential
        backoff (capped at RECONNECT_BACKOFF_MAX) and log the outage only once,
        so an off TV doesn't flood the log with one identical line per poll."""
        self._connect_failures += 1
        base = max(self.config.poll_seconds, 1.0)
        # 1st failure -> base interval, doubling each time up to the cap.
        delay = min(RECONNECT_BACKOFF_MAX,
                    base * (2 ** min(self._connect_failures - 1, 16)))
        self._next_connect_at = self._clock() + delay
        if not self._connect_warned:
            self._connect_warned = True
            self.logger.warning(
                "Could not connect to TV: %s. It may be off or on another "
                "network - retrying quietly (next attempt in ~%.0fs).",
                exc, delay)
        else:
            self.logger.debug("TV still unreachable (%s); next attempt in ~%.0fs.",
                              exc, delay)

    def _drop_client(self) -> None:
        if self._client:
            try:
                self._client.close()
            except Exception:  # noqa: BLE001
                pass
        self._client = None

    # ----- actions -----------------------------------------------------
    def sleep_screen(self) -> bool:
        with self._action_lock:
            client = self._ensure_client()
            if not client:
                return False
            try:
                client.screen_off()
                if self.config.mute_on_sleep:
                    client.set_mute(True)
                self.screen_state = STATE_OFF
                self.sleeps += 1
                self.logger.info("Screen off after %s idle",
                                 fmt_timeout(self.config.idle_seconds))
                return True
            except Exception as exc:  # noqa: BLE001
                self.last_error = f"sleep: {exc}"
                self.logger.warning("Failed to turn screen off: %s", exc)
                self._drop_client()
                return False

    def power_off_tv(self) -> bool:
        """Fully power the TV off (deep standby) for maximum energy saving."""
        with self._action_lock:
            client = self._ensure_client()
            if not client:
                return False
            try:
                client.power_off()
                self.screen_state = STATE_STANDBY
                # Fresh standby: reset the wake-failure window, and re-arm the
                # give-up safety net (so re-enabling deep-off gets a clean chance).
                self._standby_wake_since = None
                self._gave_up_deep_wake = False
                self.deep_offs += 1
                self.logger.info("TV powered off (deep energy saving) after %s idle",
                                 fmt_timeout(self.config.deep_off_seconds))
                # The socket dies as the TV powers down; reconnect on next wake.
                self._drop_client()
                return True
            except Exception as exc:  # noqa: BLE001
                self.last_error = f"power_off: {exc}"
                self.logger.warning("Failed to power off TV: %s", exc)
                self._drop_client()
                return False

    def _default_wol(self, deep: bool) -> None:
        """Fire Wake-on-LAN to bring the TV back.

        Aim the magic packet at the limited broadcast, the TV's directed subnet
        broadcast, and a unicast to its last-known IP, so it lands across a
        Google/Nest Wifi mesh (where the limited broadcast isn't always forwarded
        between wired and wireless segments, and a sleeping Wi-Fi client may only
        wake on a directed packet). From full standby (``deep``) send a sustained
        burst over a few seconds - a single blip is dropped before a sleeping TV's
        radio sees it; a screen-off TV is already awake on the LAN so a light
        nudge suffices.
        """
        mac = self.config.device.mac
        if not mac:
            return
        from .wol import wake_targets
        targets = wake_targets(self.config.device.ip)
        if deep:
            send_wol(mac, broadcast=targets, repeat=20, interval=0.25)
        else:
            send_wol(mac, broadcast=targets)

    def wake_screen(self) -> bool:
        with self._action_lock:
            # If the panel went into full standby it needs a magic packet to come
            # back; a sustained burst when deep so a sleeping Wi-Fi/mesh TV
            # actually receives one.
            try:
                self._wol_fn(self.screen_state == STATE_STANDBY)
            except Exception as exc:  # noqa: BLE001
                self.logger.debug("WOL send failed (often harmless): %s", exc)
            # A wake is user-facing and time-sensitive: bypass the reconnect
            # backoff so the screen returns as soon as the TV is reachable.
            client = self._ensure_client(force=True)
            if not client:
                return False
            try:
                client.screen_on()
                if self.config.mute_on_sleep:
                    client.set_mute(False)
                self.screen_state = STATE_ON
                self.wakes += 1
                self.logger.info("Screen on (activity detected)")
                return True
            except Exception as exc:  # noqa: BLE001
                self.last_error = f"wake: {exc}"
                self.logger.warning("Failed to turn screen on: %s", exc)
                self._drop_client()
                return False

    # ----- following the PC into sleep ---------------------------------
    def _on_system_sleep(self) -> None:
        """The PC is suspending: blank the TV so it doesn't sit there lit.

        Runs on the sleep-watcher thread, just before the machine goes down.
        """
        if not self.config.screen_off_on_pc_sleep:
            return
        if self.screen_state in (STATE_OFF, STATE_STANDBY):
            return  # already dark - nothing to do
        self.logger.info("PC is going to sleep; turning the TV screen off.")
        self.sleep_screen()

    def _on_system_resume(self) -> None:
        """The PC woke up. Bring the screen back only if a person is actually
        there (recent input); if it resumed on its own - a scheduled task, a
        Wake-on-LAN, an RTC alarm - leave the TV asleep instead of lighting an
        empty room. Genuine user wakes always involve a keypress, so the idle
        timer is near zero."""
        if not self.config.screen_off_on_pc_sleep:
            return
        if self.screen_state not in (STATE_OFF, STATE_STANDBY):
            return
        try:
            active = self._idle_fn() < self.config.idle_seconds
        except Exception:  # noqa: BLE001
            active = True
        if active:
            self.logger.info("PC resumed; turning the TV screen back on.")
            self.wake_screen()

    def _on_resume_detected(self, gap: float) -> None:
        """Backstop for resume when no OS sleep watcher caught it.

        The loop was frozen far longer than we asked it to sleep, so the whole
        machine suspended and just woke. If a person is actually back (idle near
        zero) make sure the TV is on - unlike the idle tick, this also covers the
        case where we still think the screen is ON but it went dark on its own
        (the PC slept before the idle timeout, and the panel hit its own
        standby), which the tick alone would never correct.
        """
        if not self.config.screen_off_on_pc_sleep:
            return
        try:
            active = self._idle_fn() < self.config.idle_seconds
        except Exception:  # noqa: BLE001
            active = True
        if not active:
            return  # resumed on its own (RTC/WOL/task) - don't light an empty room
        self.logger.info(
            "Resume detected (process frozen ~%.0fs); ensuring the TV is on.", gap)
        self.wake_screen()

    def _start_sleep_watcher(self) -> None:
        # Opt-out hook for tests/CI so they never spawn real OS power monitors.
        if os.environ.get("LGTV_EASY_NO_SLEEP_WATCH") == "1":
            return
        try:
            watcher = self._sleep_watcher_factory(
                on_sleep=self._on_system_sleep,
                on_resume=self._on_system_resume,
                logger=self.logger)
            watcher.start()
            self._sleep_watcher = watcher
            name = getattr(watcher, "backend_name", "?")
            if name and name != "none":
                self.logger.info("Following PC sleep (backend: %s).", name)
        except Exception as exc:  # noqa: BLE001 - the feature is best-effort
            self.logger.debug("Sleep watcher unavailable: %s", exc)
            self._sleep_watcher = None

    def _stop_sleep_watcher(self) -> None:
        watcher, self._sleep_watcher = self._sleep_watcher, None
        if watcher is not None:
            try:
                watcher.stop()
            except Exception:  # noqa: BLE001
                pass

    # ----- the loop ----------------------------------------------------
    def tick(self) -> None:
        """Evaluate idle state once and act. Safe to call from tests.

        Up to three stages, mirroring a real monitor that sleeps then lets the
        PC power down:
          ON       --(idle >= idle_minutes)-->        OFF (screen blanked)
          OFF      --(idle >= deep_off_minutes)-->     STANDBY (TV powered off)
          OFF/STANDBY --(activity)-->                  ON (wake, via WOL if off)
        """
        if not self.config.idle_enabled:
            # Disabled: make sure the screen isn't left off/standby by us.
            if self.screen_state in (STATE_OFF, STATE_STANDBY):
                self.wake_screen()
            return
        idle = self._idle_fn()
        threshold = self.config.idle_seconds
        # Deep power-off only makes sense strictly after the screen-off stage,
        # and only if we can wake the TV again (Wake-on-LAN needs its MAC) -
        # otherwise it would switch off and never come back on its own.
        deep = (self.config.deep_off_enabled
                and self.config.deep_off_seconds > threshold)
        if deep and not self.config.device.mac:
            deep = False
            if not self._warned_no_wol:
                self._warned_no_wol = True
                self.logger.warning(
                    "Deep power-off is on but no Wake-on-LAN MAC is set, so the "
                    "TV could not be woken again - skipping full power-off. Set "
                    "the MAC (lgtv-easy set --mac ..) or turn deep-off off.")
        if self.screen_state == STATE_ON and idle >= threshold:
            self.sleep_screen()
        elif (self.screen_state == STATE_OFF and deep
              and idle >= self.config.deep_off_seconds):
            self.power_off_tv()
        elif self.screen_state in (STATE_OFF, STATE_STANDBY) and idle < threshold:
            # Any input resets the OS idle timer, so this fires on wake.
            from_standby = self.screen_state == STATE_STANDBY
            if self.wake_screen():
                self._standby_wake_since = None
                self._gave_up_deep_wake = False
            elif from_standby:
                self._note_unwakeable_standby()

    def _note_unwakeable_standby(self) -> None:
        """A wake from full power-off failed. If they keep failing past the grace
        window, Wake-on-LAN can't reach this TV here - so disable deep power-off
        (once) rather than strand the TV every idle cycle. Screen-off stays on; it
        rides the live connection and never needs WoL. The user turns the TV on
        with the remote this once; we won't deep-off it again."""
        if self._gave_up_deep_wake:
            return
        now = self._clock()
        if self._standby_wake_since is None:
            self._standby_wake_since = now
            return
        if now - self._standby_wake_since < DEEP_WAKE_GIVEUP_SECONDS:
            return
        self._gave_up_deep_wake = True
        self.config.deep_off_enabled = False
        try:
            self.config.save()
        except Exception:  # noqa: BLE001 - persistence is best-effort
            pass
        self.logger.warning(
            "Full power-off could not be reversed by Wake-on-LAN after ~%.0fs - "
            "this TV/network can't be woken from deep standby (common for a Wi-Fi "
            "TV behind a mesh router). Disabling full power-off so the TV is never "
            "left unreachable again; turn it on once with the remote. Screen-off "
            "(which needs no Wake-on-LAN) stays on.", DEEP_WAKE_GIVEUP_SECONDS)

    def run(self) -> None:
        self.logger.info(
            "Easy Mode daemon started (idle backend: %s, threshold: %s, "
            "enabled: %s)",
            idle_mod.idle_backend_name(), fmt_timeout(self.config.idle_seconds),
            self.config.idle_enabled,
        )
        if not idle_mod.is_real_backend():
            self.logger.warning(
                "Idle detection is using the manual fallback; the OS-level "
                "input timer is unavailable in this environment."
            )
        # Connect once up front to learn and persist the TV's port and MAC,
        # even before the first idle event, so the config self-populates promptly.
        try:
            if self._ensure_client():
                self._drop_client()
        except Exception:  # noqa: BLE001 - best effort, never block startup
            pass
        # Start watching for whole-PC suspend so the TV follows it to sleep.
        self._start_sleep_watcher()
        last = self._clock()
        try:
            while not self._stop.is_set():
                if self._reload_pending:
                    self._reload_pending = False
                    self.reload_config()
                try:
                    self.tick()
                except Exception as exc:  # noqa: BLE001 - never let the loop die
                    self.last_error = f"tick: {exc}"
                    self.logger.exception("Unexpected error in daemon loop")
                self._sleep_fn(self.config.poll_seconds)
                # A wall-clock gap far beyond our poll interval means the process
                # was frozen while the machine slept and has now resumed. The OS
                # sleep watcher usually handles this, but it isn't available
                # everywhere; this is the universal backstop.
                now = self._clock()
                gap = now - last
                last = now
                if gap > max(RESUME_GAP_SECONDS, self.config.poll_seconds * 3):
                    self._on_resume_detected(gap)
                    last = self._clock()  # don't count the wake itself as a freeze
        finally:
            self._stop_sleep_watcher()
            self._drop_client()
            self.logger.info("Easy Mode daemon stopped")

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._stop.clear()
        self._thread = threading.Thread(target=self.run, daemon=True,
                                        name="lgtv-easy-daemon")
        self._thread.start()

    def stop(self, join_timeout: float = 5.0) -> None:
        self._stop.set()
        self._wake.set()  # cut any current poll-sleep short so we exit promptly
        if self._thread:
            self._thread.join(timeout=join_timeout)

    # ----- config hot-reload -------------------------------------------
    def _interruptible_sleep(self, seconds: float) -> None:
        """Default poll-sleep that a settings change can cut short.

        Waiting on an event instead of a bare ``time.sleep`` lets ``nudge`` and
        ``request_reload`` wake the loop the instant the user changes a setting,
        so a new timeout applies immediately rather than after the current poll
        interval - and it makes ``stop`` return promptly too.
        """
        if self._wake.wait(timeout=seconds):
            self._wake.clear()

    def nudge(self) -> None:
        """Cut the current poll-sleep short so the loop re-evaluates now.

        Used when the config object is shared by reference (the GUI drives the
        daemon in-process): the values are already updated, we just want the
        effect without waiting out the poll interval. Safe from any thread.
        """
        self._wake.set()

    def request_reload(self) -> None:
        """Ask the loop to re-read settings from disk at the next opportunity.

        Triggered by SIGHUP, which the GUI/CLI send after saving when a separate
        daemon process owns the watcher. Safe to call from a signal handler: it
        only sets flags. The bool is set first so even a missed event wakeup is
        still picked up on the next natural poll.
        """
        self._reload_pending = True
        self._wake.set()

    def reload_config(self) -> None:
        """Re-read the config file and apply it to the running loop live.

        Policy settings (timeouts, the on/off switch, mute, deep-off, sleep
        following) are taken from the file. The TV's identity - ip/mac/key/port,
        which the daemon discovers and persists itself - is preserved when the
        file carries no (newer) value, so saving a settings change can never wipe
        a freshly-learned Wake-on-LAN MAC out from under the daemon.
        """
        try:
            fresh = Config.load()
        except Exception as exc:  # noqa: BLE001 - best effort, never crash the loop
            self.logger.debug("Config reload failed: %s", exc)
            return
        live, dev = self.config.device, fresh.device
        # Don't let an empty field on disk clobber something the daemon learned
        # at runtime; a genuinely changed (non-empty) value still wins.
        dev.ip = dev.ip or live.ip
        dev.mac = dev.mac or live.mac
        dev.key = dev.key or live.key
        dev.name = dev.name or live.name
        dev.secure = dev.secure or live.secure
        with self._action_lock:
            old, self.config = self.config, fresh
        if (fresh.idle_enabled, fresh.idle_minutes, fresh.deep_off_enabled,
                fresh.deep_off_minutes) != (
                old.idle_enabled, old.idle_minutes, old.deep_off_enabled,
                old.deep_off_minutes):
            self.logger.info(
                "Settings reloaded: idle-sleep %s after %s%s.",
                "ON" if fresh.idle_enabled else "OFF",
                fmt_timeout(fresh.idle_seconds),
                (f"; full power-off after {fmt_timeout(fresh.deep_off_seconds)}"
                 if fresh.deep_off_enabled else ""))
        else:
            self.logger.debug("Settings reloaded (no idle/deep-off change).")
