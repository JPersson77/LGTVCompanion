# LGTV Companion

## Overview
This application (UI and service) controls LG WebOS TVs and displays.

This application aim to be a set and forget application to:
- provide automatic management for your WebOS display, to shut off and turn on in reponse to to the PC shutting down, rebooting, entering low power modes as well as and when user is afk (idle). 
- provide the user with a command line tool to turn displays on or off.

## Background
With the rise in popularity of using OLED TVs as PC monitors, it is apparent that standard functionality of PC-monitors is missing. Particularly turning the display on or off in response to power events in windows. With OLED monitors this is particularly important to prevent "burn-in", or more accurately pixel-wear.

## Installation and usage
1. Important prerequisites:
   - Power ON all TVs and ensure they are connected to your local area network via Wi-Fi or cable.
   - Ensure that the TV can be woken via the network. For the CX line of displays this is accomplished by navigating to Settings (cog button on remote)->All Settings->Connection->Mobile Connection Management->TV On with Mobile and enable 'Turn On via Wi-Fi'.
   - Open the administrative interface of your router, and set a static DHCP lease for your WebOS devices, i.e. to ensure that the displays always have the same IP-address on your LAN.
2. Download the setup package and install. This will install and start the service (LGTVsvc.exe), and also install the user interface (LGTV Companion.exe).
3. Open the user interface from the Windows start menu.

![mainwnd](https://user-images.githubusercontent.com/77928210/121790553-df09b400-cbe0-11eb-8922-0e84f7e1ad27.JPG)

4. Click the 'Scan' button to let the application try and automatically find network attached WebOs devices (TVs) (This button is called 'Configure' in the screenshot above)
5. Optionally, click the drop down button to manually add, remove, configure the parameters of respective devices, this includes the network IP-address, the physical address, i.e. the MAC(s). This information can easily be found in the network settings of the TV. Also, the default wake-on-lan network options should work for most configurations, but if your TV has difficulties powering on try the other options. Click the 'What's this?' link in the app to read more.

![devicewnd](https://user-images.githubusercontent.com/77928210/121790559-f5177480-cbe0-11eb-9e71-127e5a5928b8.JPG)

6. In the main application window, ensure the 'Manage this device' checkbox is checked tso the application will automatically respond to power events (shutdown, restart, suspend, resume, idle) for the respective devices.
7. Optionally, tweak additional settings, by clicking on the hamburger icon. Note that enabling logging can be very useful if you are facing any issues.

![optionswnd](https://user-images.githubusercontent.com/77928210/121790644-9dc5d400-cbe1-11eb-85a4-57e726ff10e9.JPG)
    
>if your OS is not localised in english, you must in the 'additional settings' dialog click the correct checkboxes to indicate what words refer to the system restarting/rebooting (as opposed to shutting down). This is needed because there is no better (at least known to me) way for a service to know if the system is being restarted or shut down than looking at a certain event in the event log. But the event log is localised, and this approach saves me from having to build a language table for all languages in the world. Note that if you don't do this on a non-english OS the application will not be able to determine if the system is being restarted or shut down. The difference is of course that the displays should not power off when the system is restarted.

8. Click Apply, to save the configuration file and automatically restart the service. 
    
9. At this point your respective WebOS TV will display a pairing dialog which you need to accept.

**All systems are now GO!** :+1:

10. Please go ahead and use the drop down menu again and select 'Test', to ensure that the displays properly react to power on/off commands.

## Limitations
- LG OLED displays cannot be turned on via network when an automatic pixel refresh is being performed. You can hear an internal relay click after the pixel refresh, when the display is actually powered down, at which point it can be turned on again at any time by this application.
- The WebOS displays can only be turned on/off when both the PC and the display is connected to a network. 
- The TV cannnot be on a different subnet/VLAN from your PC. This is because the TV is powerd on by means of broadcasting a magic packet, aka Wake-on-lan, which is restricted to layer 2, i.e. same subnet only. There are ways to bypass this limitation but it is outside the scope of this application, even though you can probably make it work. Let me know if you need help to make it work for you.

## Troubleshooting
If your display has trouble powering on, these are the things to check first:
- When connecting the TV via Wi-Fi it seems some users must enable "Quickstart+" and disable "HDD Eco mode" to avoid the NIC becoming inactive. (physical network cable does ot seem to need this)
- Try reconfiguring the device and use on of the other wak-on-lan network options.
- Ensure the network is not dropping WOL-broadcasts.
- The MAC-address configuration for the device in the application is erroneous.

If your display has trouble powering off it is most likely because:
- The IP configuration might be erroneous. Please check the configuration and make sure the TV has a static DHCP lease in your routers admin pages.
- The application has not yet received a pairing key. Try removing the device in the UI, click apply and then re-add the device to force re-pairing.

HOT tip! Enable the built in logger and check the output, it can be very useful for understanding where problems are.

## System requirements
- The application must be run in a modern windows environment, and any potato running Windows 10/11 is fine.
- A Local Area Network (LAN)

## Commandline arguments

*LGTV Companion.exe -[poweron|poweroff|autoenable|autodisable] [Device1|Name] [Device2|Name] ... [DeviceX|Name]*
- *-poweron* - power on a device.
- *-poweroff* - power off a device
- *-autoenable* - temporarily enable the automatic management of a device, i.e. to respond to power events. This is effective until next restart of the service. (I personally use this for my home automation system).
- *-autodisable* - temporarily disable the automatic management of a device, i.e. to respond to power events. This is effective until next restart of the service. 
- *[DeviceX|Name]* - device identifier. Either use Device1, Device2, ..., DeviceX or the friendly device name as determined in the User Interface, for example OLED48CX.

Example usage: LGTV Companion.exe -poweron Device1 Device2 "LG OLED48CX" -autodisable Device2

This command will power on device 1, device 2 and the device named LG OLED48CX, and additionally device2 is thereafter set to temporarily not respond to automatic power events (on/off). Note the usage of "quotes" for the device name.

## License
Copyright © 2021 Jörgen Persson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Thanks to
- @nlohmann - Niels Lohmann, author of JSON for Modern CPP https://github.com/nlohmann/json
- Boost libs - Boost and Beast https://www.boost.org/
- @Maassoft for initial inspo and helping me understand the WebSocket/WebOS comms - https://github.com/Maassoft

## Copyright
Copyright © 2022 Jörgen Persson
