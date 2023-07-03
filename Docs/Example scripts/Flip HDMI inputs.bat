@echo off

set "CliPath=C:\Progra~1\LGTVCo~1\LGTVcli.exe"
set "queryArguments=-request com.webos.applicationManager/getForegroundAppInfo" 
set "setHdmiArguments=-setHdmi" 
set "searchString=com.webos.app.hdmi"

for /f "usebackq delims=" %%I in (`"%CliPath% %queryArguments%"`) do (
    echo %%I | findstr /C:"%searchString%" > nul
    if not errorlevel 1 (
        set "output=%%I"
        setlocal enabledelayedexpansion
        set "result=!output:*%searchString%=!"
        set "input=!result:~0,1!"
        echo current input is: !input!
        if "!input!"=="1" (
            "%CliPath%" %setHdmiArguments% "4"
        ) else (
            "%CliPath%" %setHdmiArguments% "1"
        )
        endlocal
    ) else (
	echo no HDMI-input is currently active.
    )
)
