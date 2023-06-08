/*
	Version 1.0
	
	This is a sample script using AutoHotKey (AHK) v2 for accessing the API for LGTV Companion. This sample script
	illustrate how to use the API to enforce automatic picture mode changes when a certain process (a game perhaps) is running. 
	
	This version illustrates how to acheive this using command line.
	
	AutoHotkey is a free, open-source scripting language for Windows that allows users to easily create scripts for many kinds of tasks such as: form fillers, auto-clicking, macros, etc. Please install AHK by downloading the installer from here: https://www.autohotkey.com/ and then install.
	
	PLEASE NOTE that the bidirectional communication of LGTV Companion is using named pipes for its intra-process 
	communication so ANY scripting or programming language which can access named pipes (which is surely the vast majority) 
	can be used to communicate with LGTV Companion. Additionally, normal command line works just as well for simply issuing
	a command to LGTV Companion, and any devices managed by LGTV Companion.
	
	For discussions, tips and tricks etc please use Discord: https://discord.gg/7KkTPrP3fq	
*/

; SCRIPT STARTS HERE

; This script will run under v2 of AHK
#Requires AutoHotkey v2.0

;Only one instance of the script can run
#SingleInstance Ignore

; array of processes (games for example). This could potentially be changed so that all processes started from within f.e c:/games will trigger, depending on how you want it to function.
global process_name_array := ["notepad.exe", "mspaint.exe"]

; global variables
global LGTV_companion_dir := "C:\Program Files\LGTV Companion"
global processRunning :=  false

;  Scan the running process every 5 seconds
SetTimer(CheckProcess, 5000) 

return 

; Function which will send a command to LGTV Companion when a process in process_name_array is currently running
CheckProcess() {
	global
	if (CheckProcessExist())
	{
		; Process is running
		if (!processRunning)
		{
            ; Process just started so send a -picturemode command
			Run "LGTV Companion.exe -picturemode game device1", LGTV_companion_dir
			processRunning := true
		}
	}
	else if (processRunning)
	{
        ; Process just stopped so revert to default picture mode
		Run "LGTV Companion.exe -picturemode expert1 device1", LGTV_companion_dir
		processRunning := false
	}
}
; Function which will check if a process in process_name_array is currently running
CheckProcessExist() {
	global
	For k, value In process_name_array
	{
		If(ProcessExist(value)){
			return true
		}
	}
	return false
}