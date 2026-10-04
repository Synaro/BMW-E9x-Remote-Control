[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Serial,
    [string]$Adb = "adb",
    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repoRoot ("erisin\captures-private\session-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$stdout = Join-Path $OutputRoot "logcat-epoch.txt"
$stderr = Join-Path $OutputRoot "logcat-stderr.txt"
$timeline = Join-Path $OutputRoot "operator-actions.ndjson"

Write-Host "Starting read-only logcat capture. The existing device log buffer is NOT cleared."
$process = Start-Process -FilePath $Adb -ArgumentList @("-s", $Serial, "logcat", "-v", "epoch") `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru -WindowStyle Hidden

$actions = @(
    "vehicle_rest", "ignition_on", "engine_start", "rpm_variation", "brake_press",
    "turn_left", "turn_right", "position_lights", "low_beam", "high_beam",
    "reverse", "driver_door", "other_door", "idrive_input", "engine_stop", "ignition_off"
)

try {
    foreach ($action in $actions) {
        Read-Host "Perform '$action', wait until stable, then press Enter"
        $record = [ordered]@{
            timestamp_utc = (Get-Date).ToUniversalTime().ToString("o")
            timestamp_unix_ms = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
            action = $action
            source = "MANUAL_OPERATOR_MARKER"
            interpretation = "OBSERVED_ACTION_NOT_PROTOCOL_PROOF"
        } | ConvertTo-Json -Compress
        Add-Content -LiteralPath $timeline -Value $record -Encoding utf8NoBOM
    }
}
finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id }
}

& $Adb -s $Serial shell "dumpsys" | Set-Content -LiteralPath (Join-Path $OutputRoot "dumpsys-after.txt") -Encoding utf8NoBOM
Write-Host "Capture saved to $OutputRoot (Git-ignored)."
Write-Host "The operator timestamps bound events; they do not prove the MCU/CAN origin by themselves."
