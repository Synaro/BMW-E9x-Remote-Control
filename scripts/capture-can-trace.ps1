param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('pcan', 'slcan')]
    [string]$Interface,

    [Parameter(Mandatory = $true)]
    [string]$Channel,

    [Parameter(Mandatory = $true)]
    [ValidateRange(1, 10000000)]
    [int]$Bitrate,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [ValidateRange(0.1, 3600.0)]
    [double]$Duration = 10.0,

    [ValidateRange(1, 1000000)]
    [int]$MaximumFrames = 250000,

    [switch]$Overwrite
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$captureTool = Join-Path $projectRoot 'tools/capture_can_trace.py'
$arguments = @(
    $captureTool,
    '--interface', $Interface,
    '--channel', $Channel,
    '--bitrate', $Bitrate,
    '--duration', $Duration,
    '--max-frames', $MaximumFrames,
    '--output', $OutputPath
)

if ($Overwrite) {
    $arguments += '--overwrite'
}

& python @arguments

if ($LASTEXITCODE -ne 0) {
    throw "Passive CAN capture failed with exit code $LASTEXITCODE"
}
