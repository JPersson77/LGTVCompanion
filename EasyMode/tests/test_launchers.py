"""Guard-rails for the two portable launchers at the repository root.

These are plain-text checks (no shell/PowerShell execution needed) that catch the
classes of mistake that would silently break the one-double-click experience:

* the launcher pointing at the wrong app subdirectory,
* the launcher opening the old text wizard instead of the graphical front door,
* the Windows .bat losing its link to the .ps1 that does the real work.

If the app folder is ever renamed, or a launcher reverts to ``wizard``, one of
these fails loudly instead of shipping a broken installer.
"""
import os

import pytest

REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", ".."))
APP_DIR_NAME = "EasyMode"

WIN_BAT = os.path.join(REPO_ROOT, "Windows Launch.bat")
# The PowerShell engine lives inside the app folder; the .bat at the root is the
# only Windows file a user touches.
WIN_PS1 = os.path.join(REPO_ROOT, APP_DIR_NAME, "LGTV-Easy-Mode-WINDOWS.ps1")
LINUX_SH = os.path.join(REPO_ROOT, "Linux Launch.sh")


def _read(path):
    if not os.path.exists(path):
        pytest.skip(f"{path} not present (running outside a full checkout)")
    with open(path, "r", encoding="utf-8") as fh:
        return fh.read()


def test_app_subdir_matches_real_folder():
    assert os.path.isdir(os.path.join(REPO_ROOT, APP_DIR_NAME))


def test_windows_launcher_points_at_real_app_dir():
    ps1 = _read(WIN_PS1)
    assert f'$SubDir = "{APP_DIR_NAME}"' in ps1


def test_linux_launcher_points_at_real_app_dir():
    sh = _read(LINUX_SH)
    assert f'SUBDIR="{APP_DIR_NAME}"' in sh


def test_launchers_open_the_graphical_front_door():
    # The whole point of this update: the launchers open the GUI ("gui"),
    # not the old text-only wizard, as the everyday front door.
    ps1 = _read(WIN_PS1)
    sh = _read(LINUX_SH)
    assert 'Run-Cli @("gui")' in ps1, "Windows launcher should open the GUI"
    assert "run_cli gui" in sh, "Linux launcher should open the GUI"


def test_bat_invokes_the_ps1():
    bat = _read(WIN_BAT)
    assert "LGTV-Easy-Mode-WINDOWS.ps1" in bat
    assert "powershell" in bat.lower()


def test_bat_points_into_the_app_folder():
    # The .ps1 engine moved into the app folder, so the root .bat must reference
    # it via that subdirectory (both the cloned copy and the local fallback).
    bat = _read(WIN_BAT)
    assert f"{APP_DIR_NAME}\\LGTV-Easy-Mode-WINDOWS.ps1" in bat


def test_ps1_lives_in_the_app_folder():
    assert os.path.exists(WIN_PS1), "the PowerShell engine should live in EasyMode/"
    assert not os.path.exists(
        os.path.join(REPO_ROOT, "LGTV-Easy-Mode-WINDOWS.ps1")
    ), "the .ps1 should no longer sit at the repo root"


def test_launchers_self_update_from_a_repo():
    ps1 = _read(WIN_PS1)
    sh = _read(LINUX_SH)
    assert "LGTV_EASY_REPO" in ps1 and "git clone" in ps1
    assert "LGTV_EASY_REPO" in sh and "git clone" in sh


def test_windows_supervisor_does_not_redirect_both_streams_to_one_file():
    # PowerShell's Start-Process raises a terminating error when standard output
    # and standard error are redirected to the SAME file - that would crash the
    # background watcher on every Windows launch. Make sure the two redirects
    # never name the same path again.
    ps1 = _read(WIN_PS1)
    import re
    # The two redirect flags sit next to each other on one Start-Process call.
    pairs = re.findall(
        r"-RedirectStandardError\s+(\S+)\s+-RedirectStandardOutput\s+(\S+)", ps1)
    assert pairs, "expected the supervisor to redirect the daemon's streams"
    for err, out in pairs:
        assert err != out, (
            "Start-Process redirects stdout and stderr to the same file "
            f"({err}); PowerShell forbids this and the supervisor will crash.")
