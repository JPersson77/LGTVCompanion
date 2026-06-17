LGTV Companion - Easy Mode
==========================

Make your LG OLED TV sleep like a PC monitor: the screen turns off after a few
minutes of inactivity and wakes when you move the mouse or press a key.

HOW TO START
------------
  Windows : double-click   LGTV-Easy-Mode-WINDOWS.bat
  Linux   : run            ./LGTV-Easy-Mode-UBUNTU.sh

That's it. The first run installs what it needs, opens a 3-step setup window
(Find your TV -> Pair -> choose the timeout; 7 minutes is a good default), then
keeps your TV sleeping in the background. Closing the window does NOT stop it.

TO STOP THE BACKGROUND WATCHER
------------------------------
  Windows : run LGTV-Easy-Mode-WINDOWS.ps1 with  -Stop
  Linux   : ./LGTV-Easy-Mode-UBUNTU.sh --stop

ON YOUR TV (one-time)
---------------------
Enable "Turn on via Wi-Fi" (a.k.a. "Quick Start+" / "Always Ready") so the TV
can be woken over the network. Keep the TV and PC on the same network.

WHAT'S IN THIS FOLDER
---------------------
  LGTV-Easy-Mode-WINDOWS.bat / .ps1 - the Windows portable installer
  LGTV-Easy-Mode-UBUNTU.sh          - the Linux portable installer
  EasyMode/                         - the app itself
  README.md                         - the full guide

More help: see README.md.
