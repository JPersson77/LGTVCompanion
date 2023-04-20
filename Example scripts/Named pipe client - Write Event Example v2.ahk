/*
	Version 1.0
	
	This is a sample script using AutoHotKey (AHK) for accessing the API for LGTV Companion. This sample script
	illustrate how to send events to LGTV Companion. Note that bi-directional communication is
	supported and receiving commands/events to LGTV Companion is covered in another example script.
	
	Please install AHK by downloading the installer from here: https://www.autohotkey.com/ and then install.
	
	AutoHotkey is a free, open-source scripting language for Windows that allows users to easily create small 
	to complex scripts for all kinds of tasks such as: form fillers, auto-clicking, macros, etc.
	
	PLEASE NOTE that the LGTV Companion API is using named pipes for its intra-process communication so any	scripting or programming language which can access named pipes (which is surely the vast majority) can be used 
	to communicate with LGTV Companion.
	
	Remember to enable the "External API" option in LGTV Companion.
	
	For discussions, tips and tricks etc please use Discord: https://discord.gg/7KkTPrP3fq
	
*/

; SCRIPT STARTS HERE

; This script will run under v2 of AHK
#Requires AutoHotkey v2.0

;Only one instance of the script can run
#SingleInstance Ignore

; Set keyboard shortcut CTRL+SHIFT+M to mute TV-speaker (Device1)
^+m::
{
	Send_LGTV_Message("-mute device1")
	return
}

; Set keyboard shortcut CTRL+SHIFT+U to unmute TV-speaker (Device1)
^+u::
{
	Send_LGTV_Message("-unmute device1")
	return
}

; This function will send 'message' to the LGTV Companion service (currently the messages that can be sent correspond to the command line parameters)
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

