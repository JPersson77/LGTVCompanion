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

;; Create an event object for the overlapped structure
event := DllCall("CreateEvent", "ptr", 0, "int", 1, "int", 0, "ptr", 0)

; Create a handle to the named pipe server with overlapped flag
pipe := DllCall("CreateFile", "str", PipeName, "uint", 0xC0000000, "uint", 0x3, "ptr", 0, "uint", 0x3, "uint", 0x40000000, "ptr", 0)

; Check if the handle is valid
if (pipe = -1)
{
    MsgBox("ERROR, Pipe handle is invalid. Script will Terminate!")
    ExitApp
}

; Initialize an OVERLAPPED structure with the event handle
ol := BufferAlloc(A_PtrSize * 4 + 8)
ol.hEvent := event

; Initialize variables
VarSetStrCapacity(&buffer , 1024)
bytesread := 0

; Create an array of handles to wait for
handles := [event, pipe]

; Loop until the pipe is closed or an error occurs
loop
{
    ; Read data from the pipe server asynchronously
    DllCall("ReadFile", "ptr", pipe, "ptr", &buffer, "uint", 256, "uint*", bytesread, "ptr", &ol)

    ; Wait for the read operation or the pipe handle to be signaled
    result := DllCall("WaitForMultipleObjects", "uint", handles.Length(), "ptr", &handles[1], "int", 0, "uint", 0xFFFFFFFF) ; wait infinitely

    ; Check the result of the wait
    if (result = 0) ; WAIT_OBJECT_0
    {
        ; Get the number of bytes read
        DllCall("GetOverlappedResult", "ptr", pipe, "ptr", &ol, "uint*", read, "int", 0)
        
        ; Display the data read
        MsgBox(StrGet(&buffer, read))
    }
    else if (result = 1) ; WAIT_OBJECT_0 + 1
    {
        ; The pipe handle was signaled, meaning it was closed by the server
        MsgBox("The pipe server has closed the connection.")
        break ; exit the loop
    }
    else if (result = 258) ; WAIT_TIMEOUT
    {
        MsgBox("Timed out waiting for data.")
    }
    else ; WAIT_FAILED
    {
        MsgBox("Failed to wait for data: %A_LastError%")
        break ; exit the loop
    }
}

; Close the handle and the event object
DllCall("CloseHandle", "ptr", pipe)
DllCall("CloseHandle", "ptr", event)





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

ExitApp

; This function will write 'message' to log.txt in the same directory as the script
Log(message)
{
	FileAppend a_hour . ":" . a_min . ":" . a_sec . " > " . message "`n", "log.txt"
}

