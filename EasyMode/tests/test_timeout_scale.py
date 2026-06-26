"""The non-linear timeout scale and its human-friendly formatting.

Importing lgtv_easy.gui is safe without a display (no Tk root is created at
import time); the SteppedSlider widget itself is exercised by the GUI smoke job.
"""
from lgtv_easy.config import fmt_timeout
from lgtv_easy.gui import DEEP_STEPS_SEC, SLEEP_STEPS_SEC, _build_steps


def test_fmt_timeout_reads_naturally():
    assert fmt_timeout(10) == "10 seconds"
    assert fmt_timeout(30) == "30 seconds"
    assert fmt_timeout(60) == "1 minute"
    assert fmt_timeout(120) == "2 minutes"
    assert fmt_timeout(600) == "10 minutes"
    assert fmt_timeout(7200) == "120 minutes"
    assert fmt_timeout(90) == "1.5 minutes"   # an off-grid value stays readable


def test_sleep_scale_matches_the_requested_steps():
    # New bounds: 10 seconds up to 120 minutes.
    assert SLEEP_STEPS_SEC[0] == 10
    assert SLEEP_STEPS_SEC[-1] == 7200
    # 10s -> 1min in 10-second steps.
    assert SLEEP_STEPS_SEC[:6] == [10, 20, 30, 40, 50, 60]
    # 1 -> 10 min in 1-minute steps.
    assert [s for s in SLEEP_STEPS_SEC if 60 <= s <= 600] == \
        [60, 120, 180, 240, 300, 360, 420, 480, 540, 600]
    # 10 -> 120 min in 10-minute steps.
    assert [s for s in SLEEP_STEPS_SEC if s >= 600] == \
        [600, 1200, 1800, 2400, 3000, 3600, 4200, 4800, 5400, 6000, 6600, 7200]
    # Ordered, de-duplicated at the range boundaries (60 and 600 appear once).
    assert SLEEP_STEPS_SEC == sorted(SLEEP_STEPS_SEC)
    assert len(SLEEP_STEPS_SEC) == len(set(SLEEP_STEPS_SEC))


def test_deep_off_scale_starts_at_one_minute():
    # "A longer idle" - sub-minute makes no sense for a full power-off.
    assert DEEP_STEPS_SEC[0] == 60
    assert DEEP_STEPS_SEC[-1] == 7200
    assert 10 not in DEEP_STEPS_SEC and 30 not in DEEP_STEPS_SEC


def test_build_steps_dedupes_shared_boundaries():
    assert _build_steps((0, 4, 2), (4, 8, 2)) == [0, 2, 4, 6, 8]
