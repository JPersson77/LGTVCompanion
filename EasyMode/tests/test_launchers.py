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


def test_windows_detached_launch_quotes_the_script_path():
    """A detached watcher must survive a space in its own path.

    ``Start-Process`` joins ``-ArgumentList`` with spaces and does NOT quote the
    entries. An unquoted script path under, say, ``C:\\Users\\First Last\\`` then
    reaches powershell.exe cut in half - it reports ``-File 'C:\\Users\\First'``
    and exits at once. The watcher is started hidden, so that failure is silent:
    the launcher happily reports "running in the background" while nothing runs,
    which is exactly the bug this guards. (The bash launcher quotes correctly, so
    Linux never saw it.)

    Both halves are checked: every detached PowerShell must go through the single
    helper, and that helper must quote the path.
    """
    import re
    ps1 = _read(WIN_PS1)
    spawns = re.findall(r'Start-Process\s+-FilePath\s+"powershell\.exe"', ps1)
    assert len(spawns) == 1, (
        "detached PowerShell should be spawned from exactly one helper "
        "(Start-Detached), so the path is quoted in a single place; found "
        f"{len(spawns)} Start-Process calls for powershell.exe")
    assert "function Start-Detached" in ps1, "expected a Start-Detached helper"
    # The helper has to build a genuinely quoted path out of $scriptPath...
    m = re.search(r"\$quoted\s*=\s*(.+)", ps1)
    assert m and '"' in m.group(1) and "$scriptPath" in m.group(1), (
        "Start-Detached must wrap the script path in double quotes")
    # ...and that quoted value is what -File receives.
    assert re.search(r'"-File",\s*\$quoted', ps1), (
        "the quoted path must be the argument that follows -File")


def test_windows_supervisor_guards_against_a_second_watcher():
    # Mirrors the Linux launcher: a supervisor that finds a live one already
    # holding the pidfile stands down, instead of clobbering the pidfile and
    # stacking another daemon that blocks forever on the single-instance lock.
    ps1 = _read(WIN_PS1)
    sh = _read(LINUX_SH)
    assert "not starting another" in ps1, (
        "the Windows supervisor should stand down when one is already running")
    assert "not starting another" in sh


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
