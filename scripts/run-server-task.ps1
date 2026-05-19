# Wrapper for Task Scheduler: runs pocket-audio-server in the user session with a log file.
param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath
)

$ErrorActionPreference = "Stop"
$wd = Split-Path -Parent $ExePath
$logDir = Join-Path $env:LOCALAPPDATA "PocketAudio"
$logFile = Join-Path $logDir "server.log"

if (-not (Test-Path $ExePath)) {
    "$(Get-Date -Format o) ERROR: missing $ExePath" | Out-File $logFile -Append
    exit 1
}

New-Item -ItemType Directory -Force -Path $logDir | Out-Null
Set-Location $wd

"$(Get-Date -Format o) starting $ExePath" | Out-File $logFile -Append

# Run in this process so the task stays alive and WASAPI stays in your logon session.
& $ExePath *>> $logFile 2>&1
$code = $LASTEXITCODE
if ($null -eq $code) { $code = 0 }

"$(Get-Date -Format o) exited code $code" | Out-File $logFile -Append
exit $code
