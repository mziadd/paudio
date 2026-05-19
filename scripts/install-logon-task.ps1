# Registers PocketAudio to start at user logon (recommended on Windows — WASAPI works).
# Usage:
#   .\install-logon-task.ps1
#   .\install-logon-task.ps1 -ExePath "C:\path\pocket-audio-server.exe" -StartNow -AddFirewallRule

param(
    [string]$ExePath = "",
    [switch]$StartNow,
    [switch]$AddFirewallRule
)

$ErrorActionPreference = "Stop"
$TaskName = "PocketAudio"
$ScriptsDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RunnerScript = Join-Path $ScriptsDir "run-server-task.ps1"

function Find-Exe {
    if ($ExePath -and (Test-Path $ExePath)) {
        return (Resolve-Path $ExePath).Path
    }
    $candidates = @(
        (Join-Path $ScriptsDir "..\build\Release\pocket-audio-server.exe"),
        (Join-Path $ScriptsDir "..\build\pocket-audio-server.exe"),
        (Join-Path $ScriptsDir "..\pocket-audio-server.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }
    return $null
}

$ExePath = Find-Exe
if (-not $ExePath) {
    Write-Error @"
Could not find pocket-audio-server.exe.
Build first, then run:
  .\install-logon-task.ps1 -ExePath 'C:\full\path\pocket-audio-server.exe'
"@
}

if (-not (Test-Path $RunnerScript)) {
    Write-Error "Missing $RunnerScript"
}

$ExePath = (Resolve-Path $ExePath).Path
$RunnerScript = (Resolve-Path $RunnerScript).Path

# Hidden PowerShell hosts the server and writes logs (no extra console window).
$psArgs = @(
    "-NoProfile",
    "-WindowStyle", "Hidden",
    "-ExecutionPolicy", "Bypass",
    "-File", "`"$RunnerScript`"",
    "-ExePath", "`"$ExePath`""
) -join " "

$action = New-ScheduledTaskAction `
    -Execute "powershell.exe" `
    -Argument $psArgs `
    -WorkingDirectory (Split-Path -Parent $ExePath)

# Interactive logon = same session as your desktop audio (required for loopback capture).
$trigger = New-ScheduledTaskTrigger -AtLogOn

$principal = New-ScheduledTaskPrincipal `
    -UserId $env:USERNAME `
    -LogonType Interactive `
    -RunLevel Limited

$settings = New-ScheduledTaskSettingsSet `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries `
    -StartWhenAvailable `
    -RestartCount 3 `
    -RestartInterval (New-TimeSpan -Minutes 1) `
    -ExecutionTimeLimit ([TimeSpan]::Zero)

Register-ScheduledTask `
    -TaskName $TaskName `
    -Action $action `
    -Trigger $trigger `
    -Principal $principal `
    -Settings $settings `
    -Force | Out-Null

if ($AddFirewallRule) {
    try {
        $ruleName = "PocketAudio WebSocket"
        $existing = Get-NetFirewallRule -DisplayName $ruleName -ErrorAction SilentlyContinue
        if ($existing) {
            Remove-NetFirewallRule -DisplayName $ruleName -ErrorAction SilentlyContinue
        }
        New-NetFirewallRule `
            -DisplayName $ruleName `
            -Direction Inbound `
            -Action Allow `
            -Protocol TCP `
            -LocalPort 9000 `
            -Program $ExePath `
            -Profile Private, Domain -ErrorAction Stop | Out-Null
        Write-Host "Firewall: allowed TCP 9000 for $ExePath (Private/Domain)"
    } catch {
        Write-Warning "Firewall rule failed (run PowerShell as Administrator): $_"
    }
}

Write-Host ""
Write-Host "PocketAudio logon task installed"
Write-Host "  Task:     $TaskName"
Write-Host "  User:     $env:USERNAME (interactive — speaker capture works)"
Write-Host "  Server:   $ExePath"
Write-Host "  Log:      $env:LOCALAPPDATA\PocketAudio\server.log"
Write-Host ""
Write-Host "Commands:"
Write-Host "  Start now:  Start-ScheduledTask -TaskName $TaskName"
Write-Host "  Status:     .\status-logon-task.ps1"
Write-Host "  Remove:     .\uninstall-logon-task.ps1"
Write-Host ""

if ($StartNow) {
    Write-Host "Starting task..."
    Start-ScheduledTask -TaskName $TaskName
    Start-Sleep -Seconds 2
    & (Join-Path $ScriptsDir "status-logon-task.ps1")
}
