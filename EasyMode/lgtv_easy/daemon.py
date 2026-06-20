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
from .config import Config
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


class Daemon:
    def __init__(
        self,
        config: Config,
        client_factory: Optional[Callable[[], WebOSClient]] = None,
        idle_fn: Optional[Callable[[], float]] = None,
        sleep_fn: Optional[Callable[[float], None]] = None,
        clock_fn: Optional[Callable[[], float]] = None,
        sleep_watcher_factory: Optional[Callable[..., object]] = None,
        logger=None,
    ):
        self.config = config
        self.logger = logger or get_logger()
        self._idle_fn = idle_fn or idle_mod.get_idle_seconds
        self._sleep_fn = sleep_fn or time.sleep
        self._clock = clock_fn or time.monotonic
        self._client_factory = client_factory or self._default_client_factory
        self._sleep_watcher_factory = sleep_watcher_factory or system_sleep.make_watcher
        self._sleep_watcher: Optional[object] = None
        self._client: Optional[WebOSClient] = None
        self.screen_state = STATE_ON  # assume the screen is on at startup
        self._stop = threading.Event()
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

    # ----- TV connection ----------------------------------------------
    def _default_client_factory(self) -> WebOSClient:
        return WebOSClient(self.config.device.ip, secure=self.config.device.secure)

    def _ensure_client(self) -> Optional[WebOSClient]:
        if self._client and self._client.connected:
            return self._client
        try:
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
            self._client = client
            return client
        except Exception as exc:  # noqa: BLE001 - network errors are expected
            self.last_error = f"connect: {exc}"
            self.logger.warning("Could not connect to TV: %s", exc)
            self._client = None
            return None

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
                self.logger.info("Screen off after %.0f min idle",
                                 self.config.idle_minutes)
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
                self.deep_offs += 1
                self.logger.info("TV powered off (deep energy saving) after %.0f min idle",
                                 self.config.deep_off_minutes)
                # The socket dies as the TV powers down; reconnect on next wake.
                self._drop_client()
                return True
            except Exception as exc:  # noqa: BLE001
                self.last_error = f"power_off: {exc}"
                self.logger.warning("Failed to power off TV: %s", exc)
                self._drop_client()
                return False

    def wake_screen(self) -> bool:
        with self._action_lock:
            # If the panel went into standby it may need a magic packet first. Aim
            # it at both the limited broadcast and the TV's directed subnet
            # broadcast so it wakes reliably across a Google/Nest Wifi mesh (where
            # the limited broadcast isn't always forwarded between wired and
            # wireless segments).
            if self.config.device.mac:
                try:
                    from .wol import broadcast_targets
                    send_wol(self.config.device.mac,
                             broadcast=broadcast_targets(self.config.device.ip))
                except Exception as exc:  # noqa: BLE001
                    self.logger.debug("WOL send failed (often harmless): %s", exc)
            client = self._ensure_client()
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
            self.wake_screen()

    def run(self) -> None:
        self.logger.info(
            "Easy Mode daemon started (idle backend: %s, threshold: %.1f min, "
            "enabled: %s)",
            idle_mod.idle_backend_name(), self.config.idle_minutes,
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
        if self._thread:
            self._thread.join(timeout=join_timeout)
