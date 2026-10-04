[CmdletBinding()]
param(
    [string]$KiCadCli = 'C:\Users\synar\AppData\Local\Programs\KiCad\10.0\bin\kicad-cli.exe'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$board = Join-Path $repoRoot 'hardware\kcan-bidirectional-pcb\kicad\BMW-E9x-KCAN-Bidirectional.kicad_pcb'
$nextPcbRoot = Join-Path $repoRoot 'hardware\kcan-bidirectional-pcb\manufacturing\nextpcb'
$positionFile = Join-Path $nextPcbRoot 'BMW-E9x-KCAN-Bidirectional-PCBA-Top.pos'

if (-not (Test-Path -LiteralPath $KiCadCli -PathType Leaf)) {
    throw "KiCad CLI not found: $KiCadCli"
}

New-Item -ItemType Directory -Path $nextPcbRoot -Force | Out-Null

& $KiCadCli pcb export pos --format ascii --units mm --side front `
    --exclude-dnp --output $positionFile $board
if ($LASTEXITCODE -ne 0) {
    throw "KiCad NextPCB position export failed ($LASTEXITCODE)."
}

# Store deterministic LF bytes so the standalone file, ZIP member and Git
# checkout remain identical on Windows and Linux.
$positionLines = [System.IO.File]::ReadAllLines($positionFile)
[System.IO.File]::WriteAllText(
    $positionFile,
    (($positionLines -join "`n") + "`n"),
    [System.Text.UTF8Encoding]::new($false)
)

python (Join-Path $PSScriptRoot 'generate_phase3h_nextpcb.py')
if ($LASTEXITCODE -ne 0) {
    throw 'NextPCB BOM/centroid validation and packaging failed.'
}

# Keep the package-wide SHA-256 manifest correct when this exporter is invoked
# independently of the full manufacturing script.
python (Join-Path $PSScriptRoot 'generate_phase3h_manufacturing.py') --finalize
if ($LASTEXITCODE -ne 0) {
    throw 'Phase 3H manifest refresh failed.'
}

Write-Host "NextPCB quote exports generated: $nextPcbRoot"
