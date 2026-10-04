[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Serial,
    [string]$Adb = "adb",
    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repoRoot ("erisin\vendor-apks\" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null

$packageLines = & $Adb -s $Serial shell "pm list packages -f" 2>&1
if ($LASTEXITCODE -ne 0) { throw "Unable to list packages: $packageLines" }
$pattern = "xrc|mcu|can|canbus|car|vehicle|event|bmw|decoder|dashboard|cluster"
$inventory = @()
foreach ($line in $packageLines) {
    if ($line -notmatch '^package:(.+)=(.+)$') { continue }
    $remotePath = $Matches[1]
    $packageName = $Matches[2]
    if (("$remotePath $packageName") -notmatch $pattern) { continue }
    $safeName = ($packageName -replace '[^A-Za-z0-9._-]', '_') + ".apk"
    $localPath = Join-Path $OutputRoot $safeName
    Write-Host "[READ/PULL] $packageName"
    & $Adb -s $Serial pull $remotePath $localPath | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "adb pull failed for $remotePath" }
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $localPath).Hash.ToLowerInvariant()
    $inventory += [ordered]@{ package = $packageName; device_path = $remotePath; file = $safeName; sha256 = $hash }
}

$inventory | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputRoot "inventory.json") -Encoding utf8NoBOM
Write-Host "Vendor files were copied read-only to a Git-ignored directory: $OutputRoot"
Write-Host "Do not commit or redistribute them."
