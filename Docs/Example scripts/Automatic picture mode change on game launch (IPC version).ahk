/*
	Version 1.0
	
	This is a sample script using AutoHotKey (AHK) v2 for accessing the API for LGTV Companion. This sample script
	illustrate how to use the API to enforce automatic picture mode changes when a certain process (a game perhaps) is running. 
	
	This version illustrates how to acheive this using IPC.
	
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

; array of processes (games for example). This could potentially be adapted so that all processes started from within f.e c:/games will trigger, depending on how you want it to function.
global process_name_array := ["notepad.exe", "mspaint.exe"]

; global variables
global processRunning :=  false

;  Scan the running process every 5 seconds
SetTimer(CheckProcess, 5000) 

return ; 

; Function which will send a command to LGTV Companion via IPC when a process in process_name_array is currently running. 
CheckProcess() {
	global
	if (CheckProcessExist())
	{
		; Process is running
		if (!processRunning)
		{
            ; Process just started
			Send_LGTV_Message("-picturemode game device2")
			processRunning := true
		}
	}
	else if (processRunning)
	{
        ; Process just stopped
		Send_LGTV_Message("-picturemode expert1 device2")
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
; This function will send 'message' to the LGTV Companion service. Check the command line documentation to see all the messages/commands that can be sent. This function illustrate how to send via IPC rather than via command line. IPC allows for bidirectional communication and is more efficient since a new process does not need to be launched.
Send_LGTV_Message(message)
{
	; The name of the named pipe to connect to
	PipeName := "\\.\pipe\LGTVyolo"

	; Connect to the Named Pipe using CreateFile() https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
	Pipe := DllCall("CreateFile", "Str", PipeName, "UInt", 0x80000000 | 0x40000000, "UInt", 0x00000001 | 0x00000002, "Ptr", 0, "UInt", 0x00000003, "UInt", 0, "Ptr", 0)
	if(Pipe = -1)
	{
		MsgBox "Could not connect to the LGTV Companion service. Check that the service is running!"
		Return
	}
		
	;Write to the named pipe using WriteFile() https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile
	If !DllCall("WriteFile", "Ptr", Pipe, "Str", message, "UInt", (StrLen(message)+1)*2, "UInt*", 0, "Ptr", 0)
		MsgBox "WriteFile failed: %ErrorLevel%/%A_LastError%"
	;Tiny bit of delay so that closing the handle doesn't abort the (overlapped) read by the LGTV Companion Service
    Sleep 100
	;Close the handle
	DllCall("CloseHandle", "Ptr", Pipe)
	Return
}
