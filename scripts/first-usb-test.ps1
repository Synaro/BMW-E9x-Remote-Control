param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^COM([1-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-6])$')]
    [string]$Port
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$benchConfiguration = Join-Path $projectRoot 'config/bench-safe.example.conf'

& (Join-Path $PSScriptRoot 'configure.ps1') `
    -Check `
    -ConfigPath $benchConfiguration

$runtimePort = & (Join-Path $PSScriptRoot 'flash-prototype.ps1') `
    -Port $Port `
    -PassThru

& (Join-Path $PSScriptRoot 'configure.ps1') `
    -WriteDevice $runtimePort `
    -ConfigPath $benchConfiguration

& (Join-Path $PSScriptRoot 'configure.ps1') -ReadDevice $runtimePort

Write-Host 'first_usb_test_result: PASS'
