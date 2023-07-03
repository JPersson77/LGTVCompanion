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
