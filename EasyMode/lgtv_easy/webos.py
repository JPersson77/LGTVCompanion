"""WebOS protocol client.

Speaks the same SSAP protocol as the original LGTV Companion: connect over a
WebSocket, register (which prompts the TV to pair on first use and returns a
reusable ``client-key``), then issue requests such as turning the screen on/off.
"""
from __future__ import annotations

import json
import threading
from typing import Optional

from ._ws import WebSocket, WebSocketError

# SSAP URIs (mirror Common/lg_api.h in the original project).
URI_SCREEN_ON = "ssap://com.webos.service.tvpower/power/turnOnScreen"
URI_SCREEN_OFF = "ssap://com.webos.service.tvpower/power/turnOffScreen"
URI_POWER_OFF = "ssap://system/turnOff"
URI_GET_POWER_STATE = "ssap://com.webos.service.tvpower/power/getPowerState"
URI_SET_MUTE = "ssap://audio/setMute"
URI_CREATE_TOAST = "ssap://system.notifications/createToast"

# Registration manifest. The permission set matches what the C++ client asks for.
_MANIFEST = {
    "manifestVersion": 1,
    "appVersion": "1.0",
    "signed": {
        "created": "20260427",
        "appId": "com.lgtvc.app",
        "localizedAppNames": {"": "LGTV Companion Easy Mode"},
        "localizedVendorNames": {"": "LGTV Companion"},
        "permissions": [
            "TEST_SECURE", "CONTROL_INPUT_TEXT", "CONTROL_MOUSE_AND_KEYBOARD",
            "READ_INSTALLED_APPS", "READ_LGE_SDX", "READ_NOTIFICATIONS",
            "SEARCH", "WRITE_SETTINGS", "WRITE_NOTIFICATION_ALERT",
            "CONTROL_POWER", "READ_CURRENT_CHANNEL", "READ_RUNNING_APPS",
            "READ_UPDATE_INFO", "UPDATE_FROM_REMOTE_APP",
            "READ_LGE_TV_INPUT_EVENTS", "READ_TV_CURRENT_TIME",
        ],
        "serial": "lgtvc-webos26",
    },
    "permissions": [
        "LAUNCH", "LAUNCH_WEBAPP", "APP_TO_APP", "CLOSE", "TEST_OPEN",
        "TEST_PROTECTED", "CONTROL_AUDIO", "CONTROL_DISPLAY",
        "CONTROL_INPUT_JOYSTICK", "CONTROL_INPUT_MEDIA_RECORDING",
        "CONTROL_INPUT_MEDIA_PLAYBACK", "CONTROL_INPUT_TV", "CONTROL_POWER",
        "CONTROL_TV_SCREEN", "READ_APP_STATUS", "READ_CURRENT_CHANNEL",
        "READ_INPUT_DEVICE_LIST", "READ_NETWORK_STATE", "READ_RUNNING_APPS",
        "READ_TV_CHANNEL_LIST", "WRITE_NOTIFICATION_TOAST", "READ_POWER_STATE",
        "READ_COUNTRY_INFO", "READ_SETTINGS",
    ],
}


def _register_payload(client_key: str = "") -> dict:
    payload = {
        "forcePairing": False,
        "pairingType": "PROMPT",
        "manifest": _MANIFEST,
    }
    if client_key:
        payload["client-key"] = client_key
    return {"type": "register", "id": "register_0", "payload": payload}


class PairingError(Exception):
    pass


def pair_with_fallback(client: "WebOSClient", client_key: str = "",
                       on_prompt=None, prompt_timeout: float = 120.0,
                       log=None) -> str:
    """Pair, automatically falling back from plain ws (3000) to secure wss (3001).

    Older WebOS TVs accept the plain WebSocket on port 3000; many newer ones only
    answer the TLS WebSocket on port 3001. Beginners can't be expected to know
    which, so we try both and report each attempt. The supplied ``client`` is
    reused (its ``secure`` flag is toggled), which keeps it testable with a mock.

    Returns the client-key on success; raises the last error if both fail.
    """
    out = log or (lambda _m: None)
    last_exc: Optional[Exception] = None
    for secure, label in ((False, "plain ws (port 3000)"),
                          (True, "secure wss (port 3001)")):
        client.secure = secure
        out(f"Attempting {label}...")
        try:
            return client.connect(client_key=client_key, on_prompt=on_prompt,
                                  prompt_timeout=prompt_timeout, log=out)
        except Exception as exc:  # noqa: BLE001
            last_exc = exc
            out(f"{label} did not work: {exc}")
            try:
                client.close()
            except Exception:  # noqa: BLE001
                pass
    raise last_exc if last_exc else PairingError("pairing failed")


class WebOSClient:
    """Synchronous WebOS client.

    Typical use::

        client = WebOSClient("192.168.1.50")
        key = client.connect(client_key=saved_key, on_prompt=lambda: print("Accept on TV"))
        client.screen_off()
        client.close()
    """

    def __init__(self, ip: str, secure: bool = False, timeout: float = 10.0):
        self.ip = ip
        self.secure = secure
        self.timeout = timeout
        self.client_key = ""
        self._ws: Optional[WebSocket] = None
        self._counter = 0
        self._lock = threading.Lock()

    @property
    def connected(self) -> bool:
        return self._ws is not None and not self._ws.closed

    def _url(self) -> str:
        scheme = "wss" if self.secure else "ws"
        # Allow "host:port" in the address (handy for testing / non-standard
        # setups); otherwise use the standard WebOS ports.
        if ":" in self.ip and not self.ip.startswith("["):
            host, _, port = self.ip.rpartition(":")
            return f"{scheme}://{host}:{port}/"
        port = 3001 if self.secure else 3000
        return f"{scheme}://{self.ip}:{port}/"

    def connect(self, client_key: str = "", on_prompt=None,
                prompt_timeout: float = 60.0, log=None) -> str:
        """Open the socket and register. Returns the (possibly new) client key.

        If the TV has not paired before it shows an on-screen prompt; ``on_prompt``
        (if given) is called once so the UI can tell the user to press OK on the
        remote. Blocks up to ``prompt_timeout`` seconds waiting for acceptance.

        ``log`` (optional) receives stage-by-stage progress lines so the wizard
        can show beginners exactly where a connection got stuck.
        """
        out = log or (lambda _m: None)
        url = self._url()
        out(f"Opening WebSocket to {url} (connect timeout {self.timeout:.0f}s)...")
        self._ws = WebSocket.connect(url, timeout=self.timeout)
        out("Connected. Sending pairing/registration request to the TV...")
        self._ws.send_text(json.dumps(_register_payload(client_key)))
        # Pairing can take a while because the user must press OK on the remote.
        self._ws.sock.settimeout(prompt_timeout)
        prompted = False
        while True:
            raw = self._ws.recv_text()
            if raw is None:
                raise PairingError("Connection closed during registration")
            msg = json.loads(raw)
            mtype = msg.get("type")
            if mtype == "registered":
                key = msg.get("payload", {}).get("client-key", client_key)
                self.client_key = key or client_key
                self._ws.sock.settimeout(self.timeout)
                out("TV accepted the registration. Paired.")
                return self.client_key
            if mtype == "response" and msg.get("payload", {}).get(
                "pairingType"
            ) == "PROMPT":
                if on_prompt and not prompted:
                    prompted = True
                    out(f"TV is showing a pairing prompt; waiting up to "
                        f"{prompt_timeout:.0f}s for you to press Accept...")
                    on_prompt()
                continue
            if mtype == "error":
                raise PairingError(msg.get("error", "registration error"))
            # Ignore anything else and keep waiting for "registered".

    def request(self, uri: str, payload: Optional[dict] = None,
                wait: bool = True) -> Optional[dict]:
        """Send an SSAP request and optionally wait for its matching response."""
        if not self.connected:
            raise WebSocketError("Not connected")
        with self._lock:
            self._counter += 1
            req_id = f"req_{self._counter}"
            self._ws.send_text(json.dumps({
                "type": "request",
                "id": req_id,
                "uri": uri,
                "payload": payload or {},
            }))
            if not wait:
                return None
            # Read until we see the response with our id (TVs answer in order).
            for _ in range(20):
                raw = self._ws.recv_text()
                if raw is None:
                    raise WebSocketError("Connection closed awaiting response")
                msg = json.loads(raw)
                if msg.get("id") == req_id:
                    return msg
            raise WebSocketError("No matching response received")

    # ----- high level convenience -------------------------------------
    def screen_off(self) -> Optional[dict]:
        return self.request(URI_SCREEN_OFF)

    def screen_on(self) -> Optional[dict]:
        return self.request(URI_SCREEN_ON)

    def power_off(self) -> Optional[dict]:
        return self.request(URI_POWER_OFF)

    def set_mute(self, mute: bool) -> Optional[dict]:
        return self.request(URI_SET_MUTE, {"mute": mute})

    def get_power_state(self) -> Optional[dict]:
        return self.request(URI_GET_POWER_STATE)

    def toast(self, message: str) -> Optional[dict]:
        return self.request(URI_CREATE_TOAST, {"message": message})

    def close(self) -> None:
        if self._ws is not None:
            self._ws.close()
            self._ws = None
