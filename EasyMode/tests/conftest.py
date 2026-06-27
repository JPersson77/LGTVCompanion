import os
import sys

# Make the package importable when running tests from the repo without install.
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

# Never let a daemon started during tests spawn the real OS suspend/resume
# monitors (gdbus + systemd-inhibit on Linux, the power-notify registration on
# Windows). The daemon checks this before starting its sleep watcher.
os.environ.setdefault("LGTV_EASY_NO_SLEEP_WATCH", "1")

# Likewise, keep the GUI's startup connection self-test from firing real network
# probes/discovery when a settings panel is built in a test; scenarios that want
# it exercise selfheal/the repair dialog explicitly.
os.environ.setdefault("LGTV_EASY_NO_SELFTEST", "1")
