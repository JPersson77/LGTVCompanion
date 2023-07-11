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
