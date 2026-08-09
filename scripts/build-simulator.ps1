$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'source-manifest.ps1')

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$simulatorExecutable = Join-Path $buildDirectory 'bmw_remote_simulator.exe'
$sources = @(Get-BmwRemoteSources -Groups @(
    'core',
    'host',
    'simulation',
    'sandbox',
    'simulator'
))

New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null

& g++.exe `
    -std=c++17 `
    -Wall `
    -Wextra `
    -Wpedantic `
    -Wconversion `
    -Werror `
    -I (Join-Path $projectRoot 'include') `
    -I $projectRoot `
    $sources `
    -o $simulatorExecutable

if ($LASTEXITCODE -ne 0) {
    throw "Simulator compilation failed with exit code $LASTEXITCODE"
}

Write-Host "Simulator built: $simulatorExecutable"
