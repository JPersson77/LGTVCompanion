# Scripting API in LGTV Companion
LGTV Companion v2.2.0 and later allows external scripts and applications to act as clients and interact with LGTV Companion, and managed devices. The application provides asynchronous sending and receiving via an API as explained in this document.

## What can be acheived with the API?

External scripts and application can:
1. READ global power and system events, for example windows turning screens on/off, user idle, suspend, resume, reboot, shutdown.
2. SEND hundreds of assorted commands via LGTV Companion or the console command line interface LGTVcli controlling many aspects of the application and managed devices. Please see the command line documentation [here](https://github.com/JPersson77/LGTVCompanion/blob/master/Docs/Commandline.md)
3. READ the output of most commands mentioned in pt 2. In some cases WebOS does not provide a response. This can be used to f e query the devices about system, picture and audio settings. 

## What can not be achieved currently?
The following is not in final implementation:

1. Subscribing to be notified of the user changing settings (via remote or otherwise) is currently not in final implementation.

## How is the API implemented?
It is implemented using asynchronous named pipes, which means that most scripting- and programming languages can access it. Check the various examples available to learn more: [Link](https://github.com/JPersson77/LGTVCompanion/tree/master/Docs/Example%20scripts)

Please remember to enable the "External API" option in LGTV Companion to be able to read events from LGTV Companion.

## How to get support?
For discussions, tips and tricks etc please use Discord: https://discord.gg/7KkTPrP3fq
