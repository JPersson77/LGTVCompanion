/*
	Version 1.1
	
	This is a sample script using AutoHotKey (AHK) v2 for accessing the API for LGTV Companion. This sample script
	illustrate how to query devices using the LGTV Companion command line interface. Note that bi-directional communication is
	supported and sending commands/events to LGTV Companion is covered in another example script.
	
	Please install AHK by downloading the installer from here: https://www.autohotkey.com/ and then install.
	
	AutoHotkey is a free, open-source scripting language for Windows that allows users to easily create small 
	to complex scripts for all kinds of tasks such as: form fillers, auto-clicking, macros, etc.
	
	PLEASE NOTE that the LGTV Companion API is using named pipes for its intra-process communication so any 
	scripting or programming language which can access named pipes (which is surely the vast majority) can be used 
	to communicate with LGTV Companion.
	
	For discussions, tips and tricks etc please use Discord: https://discord.gg/7KkTPrP3fq
	
*/

; SCRIPT STARTS HERE

; This script will run under v2 of AHK
#Requires AutoHotkey v2.0

;Only one instance of the script can run
#SingleInstance Force

global LGTV_Companion_path := "C:\Program Files\LGTV Companion\LGTV Companion.exe"
global LGTVcli_path := "C:\`"Program Files\LGTV Companion\LGTVcli.exe`""
global query := " -ok backlight -get_system_settings picture [\`"backlight\`"] device2"

;get the current value
val := RunAndGetStdOut(LGTVcli_path query)
;remove some trimming
val := StrReplace(val, "`n", " ")
val := StrReplace(val, "`r", " ")
;convert to  integer
global numeric_current_value := Number(val)

return

; create kotkey for decreasing the value (ctrl+shift+1)
^+1::
{
	global
	numeric_current_value -= 5
	if (numeric_current_value < 0)
	{
		numeric_current_value := 0
	}
	Run LGTV_Companion_path " -backlight " numeric_current_value " device2",,"Hide"
	return
}

; create kotkey for increasing the value (ctrl+shift+2)
^+2::
{
	global
	numeric_current_value += 5
	if (numeric_current_value > 100)
	{
		numeric_current_value := 100
	}
	Run LGTV_Companion_path " -backlight " numeric_current_value " device2",,"Hide"
}

; this function avoids that the console window shows when starting the script and querying the device
; unfortunately cmd.exe has a few quirks to work around so this looks a bit complicated
RunAndGetStdOut(command) {
    DetectHiddenWindows True
	pid := 0
	Run A_ComSpec,,"Hide", &pid
	WinWait "ahk_pid " pid,,10
	DllCall("AttachConsole", "UInt", pid)

    shell := ComObject("WScript.Shell")
	pp :=  A_ComSpec " /C " command 
;	MsgBox(pp)
    exec := shell.Exec(pp)

    DllCall( "FreeConsole" )
	ProcessClose pid
    return exec.StdOut.ReadAll()
}

