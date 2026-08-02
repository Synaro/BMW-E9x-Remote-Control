param(
    [string]$TracePath,

    [string]$ConfigPath,

    [ValidateSet(
        'nominal',
        'hood-required',
        'hood-optional',
        'takeover-timeout',
        'takeover-confirmed',
        'user-config',
        'settings-recovery',
        'settings-link')]
    [string]$Scenario = 'hood-required',

    [ValidateSet('required', 'optional')]
    [string]$Hood = 'required'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$simulatorExecutable = Join-Path $projectRoot 'build/bmw_remote_simulator.exe'
& (Join-Path $PSScriptRoot 'build-simulator.ps1')

if (-not [string]::IsNullOrWhiteSpace($TracePath) -and
    -not [string]::IsNullOrWhiteSpace($ConfigPath)) {
    throw 'TracePath and ConfigPath cannot be used together'
}

if (-not [string]::IsNullOrWhiteSpace($ConfigPath)) {
    & $simulatorExecutable --scenario user-config --config $ConfigPath
} elseif (-not [string]::IsNullOrWhiteSpace($TracePath)) {
    & $simulatorExecutable --trace $TracePath --hood $Hood
} elseif ($Scenario -eq 'user-config') {
    throw 'Scenario user-config requires ConfigPath'
} else {
    & $simulatorExecutable --scenario $Scenario
}

if ($LASTEXITCODE -ne 0) {
    throw "Simulator failed with exit code $LASTEXITCODE"
}
