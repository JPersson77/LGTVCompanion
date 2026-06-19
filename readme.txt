LGTV Companion - Easy Mode
==========================

Make your LG OLED TV sleep like a PC monitor: the screen turns off after a few
minutes of inactivity - or the moment you put the PC to sleep - and wakes again
as soon as you move the mouse or press a key. That is the whole app - one job, a
simple window, almost nothing to configure.


HOW TO START
------------
  Windows : Double-click  "Windows Launch".

  Linux   : Right-click  "Linux Launch.sh"  ->  "Run as a Program".
            First time only: if that option isn't there, the file just needs
            permission to run (files unzipped from a download arrive without
            it). Right-click -> Properties -> Permissions -> tick "Allow
            executing file as program", then try again. Prefer a terminal?
            Open one in this folder and run:   bash "Linux Launch.sh"

That is it. The first run installs what it needs (Git + Python), downloads the
app, keeps itself up to date, then opens a 3-step setup window:

  1. Find your TV   - click Scan (or type its IP).
  2. Pair           - press OK / Accept on the prompt that pops up on the TV.
  3. Timeout        - drag the slider. 7 minutes is a good default.

After setup it keeps your TV sleeping in the background. Closing the window does
NOT stop it. (No graphical desktop? The same steps run as a text wizard.)


TO STOP THE BACKGROUND WATCHER
------------------------------
  Windows : open a terminal here and run   "Windows Launch.bat" -Stop
  Linux   : open a terminal here and run   bash "Linux Launch.sh" --stop


ON YOUR TV (one-time)
---------------------
Enable "Turn on via Wi-Fi" (a.k.a. "Quick Start+" / "Always Ready") so the TV
can be woken over the network. Keep the TV and the PC on the SAME network - a
Google/Nest Wifi mesh is fine over Ethernet or Wi-Fi, as long as it is not a
separate "guest" network. The setup window warns you if they look different.


WHAT'S IN THIS FOLDER
---------------------
  Windows Launch.bat - the Windows launcher (double-click)
  Linux Launch.sh    - the Linux launcher (right-click -> Run as a Program)
  EasyMode/          - the app itself (and its Windows engine)
  readme.txt         - this file


IF SOMETHING GOES WRONG
-----------------------
The launcher keeps its window open on an error and writes a log you can read or
share:
  Windows : %APPDATA%\LGTV Companion Easy Mode\launcher.log
  Linux   : ~/.config/lgtv-companion-easy/launcher.log

To freeze the code on disk (no auto-update), set LGTV_EASY_NO_UPDATE=1 before
launching. Developer notes and the optional command line live in EasyMode/.

LGTV Companion Easy Mode is an independent, MIT-licensed project (see LICENSE).
It controls the TV directly over the LG WebOS (SSAP) network protocol, and keeps
itself up to date from its own repository.
