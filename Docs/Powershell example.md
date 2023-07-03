# 1. Powershell example to set the backlight

- [1. Powershell example to set the backlight](#1-powershell-example-to-set-the-backlight)
  - [1.1. Script](#11-script)
  - [1.2. Shortcuts](#12-shortcuts)
    - [1.2.1. Increase](#121-increase)
    - [1.2.2. Decrease](#122-decrease)

Following script can be used to increase or decrease the OLED backlight level. It needs to be called with and argument `increase` or `decrease` and set the backlight level accordingly between `0-100` at a rate of 10.

Ideally you would make a shortcut to the script for both arguments and place it on your task bar. In this way you could just run the script with the key combination `WINDOWS + NUM`.

`NUM` would be the row number of the shortcut on the taskbar. Mine is set to 8th and 9th row, so can I increase or decrease the backlight level with `WINDOWS + 8` or `WINDOWS + 9`.

## 1.1. Script

[adjustBacklight.ps1](./Example%20scripts/adjustBacklight.ps1)

```Powershell
param (
    [Parameter(Mandatory=$true)]
    [ValidateScript({$_ -eq "increase" -or $_ -eq "decrease"})]
    $scriptArgument
)

New-Variable -Name rate -Value ([int]10) -Option Constant

$executablePath = "C:\Program Files\LGTV Companion\LGTVcli.exe"
$arguments = "-ok", "backlight", "-get_system_settings", "picture", "[`\`"backlight`\`"]"

Function ExecuteCommand ($commandPath, $commandArguments)
{
  Try {
    $pinfo = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.FileName = $commandPath
    $pinfo.RedirectStandardError = $true
    $pinfo.RedirectStandardOutput = $true
    $pinfo.UseShellExecute = $false
    $pinfo.Arguments = $commandArguments
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $pinfo
    $p.Start() | Out-Null
    [pscustomobject]@{
        stdout = $p.StandardOutput.ReadToEnd().Trim()
        stderr = $p.StandardError.ReadToEnd().Trim()
        ExitCode = $p.ExitCode
    }
    $p.WaitForExit()
  }
  Catch {
     exit
  }
}


# Execute the binary and capture the output
$process = ExecuteCommand -commandPath $executablePath -commandArguments $arguments

# Get the output value
$value = [int]($process.stdout)

switch ($scriptArgument) {
    "increase" {
        if ($value -lt 100) {
            $value += $rate
        } else {
            Write-Host "Already set to 100"
            exit 0
        }
    }

    "decrease" {
        if ($value -gt 0) {
            $value -= $rate
        } else {
            Write-Host "Already set to 0"
            exit 0
        }
    }

    Default {
        Write-Host "Invalid argument!"
        exit 1
    }
}

$argument = "-backlight $value"

$process = ExecuteCommand -commandPath $executablePath -commandArguments $argument

# Check the return status of the executable
$exitCode = $process.ExitCode

if ($exitCode -eq 0) {
    Write-Host "Backlight is set to: $value"
} else {
    Write-Host "Executable encountered an error. Return code: $exitCode"
}

```

## 1.2. Shortcuts

### 1.2.1. Increase

```
Powershell -ExecutionPolicy Bypass -File "C:\Program Files\LGTV Companion\adjustBacklight.ps1" -scriptArgument "increase"
```

### 1.2.2. Decrease

```
Powershell -ExecutionPolicy Bypass -File "C:\Program Files\LGTV Companion\adjustBacklight.ps1" -scriptArgument "decrease"
```