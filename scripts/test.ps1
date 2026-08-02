$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$testExecutable = Join-Path $buildDirectory 'bmw_remote_tests.exe'

New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null

& g++.exe `
    -std=c++17 `
    -Wall `
    -Wextra `
    -Wpedantic `
    -Wconversion `
    -Werror `
    -I (Join-Path $projectRoot 'include') `
    (Join-Path $projectRoot 'src/application/controller.cpp') `
    (Join-Path $projectRoot 'src/application/safety_policy.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/runtime.cpp') `
    (Join-Path $projectRoot 'tests/test_main.cpp') `
    -o $testExecutable

if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed with exit code $LASTEXITCODE"
}

& $testExecutable

if ($LASTEXITCODE -ne 0) {
    throw "Tests failed with exit code $LASTEXITCODE"
}
