[CmdletBinding()]
param(
    [string]$KiCadCli = 'C:\Users\synar\AppData\Local\Programs\KiCad\10.0\bin\kicad-cli.exe'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$hardwareRoot = Join-Path $repoRoot 'hardware\kcan-bidirectional-pcb'
$kicadRoot = Join-Path $hardwareRoot 'kicad'
$outputRoot = Join-Path $hardwareRoot 'manufacturing'
$name = 'BMW-E9x-KCAN-Bidirectional'
$board = Join-Path $kicadRoot "$name.kicad_pcb"
$schematic = Join-Path $kicadRoot "$name.kicad_sch"

if (-not (Test-Path -LiteralPath $KiCadCli -PathType Leaf)) {
    throw "KiCad CLI not found: $KiCadCli"
}

function Invoke-Checked {
    param([Parameter(Mandatory)][string[]]$Arguments)
    & $KiCadCli @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "KiCad CLI failed ($LASTEXITCODE): $($Arguments -join ' ')"
    }
}

$leafDirectories = @(
    'gerbers', 'drill', 'assembly', 'bom', 'cpl', 'drawings', '3d',
    'images', 'reports', 'review'
)
foreach ($leaf in $leafDirectories) {
    $path = Join-Path $outputRoot $leaf
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
    New-Item -ItemType Directory -Path $path | Out-Null
}

Invoke-Checked @(
    'pcb', 'drc', '--exit-code-violations', '--severity-all',
    '-o', (Join-Path $kicadRoot 'phase3h-drc-final.rpt'), $board
)
Invoke-Checked @(
    'sch', 'erc', '--exit-code-violations', '--severity-all',
    '-o', (Join-Path $kicadRoot 'phase3h-erc.rpt'), $schematic
)

Invoke-Checked @(
    'pcb', 'export', 'gerbers', '--check-zones', '--subtract-soldermask',
    '--precision', '6', '--output', (Join-Path $outputRoot 'gerbers'),
    '--layers', 'F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts',
    $board
)
Invoke-Checked @(
    'pcb', 'export', 'drill', '--format', 'excellon', '--excellon-units', 'mm',
    '--excellon-separate-th', '--generate-map', '--map-format', 'pdf',
    '--generate-report', '--report-path', (Join-Path $outputRoot 'reports\drill-report.txt'),
    '--output', (Join-Path $outputRoot 'drill'), $board
)
Invoke-Checked @(
    'pcb', 'export', 'pos', '--format', 'csv', '--units', 'mm', '--side', 'both',
    '--exclude-dnp', '--output',
    (Join-Path $outputRoot "assembly\$name-all-pos.csv"), $board
)
Invoke-Checked @(
    'sch', 'export', 'pdf', '--black-and-white', '--output',
    (Join-Path $outputRoot "drawings\$name-schematic.pdf"), $schematic
)
Invoke-Checked @(
    'pcb', 'export', 'pdf', '--mode-single', '--black-and-white',
    '--sketch-pads-on-fab-layers', '--include-border-title', '--layers',
    'F.Fab,F.Silkscreen,Edge.Cuts', '--output',
    (Join-Path $outputRoot "drawings\$name-assembly.pdf"), $board
)
Invoke-Checked @(
    'pcb', 'export', 'pdf', '--mode-multipage', '--black-and-white',
    '--include-border-title', '--layers',
    'F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,Edge.Cuts', '--output',
    (Join-Path $outputRoot "drawings\$name-fabrication.pdf"), $board
)
Invoke-Checked @(
    'pcb', 'export', 'step', '--force', '--subst-models', '--no-dnp', '--output',
    (Join-Path $outputRoot "3d\$name.step"), $board
)
$stepPath = Join-Path $outputRoot "3d\$name.step"
$stepLines = [System.IO.File]::ReadAllLines($stepPath) |
    ForEach-Object { $_.TrimEnd() }
[System.IO.File]::WriteAllText(
    $stepPath,
    (($stepLines -join "`n") + "`n"),
    [System.Text.UTF8Encoding]::new($false)
)
Invoke-Checked @(
    'pcb', 'render', '--side', 'top', '--quality', 'high', '--floor',
    '--width', '2400', '--height', '1600', '--background', 'opaque', '--output',
    (Join-Path $outputRoot "images\$name-front.png"), $board
)
Invoke-Checked @(
    'pcb', 'render', '--side', 'bottom', '--quality', 'high', '--floor',
    '--width', '2400', '--height', '1600', '--background', 'opaque', '--output',
    (Join-Path $outputRoot "images\$name-back.png"), $board
)
Invoke-Checked @(
    'pcb', 'export', 'stats', '--format', 'report', '--units', 'mm', '--output',
    (Join-Path $outputRoot 'reports\board-statistics.txt'), $board
)
Invoke-Checked @(
    'pcb', 'export', 'ipcd356', '--output',
    (Join-Path $outputRoot "reports\$name.ipc"), $board
)

python (Join-Path $PSScriptRoot 'generate_phase3h_manufacturing.py')
if ($LASTEXITCODE -ne 0) {
    throw 'Phase 3H BOM/CPL generation failed.'
}

& (Join-Path $PSScriptRoot 'generate_phase3h_nextpcb.ps1') -KiCadCli $KiCadCli
if ($LASTEXITCODE -ne 0) {
    throw 'Phase 3H NextPCB export generation failed.'
}

$gerberReviews = @(
    @{ File = "$name-F_Cu.gtl"; Style = 'copper'; Output = 'gerber-front-copper.png' },
    @{ File = "$name-B_Cu.gbl"; Style = 'copper'; Output = 'gerber-back-copper.png' },
    @{ File = "$name-F_Silkscreen.gto"; Style = 'silk'; Output = 'gerber-front-silkscreen.png' },
    @{ File = "$name-F_Mask.gts"; Style = 'solder_mask'; Output = 'gerber-front-mask.png' },
    @{ File = "$name-Edge_Cuts.gm1"; Style = 'default_grayscale'; Output = 'gerber-outline.png' }
)
foreach ($review in $gerberReviews) {
    $source = Join-Path $outputRoot "gerbers\$($review.File)"
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Expected Gerber missing: $source"
    }
    python -m pygerber raster-2d --dpi 1000 --style $review.Style --output `
        (Join-Path $outputRoot "review\$($review.Output)") $source
    if ($LASTEXITCODE -ne 0) {
        throw "PyGerber failed for $source"
    }
}

python (Join-Path $PSScriptRoot 'generate_phase3h_manufacturing.py') --finalize
if ($LASTEXITCODE -ne 0) {
    throw 'Phase 3H package finalization failed.'
}

Write-Host "Phase 3H manufacturing review package generated: $outputRoot"
