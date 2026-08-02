param(
    [string]$ConfigPath,

    [switch]$Show,

    [switch]$Check,

    [switch]$WriteDefaults,

    [switch]$ListDevices,

    [string]$ReadDevice,

    [string]$WriteDevice
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$configuratorExecutable = Join-Path $projectRoot 'build/bmw_remote_configurator.exe'

if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
    $ConfigPath = Join-Path $projectRoot 'config/user-settings.conf'
}

$actions = @(
    [bool]$Show,
    [bool]$Check,
    [bool]$WriteDefaults,
    [bool]$ListDevices,
    -not [string]::IsNullOrWhiteSpace($ReadDevice),
    -not [string]::IsNullOrWhiteSpace($WriteDevice)
) | Where-Object { $_ }
if ($actions.Count -gt 1) {
    throw 'Show, Check, WriteDefaults, ListDevices, ReadDevice and WriteDevice are mutually exclusive'
}

& (Join-Path $PSScriptRoot 'build-configurator.ps1')

$arguments = @()
if ($ListDevices) {
    $arguments += '--list-devices'
} elseif (-not [string]::IsNullOrWhiteSpace($ReadDevice)) {
    $arguments += @('--read-device', $ReadDevice)
} elseif (-not [string]::IsNullOrWhiteSpace($WriteDevice)) {
    $arguments += @('--write-device', $WriteDevice, '--config', $ConfigPath)
} elseif ($Show) {
    $arguments += @('--show', '--config', $ConfigPath)
} elseif ($Check) {
    $arguments += @('--check', '--config', $ConfigPath)
} elseif ($WriteDefaults) {
    $arguments += @('--write-defaults', '--config', $ConfigPath)
} else {
    $arguments += @('--config', $ConfigPath)
}

& $configuratorExecutable @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Configurator failed with exit code $LASTEXITCODE"
}
