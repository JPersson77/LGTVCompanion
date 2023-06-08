# Scripting API in LGTV Companion
LGTV Companion v2.2.0 and later allows external scripts and applications to act as clients and interact with LGTV Companion, and managed devices. The application provides asynchronous sending and receiving via an API as explained in this document.

## What can be acheived with the API?
1. Global power and system events, for example windows turning screens on/off, user idle, suspend, resume, reboot, shutdown can be READ by external scripts or applications.
2. External scripts or applications can SEND hundreds of assorted commands to LGTV Companion (please see the command line documentation here) controlling many aspects of the application and managed devices.

## What can it not do?
The following is not in final implementation:

Directly reading the response from managed LG devices, f e to get current power state, volume level, channel etc

## How is the API implemented?
It is implemented using asynchronous named pipes, which means that most scripting- and programming languages can access it. Check the various examples available here to learn more:

Please remember to enable the "External API" option in LGTV Companion to be able to read events from LGTV Companion.

## How to get support?
For discussions, tips and tricks etc please use Discord: https://discord.gg/7KkTPrP3fq