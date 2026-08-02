param(
    [string]$ConfigPath,

    [switch]$Show,

    [switch]$Check,

    [switch]$WriteDefaults
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$configuratorExecutable = Join-Path $projectRoot 'build/bmw_remote_configurator.exe'

if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
    $ConfigPath = Join-Path $projectRoot 'config/user-settings.conf'
}

$actions = @($Show, $Check, $WriteDefaults) | Where-Object { $_ }
if ($actions.Count -gt 1) {
    throw 'Show, Check and WriteDefaults are mutually exclusive'
}

& (Join-Path $PSScriptRoot 'build-configurator.ps1')

$arguments = @('--config', $ConfigPath)
if ($Show) {
    $arguments += '--show'
} elseif ($Check) {
    $arguments += '--check'
} elseif ($WriteDefaults) {
    $arguments += '--write-defaults'
}

& $configuratorExecutable @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Configurator failed with exit code $LASTEXITCODE"
}
