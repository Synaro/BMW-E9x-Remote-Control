[CmdletBinding()]
param(
    [string]$DeviceAddress = "",
    [string]$Adb = "adb",
    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputRoot = Join-Path $repoRoot "erisin\private-dumps\profile-$stamp"
}
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null

function Invoke-AdbRaw {
    param([string[]]$Arguments)
    $output = & $Adb @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "adb failed ($LASTEXITCODE): $($Arguments -join ' ')`n$output"
    }
    return ($output -join "`n")
}

if (-not [string]::IsNullOrWhiteSpace($DeviceAddress)) {
    Write-Host "Connecting ADB host to $DeviceAddress (no device files are changed)..."
    & $Adb connect $DeviceAddress | Out-Host
}

$devices = Invoke-AdbRaw -Arguments @("devices")
$serials = @($devices -split "`n" | Where-Object { $_ -match "^([^\s]+)\s+device$" } | ForEach-Object { $Matches[1] })
if ($serials.Count -ne 1) {
    throw "Expected exactly one authorized ADB device, found $($serials.Count). Use -DeviceAddress or disconnect other devices."
}
$serial = $serials[0]

function Save-ReadOnlyCommand {
    param([string]$Name, [string]$Command)
    Write-Host "[READ] $Name"
    $text = Invoke-AdbRaw -Arguments @("-s", $serial, "shell", $Command)
    $path = Join-Path $OutputRoot ($Name + ".txt")
    [System.IO.File]::WriteAllText($path, $text + "`n", [System.Text.UTF8Encoding]::new($false))
}

$commands = [ordered]@{
    "getprop"             = "getprop"
    "uname"               = "uname -a"
    "id"                  = "id"
    "root-id"             = "su -c id"
    "selinux-getenforce"  = "getenforce"
    "selinux-status"      = "cat /sys/fs/selinux/enforce 2>/dev/null"
    "mount"               = "mount"
    "df"                  = "df -h"
    "cpuinfo"             = "cat /proc/cpuinfo"
    "proc-version"        = "cat /proc/version"
    "proc-modules"        = "cat /proc/modules"
    "proc-devices"        = "cat /proc/devices"
    "tty-drivers"         = "cat /proc/tty/drivers"
    "dev"                 = "ls -la /dev"
    "dev-tty"             = "ls -la /dev/tty* 2>/dev/null"
    "dev-can-serial-spi"  = "find /dev -maxdepth 2 -type c 2>/dev/null | grep -Ei 'can|mcu|serial|tty|spi|uart|vehicle|car'"
    "sys-class"           = "find /sys/class -maxdepth 3 2>/dev/null"
    "sys-devices-matches" = "find /sys/devices -maxdepth 6 2>/dev/null | grep -Ei 'can|mcu|serial|tty|spi|uart|vehicle|car'"
    "proc-net-can"        = "cat /proc/net/can/stats 2>/dev/null; cat /proc/net/dev 2>/dev/null"
    "ip-link"             = "ip -details link show 2>/dev/null"
    "ps"                  = "ps -A"
    "services"            = "service list"
    "dumpsys"             = "dumpsys"
    "packages-all"        = "pm list packages -f"
    "packages-system"     = "pm list packages -s -f"
    "packages-thirdparty" = "pm list packages -3 -f"
    "settings-global"     = "settings list global"
    "settings-system"     = "settings list system"
    "settings-secure"     = "settings list secure"
    "properties-relevant" = "getprop | grep -Ei 'xrc|mcu|can|canbus|car|vehicle|decoder|serial|uart|spi|bmw|build|fingerprint'"
    "packages-relevant"   = "pm list packages -f | grep -Ei 'xrc|mcu|can|canbus|car|vehicle|event|bmw|decoder|dashboard|cluster'"
    "services-relevant"   = "service list | grep -Ei 'xrc|mcu|can|canbus|car|vehicle|event|bmw|decoder|dashboard|cluster'"
    "processes-relevant"  = "ps -A | grep -Ei 'xrc|mcu|can|canbus|car|vehicle|event|bmw|decoder|dashboard|cluster'"
}

foreach ($entry in $commands.GetEnumerator()) {
    try { Save-ReadOnlyCommand -Name $entry.Key -Command $entry.Value }
    catch {
        $errorPath = Join-Path $OutputRoot ($entry.Key + ".error.txt")
        [System.IO.File]::WriteAllText($errorPath, $_.Exception.Message, [System.Text.UTF8Encoding]::new($false))
        Write-Warning "$($entry.Key): $($_.Exception.Message)"
    }
}

$metadata = [ordered]@{
    schema_version = 1
    collected_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    adb_serial = $serial
    collection_mode = "READ_ONLY"
    commands_attempted = @($commands.Keys)
    writes_to_device = $false
}
$metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputRoot "manifest.json") -Encoding utf8NoBOM

$analyzer = Join-Path $PSScriptRoot "analyze_erisin_profile.py"
if (Get-Command python -ErrorAction SilentlyContinue) {
    & python $analyzer $OutputRoot
    if ($LASTEXITCODE -ne 0) { Write-Warning "Profile analyzer exited with $LASTEXITCODE" }
}

Write-Host "Read-only profile saved to: $OutputRoot"
Write-Host "Review and redact before sharing. This directory is Git-ignored."
