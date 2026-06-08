"""A mock WebOS TV for tests and simulated usage.

Implements just enough of the SSAP protocol to exercise the real client end to
end: the register/pairing handshake (including the first-time PROMPT), issuing a
client-key, and responding to screen on/off and mute requests while tracking
state. Lets the whole app be verified on a machine with no LG TV attached.
"""
from __future__ import annotations

import json
import socket
import threading
from typing import Optional

from ._ws import WebSocket


class MockTV:
    def __init__(self, require_pairing: bool = True, known_key: str = "",
                 host: str = "127.0.0.1"):
        self.require_pairing = require_pairing
        self.known_key = known_key or "MOCK-KEY-0001"
        self.host = host
        self.port = 0
        # Observable state for assertions.
        self.screen_on = True
        self.muted = False
        self.powered_on = True
        self.pair_prompts = 0
        self.requests: list = []
        self._srv: Optional[socket.socket] = None
        self._thread: Optional[threading.Thread] = None
        self._stop = threading.Event()
        self._ready = threading.Event()

    # ----- lifecycle ---------------------------------------------------
    def start(self) -> "MockTV":
        self._srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._srv.bind((self.host, 0))
        self._srv.listen(5)
        self._srv.settimeout(0.5)
        self.port = self._srv.getsockname()[1]
        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._thread.start()
        self._ready.wait(timeout=2)
        return self

    def stop(self) -> None:
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=2)
        if self._srv:
            self._srv.close()

    @property
    def url(self) -> str:
        return f"ws://{self.host}:{self.port}/"

    def __enter__(self):
        return self.start()

    def __exit__(self, *exc):
        self.stop()

    # ----- serving -----------------------------------------------------
    def _serve(self) -> None:
        self._ready.set()
        while not self._stop.is_set():
            try:
                conn, _ = self._srv.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            threading.Thread(target=self._handle, args=(conn,),
                             daemon=True).start()

    def _handle(self, conn: socket.socket) -> None:
        conn.settimeout(10)
        try:
            ws = WebSocket.accept(conn)
        except Exception:
            conn.close()
            return
        try:
            while not self._stop.is_set():
                raw = ws.recv_text()
                if raw is None:
                    break
                msg = json.loads(raw)
                self._dispatch(ws, msg)
        finally:
            ws.close()

    def _dispatch(self, ws: WebSocket, msg: dict) -> None:
        mtype = msg.get("type")
        if mtype == "register":
            self._handle_register(ws, msg)
            return
        if mtype == "request":
            self._handle_request(ws, msg)

    def _handle_register(self, ws: WebSocket, msg: dict) -> None:
        payload = msg.get("payload", {})
        supplied_key = payload.get("client-key", "")
        if self.require_pairing and supplied_key != self.known_key:
            # First-time pairing: emit a PROMPT, then "accept" and register.
            self.pair_prompts += 1
            ws.send_text(json.dumps({
                "type": "response", "id": msg.get("id"),
                "payload": {"pairingType": "PROMPT", "returnValue": True},
            }))
        ws.send_text(json.dumps({
            "type": "registered", "id": msg.get("id"),
            "payload": {"client-key": self.known_key},
        }))

    def _handle_request(self, ws: WebSocket, msg: dict) -> None:
        uri = msg.get("uri", "")
        self.requests.append(uri)
        payload_out = {"returnValue": True}
        if uri.endswith("turnOffScreen"):
            self.screen_on = False
        elif uri.endswith("turnOnScreen"):
            self.screen_on = True
        elif uri.endswith("system/turnOff"):
            self.powered_on = False
        elif uri.endswith("setMute"):
            self.muted = bool(msg.get("payload", {}).get("mute"))
        elif uri.endswith("getPowerState"):
            payload_out["state"] = "Active" if self.powered_on else "Suspend"
        ws.send_text(json.dumps({
            "type": "response", "id": msg.get("id"), "payload": payload_out,
        }))
