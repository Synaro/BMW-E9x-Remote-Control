$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'source-manifest.ps1')

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$testExecutable = Join-Path $buildDirectory 'bmw_remote_tests.exe'
$sources = @(Get-BmwRemoteSources -Groups @(
    'core',
    'host',
    'simulation',
    'sandbox',
    'tests'
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
    -I (Join-Path $projectRoot 'libs/can-core/include') `
    -I $projectRoot `
    $sources `
    -o $testExecutable

if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed with exit code $LASTEXITCODE"
}

& $testExecutable

if ($LASTEXITCODE -ne 0) {
    throw "Tests failed with exit code $LASTEXITCODE"
}
