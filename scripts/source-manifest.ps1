function Get-BmwRemoteSources {
    param(
        [Parameter(Mandatory = $true)]
        [string[]] $Groups
    )

    $projectRoot = Split-Path -Parent $PSScriptRoot
    $manifestPath = Join-Path $projectRoot 'cmake/source-manifest.txt'

    foreach ($line in Get-Content -LiteralPath $manifestPath) {
        $entry = $line.Trim()
        if ($entry.Length -eq 0 -or $entry.StartsWith('#')) {
            continue
        }

        $parts = $entry.Split('|', 2)
        if ($parts.Length -ne 2) {
            throw "Invalid source manifest entry: $line"
        }
        if ($Groups -contains $parts[0]) {
            Join-Path $projectRoot $parts[1]
        }
    }
}
