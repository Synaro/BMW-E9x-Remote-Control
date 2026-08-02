param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$')]
    [string]$SessionName
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$privateRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $projectRoot 'captures/private'))
$sessionPath = [System.IO.Path]::GetFullPath(
    (Join-Path $privateRoot $SessionName))
$privatePrefix = $privateRoot.TrimEnd(
    [System.IO.Path]::DirectorySeparatorChar) +
    [System.IO.Path]::DirectorySeparatorChar

if (-not $sessionPath.StartsWith(
    $privatePrefix,
    [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Session path must stay inside captures/private'
}

if (Test-Path -LiteralPath $sessionPath) {
    throw "Diagnostic session already exists: $sessionPath"
}

$templatePath = Join-Path `
    $projectRoot `
    'docs/templates/diagnostic-inventory-template.md'
$inventoryPath = Join-Path $sessionPath 'diagnostic-inventory.md'
$screenshotsPath = Join-Path $sessionPath 'screenshots'

New-Item -ItemType Directory -Path $screenshotsPath | Out-Null
$template = Get-Content -Raw -LiteralPath $templatePath
$inventory = $template.Replace('{{SESSION_NAME}}', $SessionName)
Set-Content -LiteralPath $inventoryPath -Value $inventory -Encoding utf8NoBOM

Write-Host "Private diagnostic session created: $sessionPath"
Write-Host "Inventory: $inventoryPath"
