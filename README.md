# LGTV Companion

## Download and install instructions
**The installer for the latest version can be downloaded from the [Releases](https://github.com/JPersson77/LGTVCompanion/releases) page. Alternatively you can use Winget via Windows Terminal/PowerShell to install: `winget install LGTVCompanion`**

>Please note that Windows Defender SmartScreen may block the download. There is no way for developers to directly influence which apps will show up on SmartScreen. If the installer will not download you must therefore instruct your web browser to download it regardless and if the installer does not run you must unblock the installer. Unblocking is done by right clicking the downloaded installer, clicking on properties, under general and at the bottom security section check the "Unblock" checkbox. You can help prevent this in the future by reporting this application as safe to Microsoft when downloading the installer.

[![SLSA3](https://img.shields.io/badge/SLSA-level%203-green?logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAA4AAAAOCAMAAAAolt3jAAAABGdBTUEAALGPC/xhBQAAACBjSFJNAAB6JgAAgIQAAPoAAACA6AAAdTAAAOpgAAA6mAAAF3CculE8AAABMlBMVEXvMQDvMADwMQDwMADwMADvMADvMADwMADwMQDvMQDvMQDwMADwMADvMADwMADwMADwMQDvMQDvMQDwMQDvMQDwMQDwMADwMADwMQDwMADwMADvMADvMQDvMQDwMADwMQDwMADvMQDwMADwMQDwMADwMADwMADwMADwMADwMADvMQDvMQDwMADwMQDwMADvMQDvMQDwMADvMQDvMQDwMADwMQDwMQDwMQDvMQDwMADvMADwMADwMQDvMQDwMADwMQDwMQDwMQDwMQDvMQDvMQDvMADwMADvMADvMADvMADwMQDwMQDvMADvMQDvMQDvMADvMADvMQDwMQDvMQDvMADvMADvMADvMQDwMQDvMQDvMQDvMADvMADwMADvMQDvMQDvMQDvMADwMADwMQDwMAAAAAA/HoSwAAAAY3RSTlMpsvneQlQrU/LQSWzvM5DzmzeF9Pi+N6vvrk9HuP3asTaPgkVFmO3rUrMjqvL6d0LLTVjI/PuMQNSGOWa/6YU8zNuDLihJ0e6aMGzl8s2IT7b6lIFkRj1mtvQ0eJW95rG0+Sid59x/AAAAAWJLR0Rltd2InwAAAAlwSFlzAAAOwwAADsMBx2+oZAAAAAd0SU1FB+YHGg0tGLrTaD4AAACqSURBVAjXY2BgZEqGAGYWVjYGdg4oj5OLm4eRgZcvBcThFxAUEk4WYRAVE09OlpCUkpaRTU6WY0iWV1BUUlZRVQMqUddgSE7W1NLS1gFp0NXTB3KTDQyNjE2Sk03NzC1A3GR1SytrG1s7e4dkBogtjk7OLq5uyTCuu4enl3cyhOvj66fvHxAIEmYICg4JDQuPiAQrEmGIio6JjZOFOjSegSHBBMpOToxPAgCJfDZC/m2KHgAAACV0RVh0ZGF0ZTpjcmVhdGUAMjAyMi0wNy0yNlQxMzo0NToyNCswMDowMC8AywoAAAAldEVYdGRhdGU6bW9kaWZ5ADIwMjItMDctMjZUMTM6NDU6MjQrMDA6MDBeXXO2AAAAGXRFWHRTb2Z0d2FyZQB3d3cuaW5rc2NhcGUub3Jnm+48GgAAAABJRU5ErkJggg==)](https://slsa.dev) 

*The committment to quality and security is demonstrated by SLSA (Supply-chain Levels for Software Artifacts) Level 3 compliance*

## Overview
This application (UI and service) controls LG WebOS TVs and displays.

This application aim to be a set-and-forget application to provide:
- automatic management for your WebOS device (TV), to shut off and turn on in reponse to the PC shutting down, rebooting, entering low power modes and when user is idle as well as in response to changes in the windows monitor topology, in a multi-monitor environment.
- a powerful command line tool to control your WebOS device and change most of its settings.
- an API, which external scripts and/or applications can use to facilitate automation.

## Background
With the rise in popularity of using OLED TVs as PC monitors, it is apparent that standard functionality of PC-monitors is missing. Particularly turning the device on or off in response to power events in windows. With OLED monitors this is particularly important to prevent "burn-in", or more accurately pixel-wear.

## What other people say

- *"That is a really janky solution. But... it WORKS!"* - Linus Tech Tips at [YouTube](https://youtu.be/4mgePWWCAmA?t=21m14s)
- *"OLED life saver"* - Way of the Wrench at [YouTube](https://youtu.be/qUiH_btH6NY)
- *"The best kind of programming is fueled by pure hate for an annoying situation."* - reddituser at [Reddit](https://www.reddit.com/r/OLED_Gaming/comments/okhv67/comment/h58alyu/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button)
- *"Yeah, that's nice..." - my wife

## Installation and usage
1. Important prerequisites:
   - Power ON all TVs and ensure they are connected to your local area network via Wi-Fi or cable.
   - Ensure that the TV can be woken via the network. For the CX line of displays this is accomplished by navigating to Settings (cog button on remote)->All Settings->Connection->Mobile Connection Management->TV On with Mobile, and then enable 'Turn On via Wi-Fi'. For C1, C2, C3 and C4 it's All Settings->General->Devices->External Devices->TV On With Mobile->Turn on via Wi-Fi. NOTE! This step is needed regardless of using WiFi or a cable.
   - Open the administrative interface of your router, and set a static DHCP lease for your WebOS devices, i.e. to ensure that your devices always have the same IP-addresses on your LAN.

🔥HOT TIP! While in the settings of the TV, ensure that the device automatic power off is set long enough to not interfere with your sessions with the PC, f e 8 hours. Note that for the C2 displays this setting can be found here: All settings->General->OLED Care->Device Self-Care->Energy Saving->Auto Power off. LGTV Companion will manage the power state of the display and the less interference the better.

2. Download the latest version of the setup package from the releases page ( [Click here](https://github.com/JPersson77/LGTVCompanion/releases) ) and install it. This will install and start the service (LGTVsvc.exe), install the user interface (LGTV Companion.exe) as well as the desktop user mode daemon (LGTVdaemon.exe).
3. Open the user interface from the Windows start menu, it is called "LGTV Companion".

![l_main](https://user-images.githubusercontent.com/77928210/213521847-31f66061-629f-4808-a20c-1bb7bbf441ca.jpg)

4. Click the 'Scan' button to let the application try and automatically find network attached WebOs devices (TVs) (This button is called 'Configure' in the screenshot above). If the 'Scan' feature is unable to find your device(s) you are able to add your devices manually.
5. Optionally, click the drop down button to manually add, remove, configure devices and the parameters of the respective device, this includes the network IP-address, the physical address, i.e. the MAC(s). This information can easily be found in the network settings of the TV. Also, the default wake-on-lan network options should work for most configurations, but if your TV has difficulties powering on then try the other options. 

🔥HOT TIP! Clicking the 'What's this?' links will show you more information about all options throughout the UI.

![l_dev](https://user-images.githubusercontent.com/77928210/213522732-ee6c737c-c12e-4f32-b638-863e875151e2.jpg)

6. In the main application window, ensure the 'Automatically manage this device' checkbox is checked so the application will automatically respond to power events (shutdown, restart, suspend, resume, idle) for the respective devices.
7. Optionally, tweak the global settings, by clicking on the hamburger icon button (settings). 

🔥HOT TIP! The "User Idle Mode" works seprately from all other windows power options and can be really useful to provide maximum protection against screen burn-in and also some additional power savings.

![l_opt](https://user-images.githubusercontent.com/77928210/213523626-e54dc98e-9ea2-4f3c-ac55-fe2ad7c1088e.jpg)
    
>if your OS is not localised in english, and the built in dictionary does not include your language, you must in the 'shutdown settings' dialog click the correct checkboxes to indicate what words refer to the system restarting/rebooting (as opposed to shutting down). This is needed because there is no better (at least known to me) way for a service to know if the system is being restarted or shut down than looking at a certain event in the event log. Contribute to building the dictionary here: [Link](https://docs.google.com/spreadsheets/d/1SNjYh7kt4aVhHg0ZjI3i96vCeOJZJS73o9Wl-OVEvzk/edit?usp=drive_link).

8. Click Apply, to save the configuration file and automatically restart the service. 
    
9. At this point your respective WebOS TV will display a pairing dialog which you need to accept.

**All systems are now GO!** :+1:

10. Please go ahead and use the drop down menu again and select 'Test', to ensure that the devices properly react to power on/off commands.

11. You can now close the main UI. The service will continue running in the background.

## Limitations
- Some LG OLED displays can sometimes not be turned on via network when an automatic pixel refresh is being performed. You can on some models hear an internal relay click after the pixel refresh, when the display is actually powered down, at which point it can be turned on again at any time by this application.
- The WebOS device can only be turned on/off when the PC and the device are both connected to a network. 
- The TV cannnot be on a different subnet/VLAN from your PC. This is because the TV is powerd on by means of broadcasting a magic packet, aka Wake-on-lan, which is restricted to layer 2, i.e. same subnet only. There are ways to bypass this limitation but it is outside the scope of this application, even though you can probably make it work. Let me know if you need help to make it work for you.

## Troubleshooting
*If your display has trouble powering on*, these are the things to check first:
- "Turn on via WiFi" must be enabled in the TV configuration, regardless of using WiFI or Ethernet. Read more under "1. Important prerequisites" above.
- When connecting the TV via Wi-Fi it seems some users must enable "Quickstart+" (up to 2021 models) or "Always ready" (2022 and forward models) and disable "HDD Eco mode" to avoid the NIC becoming inactive. (physical network cable does not seem to need this)
- Confirm that the device is properly configured (i e IP and MAC) and try to use one of the other wake-on-lan network options, primarily use option two, send to IP-address.
- Ensure the network is not dropping WOL-broadcasts.
- If the connection between the TV and the Router and/or access point is lost for any reason (e.g. router reboot, power outage, firmware update, etc.) while the TV is off, the TV might not automatically reconnect and therefore won't react to attempts to turn it on via Wi-Fi or Ethernet. You may therefore need to turn the TV on manually at least once so that it can reconnect to your network.
- In case of issues with devices not turning on in response to changes in the windows monitor topology configuration ensure that "Always ready" (2022 and forward models) is enabled.
- Also note that a manual power off via remote and/or automatic display power off (Settings->General->OLED Care->Device Self-Care->Energy Saving->Auto Power off) can sometimes cause a situation where you also need to power on the display with the remote. 

🔥HOT TIP! Aim to configure the app to cover your use case and let the remote be, and also set a long enough timeout for the device built-in automatic power-off to never interfere with the length of your typical session with the PC.

*If your display has trouble powering off* it is most likely because:
- The IP configuration might be erroneous. Verify the configuration and make sure the TV has a static DHCP lease in your routers admin pages.
- The application has not yet received a pairing key. Try removing the device in the UI, click apply and then re-add the device to force re-pairing.

🔥HOT tip! Enable the built in logger and check the output, it can be very useful for understanding potential issues with the functionality.

## System requirements
- The application must be run in a modern windows environment, and any potato running Windows 10 or 11 is fine.
- A Local Area Network (LAN)

## Commandline arguments

 'LGTV companion.exe" can accept hundreds of command line arguments for controlling the application and managed devices. Please see the documentation for all command line arguments here: [Command line documentation](https://github.com/JPersson77/LGTVCompanion/blob/master/Docs/Commandline.md)

## License
Copyright © 2021-2025 Jörgen Persson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Discussions

Discord: https://discord.gg/7KkTPrP3fq

Please use the Github issue tracker for potential bug reports and/or feature requests

## Thanks to
- Boost libs - Boost and Beast https://www.boost.org/
- @nlohmann - Niels Lohmann, author of JSON for Modern CPP https://github.com/nlohmann/json
- @chros73 - for thorough documentation of the api https://github.com/chros73
- @Maassoft - for initial help with understanding the WebOS comms - https://github.com/Maassoft
- @mohabouje - Mohammed Boujemaoui - Author of WinToast https://github.com/mohabouje/WinToast
- OpenSSL - https://www.openssl.org/
- Vcpkg - https://github.com/microsoft/vcpkg
- Contributors
- Donors and supporters

## Donations

This is free software, but your support is appreciated and there is a donation page set up over at PayPal. PayPal allows you to use a credit- or debit card or your PayPal balance to make a donation.

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.me/jpersson77)

## Copyright
Copyright © 2021-2025 Jörgen Persson
