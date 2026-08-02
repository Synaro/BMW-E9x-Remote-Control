param(
    [Parameter(Mandatory = $true)]
    [string]$BaselineTrace,

    [Parameter(Mandatory = $true)]
    [string]$EventTrace,

    [ValidateRange(0.0, 1.0)]
    [double]$MinimumDistance = 0.25,

    [ValidateRange(1, 1000)]
    [int]$Limit = 20
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$analyzer = Join-Path $projectRoot 'tools/analyze_trace_change.py'

& python $analyzer `
    $BaselineTrace `
    $EventTrace `
    --min-distance $MinimumDistance `
    --limit $Limit

if ($LASTEXITCODE -ne 0) {
    throw "Trace analysis failed with exit code $LASTEXITCODE"
}
