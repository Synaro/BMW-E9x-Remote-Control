param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^COM([1-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-6])$')]
    [string]$Port,

    [switch]$PassThru
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$configuratorExecutable = Join-Path $projectRoot 'build/bmw_remote_configurator.exe'

function Invoke-CheckedNative {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Executable,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [string]$FailureMessage
    )

    $nativeOutput = @(& $Executable @Arguments 2>&1)
    $nativeExitCode = $LASTEXITCODE
    $nativeOutput | ForEach-Object { Write-Host $_ }
    if ($nativeExitCode -ne 0) {
        throw "$FailureMessage (code $nativeExitCode)"
    }
}

$availablePorts = @([System.IO.Ports.SerialPort]::GetPortNames())
if ($Port -notin $availablePorts) {
    $displayedPorts = if ($availablePorts.Count -eq 0) {
        'aucun'
    } else {
        $availablePorts -join ', '
    }
    throw "Le port $Port n'est pas present. Ports detectes : $displayedPorts"
}

Push-Location $projectRoot
try {
    Invoke-CheckedNative `
        -Executable 'pio' `
        -Arguments @('run', '-e', 'esp32s3dev') `
        -FailureMessage 'La compilation du firmware a echoue'

    & (Join-Path $PSScriptRoot 'build-configurator.ps1')

    Invoke-CheckedNative `
        -Executable 'pio' `
        -Arguments @(
            'run',
            '-e', 'esp32s3dev',
            '--target', 'upload',
            '--upload-port', $Port
        ) `
        -FailureMessage "Le flashage sur $Port a echoue"

    $runtimePort = $null
    $portDeadline = [DateTime]::UtcNow.AddSeconds(20)
    while ($null -eq $runtimePort) {
        $currentPorts = @([System.IO.Ports.SerialPort]::GetPortNames())
        if ($Port -in $currentPorts) {
            $runtimePort = $Port
            break
        }

        $newPorts = @($currentPorts | Where-Object { $_ -notin $availablePorts })
        if ($newPorts.Count -eq 1) {
            $runtimePort = $newPorts[0]
            Write-Host "Le firmware est reapparu sur $runtimePort."
            break
        }
        if ($newPorts.Count -gt 1) {
            throw "Plusieurs nouveaux ports sont apparus : $($newPorts -join ', ')"
        }
        if ([DateTime]::UtcNow -ge $portDeadline) {
            throw "Aucun port serie attribuable au prototype n'est apparu apres le flashage"
        }
        Start-Sleep -Milliseconds 250
    }

    $lastProbeOutput = @()
    for ($attempt = 1; $attempt -le 5; $attempt++) {
        $lastProbeOutput = @(
            & $configuratorExecutable --probe-device $runtimePort 2>&1
        )
        if ($LASTEXITCODE -eq 0) {
            $lastProbeOutput | ForEach-Object { Write-Host $_ }
            Write-Host "Firmware flashe et identifie sur $runtimePort."
            if ($PassThru) {
                Write-Output $runtimePort
            }
            return
        }
        Start-Sleep -Milliseconds 500
    }

    $lastProbeOutput | ForEach-Object { Write-Host $_ }
    throw "Le firmware a ete flashe, mais l'identite du boitier n'a pas ete confirmee"
} finally {
    Pop-Location
}
