$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$configuratorExecutable = Join-Path $buildDirectory 'bmw_remote_configurator.exe'

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
    (Join-Path $projectRoot 'src/application/user_settings.cpp') `
    (Join-Path $projectRoot 'tools/user_settings_file.cpp') `
    (Join-Path $projectRoot 'tools/settings_configurator.cpp') `
    -o $configuratorExecutable

if ($LASTEXITCODE -ne 0) {
    throw "Configurator compilation failed with exit code $LASTEXITCODE"
}

Write-Host "Configurator built: $configuratorExecutable"
