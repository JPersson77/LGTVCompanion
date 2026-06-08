"""A tiny, dependency-free WebSocket implementation (RFC 6455).

Only what we need to talk to a WebOS TV: text frames, ping/pong and close, over
a plain or TLS socket. Implementing this in the standard library means Easy Mode
has no third-party runtime dependencies, which keeps the launchers trivial.

The same code drives both the real client and the mock TV used in tests, so the
framing is exercised from both sides.
"""
from __future__ import annotations

import base64
import hashlib
import os
import socket
import ssl as ssl_mod
import struct
from typing import Optional, Tuple
from urllib.parse import urlparse

_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

OP_CONT = 0x0
OP_TEXT = 0x1
OP_BINARY = 0x2
OP_CLOSE = 0x8
OP_PING = 0x9
OP_PONG = 0xA


class WebSocketError(Exception):
    pass


class WebSocket:
    """Minimal blocking WebSocket connection over an existing socket."""

    def __init__(self, sock: socket.socket, is_client: bool):
        self.sock = sock
        self.is_client = is_client
        self._buf = b""
        self.closed = False

    # ----- connection setup -------------------------------------------
    @classmethod
    def connect(cls, url: str, timeout: float = 10.0) -> "WebSocket":
        parsed = urlparse(url)
        secure = parsed.scheme == "wss"
        host = parsed.hostname
        port = parsed.port or (3001 if secure else 3000)
        path = parsed.path or "/"

        raw = socket.create_connection((host, port), timeout=timeout)
        raw.settimeout(timeout)
        if secure:
            # WebOS TVs use a self-signed certificate; verification is not
            # meaningful on a LAN device, so we deliberately skip it.
            ctx = ssl_mod.SSLContext(ssl_mod.PROTOCOL_TLS_CLIENT)
            ctx.check_hostname = False
            ctx.verify_mode = ssl_mod.CERT_NONE
            raw = ctx.wrap_socket(raw, server_hostname=host)

        key = base64.b64encode(os.urandom(16)).decode()
        req = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"
        )
        raw.sendall(req.encode())
        ws = cls(raw, is_client=True)
        headers = ws._read_http_headers()
        if "101" not in headers.split("\r\n", 1)[0]:
            raw.close()
            raise WebSocketError(f"Handshake failed: {headers.splitlines()[0]!r}")
        return ws

    @classmethod
    def accept(cls, sock: socket.socket) -> "WebSocket":
        """Server side: complete the upgrade handshake on an accepted socket."""
        ws = cls(sock, is_client=False)
        headers = ws._read_http_headers()
        key = ""
        for line in headers.split("\r\n"):
            if line.lower().startswith("sec-websocket-key:"):
                key = line.split(":", 1)[1].strip()
        accept = base64.b64encode(
            hashlib.sha1((key + _GUID).encode()).digest()
        ).decode()
        resp = (
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Accept: {accept}\r\n\r\n"
        )
        sock.sendall(resp.encode())
        return ws

    # ----- low level ---------------------------------------------------
    def _read_http_headers(self) -> str:
        while b"\r\n\r\n" not in self._buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise WebSocketError("Connection closed during handshake")
            self._buf += chunk
        head, _, rest = self._buf.partition(b"\r\n\r\n")
        self._buf = rest
        return head.decode("latin-1")

    def _recv_exact(self, n: int) -> bytes:
        while len(self._buf) < n:
            chunk = self.sock.recv(max(4096, n - len(self._buf)))
            if not chunk:
                raise WebSocketError("Connection closed")
            self._buf += chunk
        out, self._buf = self._buf[:n], self._buf[n:]
        return out

    def _send_frame(self, opcode: int, data: bytes) -> None:
        if self.closed:
            raise WebSocketError("Socket already closed")
        b0 = 0x80 | opcode  # FIN + opcode
        length = len(data)
        header = struct.pack("!B", b0)
        mask_bit = 0x80 if self.is_client else 0x00
        if length < 126:
            header += struct.pack("!B", mask_bit | length)
        elif length < (1 << 16):
            header += struct.pack("!BH", mask_bit | 126, length)
        else:
            header += struct.pack("!BQ", mask_bit | 127, length)
        if self.is_client:
            mask = os.urandom(4)
            masked = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
            self.sock.sendall(header + mask + masked)
        else:
            self.sock.sendall(header + data)

    def _read_frame(self) -> Tuple[int, bytes]:
        b0, b1 = struct.unpack("!BB", self._recv_exact(2))
        opcode = b0 & 0x0F
        masked = bool(b1 & 0x80)
        length = b1 & 0x7F
        if length == 126:
            (length,) = struct.unpack("!H", self._recv_exact(2))
        elif length == 127:
            (length,) = struct.unpack("!Q", self._recv_exact(8))
        mask = self._recv_exact(4) if masked else b""
        payload = self._recv_exact(length)
        if masked:
            payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
        return opcode, payload

    # ----- public API --------------------------------------------------
    def send_text(self, text: str) -> None:
        self._send_frame(OP_TEXT, text.encode("utf-8"))

    def recv_text(self) -> Optional[str]:
        """Return the next text message, or None when the peer closes."""
        while True:
            try:
                opcode, payload = self._read_frame()
            except (WebSocketError, OSError, socket.timeout):
                return None
            if opcode == OP_TEXT:
                return payload.decode("utf-8", "replace")
            if opcode == OP_PING:
                self._send_frame(OP_PONG, payload)
            elif opcode == OP_CLOSE:
                self.close()
                return None
            # PONG / continuation frames are ignored.

    def close(self) -> None:
        if not self.closed:
            self.closed = True
            try:
                self._send_frame(OP_CLOSE, b"")
            except (WebSocketError, OSError):
                pass
            try:
                self.sock.close()
            except OSError:
                pass
