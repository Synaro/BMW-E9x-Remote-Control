param(
    [string]$TracePath,

    [ValidateSet('nominal', 'hood-required', 'hood-optional')]
    [string]$Scenario = 'hood-required',

    [ValidateSet('required', 'optional')]
    [string]$Hood = 'required'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$simulatorExecutable = Join-Path $projectRoot 'build/bmw_remote_simulator.exe'
& (Join-Path $PSScriptRoot 'build-simulator.ps1')

if ([string]::IsNullOrWhiteSpace($TracePath)) {
    & $simulatorExecutable --scenario $Scenario
} else {
    & $simulatorExecutable --trace $TracePath --hood $Hood
}

if ($LASTEXITCODE -ne 0) {
    throw "Simulator failed with exit code $LASTEXITCODE"
}
