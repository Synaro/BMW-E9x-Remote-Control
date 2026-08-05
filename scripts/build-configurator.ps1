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
    (Join-Path $projectRoot 'src/application/feature_catalog.cpp') `
    (Join-Path $projectRoot 'src/application/user_settings.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/settings_identity.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/settings_payload.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/settings_protocol.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/settings_stream.cpp') `
    (Join-Path $projectRoot 'tools/settings_device_client.cpp') `
    (Join-Path $projectRoot 'tools/serial_settings_channel.cpp') `
    (Join-Path $projectRoot 'tools/user_settings_file.cpp') `
    (Join-Path $projectRoot 'tools/settings_configurator.cpp') `
    -o $configuratorExecutable

if ($LASTEXITCODE -ne 0) {
    throw "Configurator compilation failed with exit code $LASTEXITCODE"
}

Write-Host "Configurator built: $configuratorExecutable"
