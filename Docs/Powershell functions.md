# Powershell example functions

Following functions are definedin the file [Microsoft.PowerShell_profile.ps1](./Example%20scripts/Microsoft.PowerShell_profile.ps1)

- getBacklight
- setBacklight [0-100]
- getVolume
- setVolume [0-100]
- volumeUp
- volumeDown

If you don't have this file already, just make a folder called `WindowsPowerShell` in your `C:\Users\<username>\Documents` and copy the file [Microsoft.PowerShell_profile.ps1](./Example%20scripts/Microsoft.PowerShell_profile.ps1) into it. The functions will be then usable in the every new powershell session.

# Functions

```Powershell
function getBacklight()
{
    $value = & 'C:\Program Files\LGTV Companion\LGTVcli.exe' -ok backlight -get_system_settings picture [`\`"backlight`\`"]
    Write-Host "Backlight is set to $value"
}

function setBacklight([int]$value)
{
    & 'C:\Program Files\LGTV Companion\LGTVcli.exe' -backlight "$value" | Out-Null
    Write-Host "Backlight set to $value"
}

function volumeUp()
{
    & 'C:\Program Files\LGTV Companion\LGTVcli.exe' -request com.webos.service.audio/master/volumeUp | Out-Null
}

function volumeDown()
{
    & 'C:\Program Files\LGTV Companion\LGTVcli.exe' -request com.webos.service.audio/master/volumeDown | Out-Null
}


function getVolume()
{
    $jsonObject = (& 'C:\Program Files\LGTV Companion\LGTVcli.exe' -request com.webos.service.audio/master/getVolume)|ConvertFrom-json
    $volume=$jsonObject.Device1.payload.volumeStatus.volume
    Write-Host "Volume is set to $volume"
}

function setVolume([int]$value)
{
    & 'C:\Program Files\LGTV Companion\LGTVcli.exe' -request_with_param com.webos.service.audio/master/setVolume "{\`"volume\`":$value}" | Out-Null
    Write-Host "Volume set to $value"
}
```