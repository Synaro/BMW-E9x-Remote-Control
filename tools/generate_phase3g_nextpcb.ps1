[CmdletBinding()]
param(
    [string]$KiCadCli = 'C:\Users\synar\AppData\Local\Programs\KiCad\10.0\bin\kicad-cli.exe'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$board = Join-Path $repoRoot 'hardware\kcan-rxonly-pcb\kicad\BMW-E9x-KCAN-RXOnly.kicad_pcb'
$nextPcbRoot = Join-Path $repoRoot 'hardware\kcan-rxonly-pcb\manufacturing\nextpcb'
$positionFile = Join-Path $nextPcbRoot 'BMW-E9x-KCAN-RXOnly-PCBA-Top.pos'

if (-not (Test-Path -LiteralPath $KiCadCli -PathType Leaf)) {
    throw "KiCad CLI not found: $KiCadCli"
}

New-Item -ItemType Directory -Path $nextPcbRoot -Force | Out-Null

& $KiCadCli pcb export pos --format ascii --units mm --side front `
    --exclude-dnp --output $positionFile $board
if ($LASTEXITCODE -ne 0) {
    throw "KiCad NextPCB position export failed ($LASTEXITCODE)."
}

python (Join-Path $PSScriptRoot 'generate_phase3g_nextpcb.py')
if ($LASTEXITCODE -ne 0) {
    throw 'NextPCB BOM/centroid validation and packaging failed.'
}

Write-Host "NextPCB quote exports generated: $nextPcbRoot"
