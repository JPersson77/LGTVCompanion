"""A tiny, dependency-free D-Bus *session-bus* client for one job: calling a
no-argument method that returns a single unsigned integer (an idle-time query).

Why this exists: idle detection on Wayland has to ask the compositor's D-Bus
service how long the user has been idle, every poll. Shelling out to ``gdbus``
each time spawns a whole process (fork+exec+GLib startup) several times a minute,
forever - wasteful on a tool whose entire purpose is saving energy. This module
holds one persistent socket to the session bus instead and marshals the single
method call by hand, so the steady-state cost is a couple of small reads/writes.

It is deliberately minimal - it speaks only enough of the D-Bus wire protocol to
issue a no-arg ``METHOD_CALL`` and read back a ``u``/``t``/``i`` return value -
and it never raises into the caller: ``session_get_uint`` returns ``None`` on any
problem, so idle.py can fall back to ``gdbus`` (or another backend) cleanly. If
the connection can't be established a few times running, it disables itself so it
never adds overhead on a system where the native path simply doesn't work.
"""
from __future__ import annotations

import os
import socket
import struct
import threading
from typing import Optional

# After this many consecutive *connection* failures (not D-Bus error replies),
# stop trying the native path for the rest of the process - the caller's gdbus
# fallback takes over and we add no further cost.
_MAX_CONN_FAILURES = 3


class _DBusErrorReply(Exception):
    """The bus answered with an ERROR (e.g. method not supported). The
    connection is healthy; this particular call just has no value to give."""


def _pad(n: int, align: int) -> int:
    return (align - (n % align)) % align


def _marshal_string(value: str) -> bytes:
    """Marshal a D-Bus STRING ('s') / OBJECT_PATH ('o'): u32 length + bytes + nul."""
    data = value.encode("utf-8")
    return struct.pack("<I", len(data)) + data + b"\0"


def _marshal_signature(sig: str) -> bytes:
    """Marshal a D-Bus SIGNATURE ('g'): u8 length + bytes + nul."""
    data = sig.encode("ascii")
    return struct.pack("<B", len(data)) + data + b"\0"


class _Connection:
    """One authenticated session-bus connection, reused across calls."""

    def __init__(self) -> None:
        self._sock: Optional[socket.socket] = None
        self._serial = 0
        self._lock = threading.Lock()
        self._conn_failures = 0
        self._disabled = False

    # ----- connection setup -------------------------------------------------
    @staticmethod
    def _bus_path() -> "tuple[Optional[str], Optional[str]]":
        """(path, abstract) from DBUS_SESSION_BUS_ADDRESS; None,None if unusable."""
        addr = os.environ.get("DBUS_SESSION_BUS_ADDRESS", "")
        if not addr:
            # The well-known fallback location for the user's session bus.
            uid = os.getuid() if hasattr(os, "getuid") else None
            if uid is not None:
                guess = f"/run/user/{uid}/bus"
                if os.path.exists(guess):
                    return guess, None
            return None, None
        # Use the first transport in the (possibly ;-separated) address list.
        for part in addr.split(";")[0].split(","):
            if part.startswith("unix:path="):
                return part[len("unix:path="):], None
            if part.startswith("path="):
                return part[len("path="):], None
            if part.startswith("unix:abstract="):
                return None, part[len("unix:abstract="):]
            if part.startswith("abstract="):
                return None, part[len("abstract="):]
        return None, None

    def _connect(self, timeout: float) -> None:
        path, abstract = self._bus_path()
        if path is None and abstract is None:
            raise OSError("no usable session bus address")
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect("\0" + abstract if abstract is not None else path)
        # SASL handshake: EXTERNAL auth with our uid, the standard for the
        # session bus on the same machine.
        uid = os.getuid() if hasattr(os, "getuid") else 0
        hex_uid = "".join(f"{ord(c):02x}" for c in str(uid)).encode()
        sock.sendall(b"\0AUTH EXTERNAL " + hex_uid + b"\r\n")
        if not self._recv_line(sock).startswith("OK"):
            raise OSError("D-Bus EXTERNAL auth rejected")
        sock.sendall(b"BEGIN\r\n")
        self._sock = sock
        self._serial = 0
        # Every connection must say Hello before issuing other calls.
        self._call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                   "org.freedesktop.DBus", "Hello", expect_value=False)

    @staticmethod
    def _recv_line(sock: socket.socket) -> str:
        buf = b""
        while not buf.endswith(b"\r\n"):
            ch = sock.recv(1)
            if not ch:
                raise OSError("session bus closed during auth")
            buf += ch
            if len(buf) > 8192:
                raise OSError("auth reply too long")
        return buf[:-2].decode("utf-8", "replace")

    # ----- message I/O ------------------------------------------------------
    def _recv_exact(self, n: int) -> bytes:
        assert self._sock is not None
        buf = b""
        while len(buf) < n:
            chunk = self._sock.recv(n - len(buf))
            if not chunk:
                raise OSError("session bus closed")
            buf += chunk
        return buf

    def _next_serial(self) -> int:
        self._serial += 1
        return self._serial

    def _send_method_call(self, dest, path, interface, member) -> int:
        assert self._sock is not None
        serial = self._next_serial()
        fields = b""

        def add_field(code: int, sig: str, value: str) -> None:
            nonlocal fields
            fields += b"\0" * _pad(len(fields), 8)   # each header field is 8-aligned
            fields += struct.pack("<B", code)         # field code byte
            fields += _marshal_signature(sig)         # variant's type signature
            fields += b"\0" * _pad(len(fields), 4)    # align to the value (4 for s/o)
            fields += _marshal_string(value)

        add_field(1, "o", path)        # PATH
        add_field(6, "s", dest)        # DESTINATION
        add_field(2, "s", interface)   # INTERFACE
        add_field(3, "s", member)      # MEMBER

        header = struct.pack("<BBBB", ord("l"), 1, 0, 1)  # LE, METHOD_CALL, flags, v1
        header += struct.pack("<I", 0)            # body length (no args)
        header += struct.pack("<I", serial)
        header += struct.pack("<I", len(fields))  # header-fields array length
        header += fields
        header += b"\0" * _pad(len(header), 8)    # pad to 8 before the (empty) body
        self._sock.sendall(header)
        return serial

    def _read_message(self) -> dict:
        head = self._recv_exact(16)
        if head[0:1] != b"l":
            # We only ever emit little-endian; well-behaved buses reply in kind,
            # but guard rather than mis-parse a big-endian reply.
            raise OSError("unexpected big-endian D-Bus reply")
        mtype = head[1]
        body_len, serial, fields_len = struct.unpack("<III", head[4:16])
        fields_raw = self._recv_exact(fields_len)
        self._recv_exact(_pad(16 + fields_len, 8))   # padding before body
        body = self._recv_exact(body_len)
        sig, reply_serial, error_name = self._parse_fields(fields_raw)
        return {"type": mtype, "serial": serial, "reply_serial": reply_serial,
                "error": error_name, "sig": sig, "body": body}

    @staticmethod
    def _parse_fields(raw: bytes) -> "tuple[str, Optional[int], Optional[str]]":
        """Pull SIGNATURE(8), REPLY_SERIAL(5) and ERROR_NAME(4) out of the
        header-field array. Other fields are skipped by their own type."""
        sig, reply_serial, error_name = "", None, None
        i, n = 0, len(raw)
        while i < n:
            i += _pad(i, 8)
            if i + 2 > n:
                break
            code = raw[i]; i += 1
            siglen = raw[i]; i += 1
            fsig = raw[i:i + siglen].decode("ascii", "replace"); i += siglen + 1
            if fsig in ("s", "o"):
                i += _pad(i, 4)
                (slen,) = struct.unpack("<I", raw[i:i + 4]); i += 4
                val = raw[i:i + slen].decode("utf-8", "replace"); i += slen + 1
                if code == 8:
                    sig = val
                elif code == 4:
                    error_name = val
            elif fsig == "g":
                glen = raw[i]; i += 1
                val = raw[i:i + glen].decode("ascii", "replace"); i += glen + 1
                if code == 8:
                    sig = val
            elif fsig == "u":
                i += _pad(i, 4)
                (val,) = struct.unpack("<I", raw[i:i + 4]); i += 4
                if code == 5:
                    reply_serial = val
            else:
                break  # an unfamiliar field type: stop rather than misalign
        return sig, reply_serial, error_name

    def _call(self, dest, path, interface, member, expect_value=True):
        serial = self._send_method_call(dest, path, interface, member)
        for _ in range(64):  # skip signals/other replies until ours arrives
            msg = self._read_message()
            if msg["reply_serial"] != serial:
                continue
            if msg["type"] == 3 or msg["error"]:   # ERROR message
                raise _DBusErrorReply(msg["error"] or "error")
            if not expect_value:
                return None
            return self._decode_uint(msg["sig"], msg["body"])
        raise OSError("no matching D-Bus reply")

    @staticmethod
    def _decode_uint(sig: str, body: bytes) -> int:
        if sig == "u" and len(body) >= 4:
            return struct.unpack("<I", body[:4])[0]
        if sig == "t" and len(body) >= 8:
            return struct.unpack("<Q", body[:8])[0]
        if sig == "i" and len(body) >= 4:
            return struct.unpack("<i", body[:4])[0]
        if sig == "x" and len(body) >= 8:
            return struct.unpack("<q", body[:8])[0]
        raise _DBusErrorReply(f"unhandled reply signature {sig!r}")

    # ----- public ----------------------------------------------------------
    def get_uint(self, dest, path, interface, member,
                 timeout: float = 2.0) -> Optional[int]:
        """Call the no-arg method and return its integer result, or ``None``.

        ``None`` means "no value this time" - either the bus replied with an
        error (method unsupported) or the connection had trouble. The connection
        is re-established on demand; after repeated connection failures the
        native path disables itself so the caller's fallback simply takes over.
        """
        with self._lock:
            if self._disabled:
                return None
            try:
                if self._sock is None:
                    self._connect(timeout)
                self._sock.settimeout(timeout)
                value = self._call(dest, path, interface, member)
                self._conn_failures = 0   # a clean round-trip: connection is good
                return value
            except _DBusErrorReply:
                # The socket is fine; this method just has nothing for us.
                self._conn_failures = 0
                return None
            except Exception:  # noqa: BLE001 - socket/protocol/parse trouble
                self._close_locked()
                self._conn_failures += 1
                if self._conn_failures >= _MAX_CONN_FAILURES:
                    self._disabled = True
                return None

    def _close_locked(self) -> None:
        if self._sock is not None:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None


# One shared connection for the whole process.
_CONN = _Connection()


def session_get_uint(dest: str, path: str, interface: str, member: str,
                     timeout: float = 2.0) -> Optional[int]:
    """Call a no-arg session-bus method returning an unsigned int. Never raises;
    returns ``None`` if the native D-Bus path can't answer (caller should fall
    back to ``gdbus`` or another method)."""
    return _CONN.get_uint(dest, path, interface, member, timeout=timeout)
