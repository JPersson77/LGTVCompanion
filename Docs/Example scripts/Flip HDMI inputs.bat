@echo off

set "CliPath=C:\Progra~1\LGTVCo~1\LGTVcli.exe"
set "queryArguments=-request com.webos.applicationManager/getForegroundAppInfo"  REM add device as needed
set "setHdmiAArguments=-setHdmi 1"  REM change input and add device as needed
set "setHdmiBArguments=-setHdmi 4"  REM change input and add device as needed

set "searchString=com.webos.app.hdmi"

set "output="

for /f "usebackq delims=" %%I in (`"%CliPath% %queryArguments%"`) do (
    echo %%I | findstr /C:"%searchString%" > nul
    if not errorlevel 1 (
        set "output=%%I"
		setlocal enabledelayedexpansion
        set "searchLength=!searchString:~0,-1!"
        set "result=!output:*%searchString%=!"
        set "input=!result:~0,1!"
		echo current input is: !input!
		if "!input!"=="1" (
            "%CliPath%" %setHdmiBArguments%
        ) else (
            "%CliPath%" %setHdmiAArguments%
        )
        endlocal
    )
)
