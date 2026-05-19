# Shows PocketAudio scheduled task state and whether port 9000 is listening.

$TaskName = "PocketAudio"
$logFile = Join-Path $env:LOCALAPPDATA "PocketAudio\server.log"

Write-Host "=== PocketAudio logon task ==="
$task = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
if (-not $task) {
    Write-Host "Task '$TaskName': NOT INSTALLED"
    Write-Host "Install: .\install-logon-task.ps1 -StartNow"
    exit 1
}

$info = Get-ScheduledTaskInfo -TaskName $TaskName
Write-Host "Task:        $TaskName"
Write-Host "State:       $($task.State)"
Write-Host "Last run:    $($info.LastRunTime)"
Write-Host "Last result: $($info.LastTaskResult) (0 = success)"
Write-Host "Next run:    $($info.NextRunTime)"

$listen = Get-NetTCPConnection -LocalPort 9000 -State Listen -ErrorAction SilentlyContinue
if ($listen) {
    Write-Host "Port 9000:   LISTENING"
} else {
    Write-Host "Port 9000:   not listening (server not running?)"
}

if (Test-Path $logFile) {
    Write-Host ""
    Write-Host "Last log lines ($logFile):"
    Get-Content $logFile -Tail 8 -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "  $_" }
} else {
    Write-Host ""
    Write-Host "Log file not found yet: $logFile"
}

Write-Host ""
if (-not $listen -and $task.State -eq "Ready") {
    Write-Host "Tip: Start-ScheduledTask -TaskName $TaskName"
}
