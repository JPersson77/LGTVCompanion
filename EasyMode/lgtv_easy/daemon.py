"""The idle-monitoring daemon: the heart of Easy Mode.

Every ``poll_seconds`` it asks the OS how long the user has been idle. Cross the
configured threshold and the TV screen is blanked; touch the keyboard or mouse
and it comes straight back on. That is the entire job.

The loop is written with injectable dependencies (idle source, client factory,
clock, stop event) so the whole behaviour can be stepped deterministically in
tests without a real TV or a real wait.
"""
from __future__ import annotations

import threading
import time
from typing import Callable, Optional

from . import idle as idle_mod
from .applog import get_logger
from .config import Config
from .webos import WebOSClient
from .wol import send_wol

# Screen state as tracked by the daemon.
STATE_ON = "on"
STATE_OFF = "off"          # panel blanked, TV still powered and on the network
STATE_STANDBY = "standby"  # TV fully powered off (deep energy saving)


class Daemon:
    def __init__(
        self,
        config: Config,
        client_factory: Optional[Callable[[], WebOSClient]] = None,
        idle_fn: Optional[Callable[[], float]] = None,
        sleep_fn: Optional[Callable[[float], None]] = None,
        logger=None,
    ):
        self.config = config
        self.logger = logger or get_logger()
        self._idle_fn = idle_fn or idle_mod.get_idle_seconds
        self._sleep_fn = sleep_fn or time.sleep
        self._client_factory = client_factory or self._default_client_factory
        self._client: Optional[WebOSClient] = None
        self.screen_state = STATE_ON  # assume the screen is on at startup
        self._stop = threading.Event()
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
        client = self._ensure_client()
        if not client:
            return False
        try:
            client.power_off()
            self.screen_state = STATE_STANDBY
            self.deep_offs += 1
            self.logger.info("TV powered off (deep energy saving) after %.0f min idle",
                             self.config.deep_off_minutes)
            # The socket dies as the TV powers down; reconnect on the next wake.
            self._drop_client()
            return True
        except Exception as exc:  # noqa: BLE001
            self.last_error = f"power_off: {exc}"
            self.logger.warning("Failed to power off TV: %s", exc)
            self._drop_client()
            return False

    def wake_screen(self) -> bool:
        # If the panel went into standby it may need a magic packet first.
        if self.config.device.mac:
            try:
                send_wol(self.config.device.mac)
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
        while not self._stop.is_set():
            try:
                self.tick()
            except Exception as exc:  # noqa: BLE001 - never let the loop die
                self.last_error = f"tick: {exc}"
                self.logger.exception("Unexpected error in daemon loop")
            self._sleep_fn(self.config.poll_seconds)
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
