"""Standalone MockTV process for cross-process launcher testing.

Usage: mock_tv_server.py <port> <event_log_path>
Runs a mock WebOS TV on the given port and appends every received SSAP URI to
the event-log file, so a separate process (the launcher) can be verified.
"""
import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from lgtv_easy import mock_tv as mt  # noqa: E402


def main():
    port = int(sys.argv[1])
    logfile = sys.argv[2]

    orig = mt.MockTV._handle_request

    def logged(self, ws, msg):
        with open(logfile, "a") as fh:
            fh.write(msg.get("uri", "") + "\n")
        return orig(self, ws, msg)

    mt.MockTV._handle_request = logged

    tv = mt.MockTV(require_pairing=False, host="127.0.0.1")
    # Bind to the requested fixed port instead of an ephemeral one.
    import socket
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port))
    srv.listen(5)
    srv.settimeout(0.5)
    tv._srv = srv
    tv.port = port
    import threading
    tv._thread = threading.Thread(target=tv._serve, daemon=True)
    tv._thread.start()
    tv._ready.wait(timeout=2)
    print(f"mock tv ready on {port}", flush=True)
    try:
        while True:
            import time
            time.sleep(1)
    except KeyboardInterrupt:
        tv.stop()


if __name__ == "__main__":
    main()
