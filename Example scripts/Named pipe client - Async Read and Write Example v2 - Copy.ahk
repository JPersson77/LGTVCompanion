/*
	Version 1.1
	
	This is a sample script using AutoHotKey (AHK) v2 for accessing the API for LGTV Companion. This sample script
	illustrate how to listen to events sent from LGTV Companion. Note that bi-directional communication is
	supported and sending commands/events to LGTV Companion is covered in another example script.
	
	Please install AHK by downloading the installer from here: https://www.autohotkey.com/ and then install.
	
	AutoHotkey is a free, open-source scripting language for Windows that allows users to easily create small 
	to complex scripts for all kinds of tasks such as: form fillers, auto-clicking, macros, etc.
	
	PLEASE NOTE that the LGTV Companion API is using named pipes for its intra-process communication so any 
	scripting or programming language which can access named pipes (which is surely the vast majority) can be used 
	to communicate with LGTV Companion.
	
	Remember to enable the "External API" option in LGTV Companion.
	
	For discussions, tips and tricks etc please use Discord: https://discord.gg/7KkTPrP3fq
	
*/

; SCRIPT STARTS HERE

; This script will run under v2 of AHK
#Requires AutoHotkey v2.0

;Only one instance of the script can run
#SingleInstance Ignore

; The following DllCall() is optional: it tells the OS to shut down this script last (after all other applications).
DllCall("kernel32.dll\SetProcessShutdownParameters", "UInt", 0x101, "UInt", 0)

; The name of the named pipe to connect to
PipeName := "\\.\pipe\LGTVyolo"

;; Create event objects for the overlapped structure
read_event := DllCall("CreateEvent", "ptr", 0, "int", 1, "int", 0, "ptr", 0)
stop_event := DllCall("CreateEvent", "ptr", 0, "int", 1, "int", 0, "ptr", 0)

; Create a handle to the named pipe server with overlapped flag
pipe := DllCall("CreateFile", "str", PipeName, "uint", 0xC0000000, "uint", 0x3, "ptr", 0, "uint", 0x3, "uint", 0x40000000, "ptr", 0)

; Check if the handle is valid
if (pipe = -1)
{
    MsgBox("Pipe handle is invalid. Script will Terminate!")
    ExitApp
}

; Initialize an OVERLAPPED structure with the event handle
overlap := Buffer(32,0)
; NumPut("type", value, Buffer, offset)
NumPut("Ptr",read_event, overlap, 24)

; Initialize variables
VarSetStrCapacity(&buff , 1024)
bytesread := 0
WritePerformed := false

; Create an array of event handles
handles := [read_event, stop_event, pipe]


; Read data from the pipe server asynchronously (overlapped)
DllCall("ReadFile", "ptr", pipe, "str", buff, "uint", 1024, "uint*", 0, "Ptr", overlap)

;Loop until the pipe is closed or an error occurs
loop
{
    ; Wait for the read operation or the pipe handle to be signaled
    result := DllCall("WaitForMultipleObjects", "uint", 3, "ptr", handles[1], "int", 0, "uint", 0xFFFFFFFF) ; wait indefinitely

    ; Check the result of the wait
    if (result = 0) ; READ or WRITE event
    {
        ; Get the number of bytes read
        if(!DllCall("GetOverlappedResult", "ptr", pipe, "ptr", overlap, "uint*", bytesread, "int", 0))
		{
			Log("ERROR, GetOverlappedResult() failed. Script will Terminate!")
			break
		}
		
		overlap := Buffer(32,0)
		; NumPut("type", value, Buffer, offset)
		NumPut("Ptr",read_event, overlap, 24)

		; Read data from the pipe server asynchronously (overlapped)
		DllCall("ReadFile", "ptr", pipe, "ptr", &buff, "uint", 256, "uint*", 0, "ptr", overlap)
		
        ; Display the data read
        MsgBox(buffer)
    }
    else if (result = 1) ; STOP event
    {
        ; The pipe handle was signaled, meaning it was closed by the server
        MsgBox("The pipe server has closed the connection.")
        break ; exit the loop
    }
    else ; WAIT_FAILED
    {
        MsgBox("Failed to wait for data: %A_LastError%")
        break ; exit the loop
    }
}

; Close the handle and the event object
DllCall("CloseHandle", "ptr", pipe)
DllCall("CloseHandle", "ptr", read_event)
DllCall("CloseHandle", "ptr", stop_event)


/*

; The following DllCall() is optional: it tells the OS to shut down this script last (after all other applications).
DllCall("kernel32.dll\SetProcessShutdownParameters", "UInt", 0x101, "UInt", 0)

; The name of the named pipe to connect to
PipeName := "\\.\pipe\LGTVyolo"

; Connect to the Named Pipe using CreateFile() https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
Pipe := DllCall("CreateFile", "Str", PipeName, "UInt", 0x80000000 | 0x40000000, "UInt", 0, "Ptr", 0, "UInt", 0x00000003, "UInt", 0x00000080, "Ptr", 0)

; Set the max message length to 1024 bytes (maximum supported currently)
VarSetStrCapacity(&Buff , 1024)
Bytes := 0

;read messages in an infinite loop	
Go := true
while (Go = true) 
{		
	if (Pipe != -1)
	{
		; Read synchronously (i.e. blocking) using ReadFile() https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
		if (DllCall("ReadFile", "Ptr", Pipe, "Str", Buff, "UInt", 512, "UInt*", Bytes, "Ptr", 0))
		{
			Log(Buff)
			
			Switch Buff
			{			
				Case "SYSTEM_DISPLAYS_OFF": ; System signals displays OFF
				{
					; Do stuff here
				}
				Case "SYSTEM_DISPLAYS_OFF_DIMMED": ; System signals displays OFF, BUT DIMMED (usually occurs in laptops)
				{
					; Do stuff here
				}
				Case "SYSTEM_DISPLAYS_ON": ; System signals displays ON
				{
					; Do stuff here
				}
				Case "SYSTEM_USER_BUSY": ; System is busy / user away mode is inactive
				{
					; Do stuff here
				}
				Case "SYSTEM_USER_IDLE": ; System is idle / user away mode is active
				{
					; Do stuff here
				}
				Case "SYSTEM_REBOOT": ; System is about to reboot
				{
					; Do stuff here
				}
				Case "SYSTEM_SHUTDOWN": ; System is about to shutdown
				{
					; Do stuff here
				}
				Case "SYSTEM_RESUME": ; System is resuming from sleep/hibernate
				{
					; Do stuff here
				}
				Case "SYSTEM_SUSPEND": ; System is suspending to sleep/hibernate
				{
					; Do stuff here			
				}
				Default:
				{
				}		
			}
		}
		else
		{
			Log("ERROR, Failed to ReadFile(). Script will Terminate!")
			Go := false
		}
	}
	else
	{
		Log("ERROR, Pipe handle is invalid. Script will Terminate!")
		Go := false
	}
}

; Close the handle
DllCall("CloseHandle", "Ptr", Pipe)
*/
ExitApp

; This function will write 'message' to log.txt in the same directory as the script
Log(message)
{
	FileAppend a_hour . ":" . a_min . ":" . a_sec . " > " . message "`n", "log.txt"
}

