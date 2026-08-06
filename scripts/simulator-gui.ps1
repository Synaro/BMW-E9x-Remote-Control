param(
    [switch]$SkipBuild,
    [switch]$SelfTest,
    [string]$PreviewPath,
    [string]$SandboxPreviewPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$simulatorExecutable = Join-Path $projectRoot 'build/bmw_remote_simulator.exe'
$buildScript = Join-Path $PSScriptRoot 'build-simulator.ps1'

$scenarioDefinitions = @(
    [pscustomobject]@{
        Label = 'Démarrage et arrêt nominaux'
        Code = 'nominal'
        Description = 'Parcourt un démarrage distant complet, confirme le moteur puis exécute un arrêt demandé.'
    },
    [pscustomobject]@{
        Label = 'Capot obligatoire'
        Code = 'hood-required'
        Description = 'Ouvre le capot pendant la session et vérifie le passage immédiat en défaut lorsque le contrôle est requis.'
    },
    [pscustomobject]@{
        Label = 'Capot facultatif'
        Code = 'hood-optional'
        Description = "Vérifie qu'une installation configurée sans capteur ignore entièrement le signal de capot."
    },
    [pscustomobject]@{
        Label = 'Reprise conducteur expirée'
        Code = 'takeover-timeout'
        Description = "Ouvre une portière sans confirmer la reprise conducteur et vérifie l'arrêt à l'échéance."
    },
    [pscustomobject]@{
        Label = 'Reprise conducteur confirmée'
        Code = 'takeover-confirmed'
        Description = 'Simule une reprise authentifiée et le transfert de contrôle au conducteur.'
    },
    [pscustomobject]@{
        Label = 'Perte des signaux carrosserie'
        Code = 'signal-loss'
        Description = "Supprime les mises à jour de carrosserie jusqu'à leur péremption et vérifie la mise en sécurité."
    },
    [pscustomobject]@{
        Label = 'Retard des signaux de sécurité'
        Code = 'signal-delay'
        Description = 'Retarde les données nécessaires avant le lancement afin de confirmer que le démarreur reste interdit.'
    },
    [pscustomobject]@{
        Label = "Corruption d'une trame"
        Code = 'frame-corruption'
        Description = 'Corrompt une trame synthétique reconnue et vérifie que la communication est refusée.'
    },
    [pscustomobject]@{
        Label = 'Récupération des réglages'
        Code = 'settings-recovery'
        Description = 'Corrompt la génération la plus récente des réglages puis restaure automatiquement la précédente.'
    },
    [pscustomobject]@{
        Label = 'Liaison de configuration'
        Code = 'settings-link'
        Description = "Teste l'autorisation, l'état occupé, l'écriture, la relecture et le rejet d'un CRC corrompu."
    },
    [pscustomobject]@{
        Label = 'Protection anti-rejeu'
        Code = 'lock-replay-guard'
        Description = 'Refuse les preuves de verrouillage non fiables, anciennes, dupliquées ou désordonnées.'
    },
    [pscustomobject]@{
        Label = 'Adaptateur CAN fictif qualifié'
        Code = 'qualified-lock-adapter'
        Description = 'Valide les fronts et le compteur roulant sur un vecteur de test qui ne représente aucune trame BMW.'
    },
    [pscustomobject]@{
        Label = 'Superviseur des actionneurs'
        Code = 'actuator-supervisor'
        Description = 'Injecte une expiration du heartbeat et un retour électrique incohérent dans le pilote simulé.'
    },
    [pscustomobject]@{
        Label = 'Chaîne complète supervisée'
        Code = 'supervised-runtime'
        Description = "Relie contrôleur, runtime et superviseur, puis propage une panne jusqu'au défaut applicatif."
    },
    [pscustomobject]@{
        Label = "Configuration d'exemple"
        Code = 'user-config-example'
        Description = "Charge le fichier d'exemple fourni, applique ses réglages et exécute le parcours utilisateur complet."
    }
)

function Test-SimulatorNeedsBuild {
    if (-not (Test-Path -LiteralPath $simulatorExecutable -PathType Leaf)) {
        return $true
    }

    $executableTimestamp =
        (Get-Item -LiteralPath $simulatorExecutable).LastWriteTimeUtc
    $sourceRoots = @(
        (Join-Path $projectRoot 'include'),
        (Join-Path $projectRoot 'src'),
        (Join-Path $projectRoot 'tools'),
        $buildScript
    )
    foreach ($sourceRoot in $sourceRoots) {
        if (Test-Path -LiteralPath $sourceRoot -PathType Leaf) {
            if ((Get-Item -LiteralPath $sourceRoot).LastWriteTimeUtc -gt
                $executableTimestamp) {
                return $true
            }
            continue
        }
        $newerSource = Get-ChildItem -LiteralPath $sourceRoot -File -Recurse |
            Where-Object { $_.LastWriteTimeUtc -gt $executableTimestamp } |
            Select-Object -First 1
        if ($null -ne $newerSource) {
            return $true
        }
    }
    return $false
}

function Initialize-SimulatorExecutable {
    if (-not $SkipBuild -and (Test-SimulatorNeedsBuild)) {
        & $buildScript
        if ($LASTEXITCODE -ne 0) {
            throw "La compilation du simulateur a échoué ($LASTEXITCODE)."
        }
    }
    if (-not (Test-Path -LiteralPath $simulatorExecutable -PathType Leaf)) {
        throw 'Le simulateur est introuvable. Relancez sans -SkipBuild.'
    }
}

function ConvertTo-NativeArgument {
    param([Parameter(Mandatory)][string]$Value)

    if ($Value -notmatch '[\s"]') {
        return $Value
    }
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Invoke-SimulatorProcess {
    param([Parameter(Mandatory)][string[]]$Arguments)

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $simulatorExecutable
    $startInfo.Arguments = (($Arguments | ForEach-Object {
        ConvertTo-NativeArgument $_
    }) -join ' ')
    $startInfo.WorkingDirectory = $projectRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw 'Impossible de lancer le simulateur.'
    }
    $standardOutput = $process.StandardOutput.ReadToEnd()
    $standardError = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    $process.Dispose()

    return [pscustomobject]@{
        ExitCode = $exitCode
        StandardOutput = $standardOutput
        StandardError = $standardError
        Command = "$simulatorExecutable $($startInfo.Arguments)"
    }
}

function Start-SandboxProcess {
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $simulatorExecutable
    $startInfo.Arguments = '--sandbox'
    $startInfo.WorkingDirectory = $projectRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw 'Impossible de lancer le bac à sable.'
    }
    return $process
}

function Invoke-SandboxCommand {
    param(
        [Parameter(Mandatory)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory)][string]$Command
    )

    if ($Process.HasExited) {
        $details = $Process.StandardError.ReadToEnd()
        throw "Le bac à sable s'est arrêté prématurément. $details"
    }
    $Process.StandardInput.WriteLine($Command)
    $Process.StandardInput.Flush()
    $line = $Process.StandardOutput.ReadLine()
    if ($null -eq $line) {
        $details = $Process.StandardError.ReadToEnd()
        throw "Le bac à sable n'a pas répondu. $details"
    }
    try {
        return $line | ConvertFrom-Json
    } catch {
        throw "Réponse invalide du bac à sable : $line"
    }
}

function Stop-SandboxProcess {
    param([System.Diagnostics.Process]$Process)

    if ($null -eq $Process) {
        return
    }
    try {
        if (-not $Process.HasExited) {
            $Process.StandardInput.WriteLine('quit')
            $Process.StandardInput.Flush()
            [void]$Process.StandardOutput.ReadLine()
            if (-not $Process.WaitForExit(1000)) {
                $Process.Kill()
                [void]$Process.WaitForExit(1000)
            }
        }
    } finally {
        $Process.Dispose()
    }
}

try {
    Initialize-SimulatorExecutable

    if ($SelfTest) {
        $listedScenarios = @(
            & $simulatorExecutable --list-scenarios |
                ForEach-Object { ($_ -split '\s+', 2)[0] }
        )
        $definedScenarios = @(
            $scenarioDefinitions |
                Where-Object { $_.Code -ne 'user-config-example' } |
                ForEach-Object { $_.Code }
        )
        $knownCliScenarios = @($definedScenarios) + 'user-config'
        $missingScenarios = @(
            $definedScenarios | Where-Object { $_ -notin $listedScenarios }
        )
        $unknownScenarios = @(
            $listedScenarios | Where-Object { $_ -notin $knownCliScenarios }
        )
        if ($missingScenarios.Count -ne 0 -or $unknownScenarios.Count -ne 0) {
            throw "Scénarios GUI désynchronisés. Manquants=$missingScenarios Inconnus=$unknownScenarios"
        }
        $smokeTest = Invoke-SimulatorProcess @(
            '--scenario', 'supervised-runtime')
        if ($smokeTest.ExitCode -ne 0 -or
            $smokeTest.StandardOutput -notmatch 'scenario_result: PASS') {
            throw 'Le test de fumée de la chaîne supervisée a échoué.'
        }
        $sandboxProcess = Start-SandboxProcess
        try {
            $sandboxStatus = Invoke-SandboxCommand $sandboxProcess 'status'
            $sandboxPreparing = Invoke-SandboxCommand $sandboxProcess 'start'
            $sandboxCranking = Invoke-SandboxCommand $sandboxProcess 'timer'
            $sandboxRunning = Invoke-SandboxCommand $sandboxProcess 'vehicle rpm=850'
            $sandboxFault = Invoke-SandboxCommand $sandboxProcess 'watchdog'
            if (-not $sandboxStatus.ok -or $sandboxStatus.state -ne 'idle' -or
                $sandboxPreparing.state -ne 'preparing' -or
                $sandboxCranking.state -ne 'cranking' -or
                $sandboxRunning.state -ne 'running' -or
                $sandboxFault.state -ne 'fault' -or
                $sandboxFault.fault -ne 'actuator_failure' -or
                $sandboxFault.ignition_active -or
                $sandboxFault.starter_active) {
                $observed = @(
                    "status=$($sandboxStatus.state)/$($sandboxStatus.ok)/$($sandboxStatus.error)",
                    "start=$($sandboxPreparing.state)/$($sandboxPreparing.fault)",
                    "timer=$($sandboxCranking.state)/$($sandboxCranking.fault)",
                    "running=$($sandboxRunning.state)/$($sandboxRunning.fault)",
                    "watchdog=$($sandboxFault.state)/$($sandboxFault.fault)/$($sandboxFault.supervisor_fault)",
                    "outputs=$($sandboxFault.ignition_active)/$($sandboxFault.starter_active)"
                ) -join ', '
                throw "Le test du bac à sable persistant a échoué : $observed"
            }
        } finally {
            Stop-SandboxProcess $sandboxProcess
        }
        Write-Output 'gui_self_test: PASS'
        exit 0
    }

    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    [System.Windows.Forms.Application]::EnableVisualStyles()

    $colors = @{
        Window = [System.Drawing.Color]::FromArgb(9, 16, 29)
        Panel = [System.Drawing.Color]::FromArgb(17, 27, 46)
        PanelRaised = [System.Drawing.Color]::FromArgb(24, 38, 61)
        Text = [System.Drawing.Color]::FromArgb(235, 242, 252)
        Muted = [System.Drawing.Color]::FromArgb(151, 166, 190)
        Accent = [System.Drawing.Color]::FromArgb(37, 150, 255)
        AccentHover = [System.Drawing.Color]::FromArgb(67, 170, 255)
        Success = [System.Drawing.Color]::FromArgb(50, 205, 132)
        Failure = [System.Drawing.Color]::FromArgb(255, 99, 112)
        Warning = [System.Drawing.Color]::FromArgb(255, 190, 80)
        Border = [System.Drawing.Color]::FromArgb(44, 62, 88)
    }
    $uiFont = [System.Drawing.Font]::new('Segoe UI', 10)
    $smallFont = [System.Drawing.Font]::new('Segoe UI', 9)
    $titleFont = [System.Drawing.Font]::new(
        'Segoe UI Semibold', 21, [System.Drawing.FontStyle]::Bold)
    $sectionFont = [System.Drawing.Font]::new(
        'Segoe UI Semibold', 11, [System.Drawing.FontStyle]::Bold)
    $monoFont = [System.Drawing.Font]::new('Consolas', 10)

    $form = [System.Windows.Forms.Form]::new()
    $form.Text = 'BMW E9x Remote Control — Simulateur hors véhicule'
    $form.StartPosition = 'CenterScreen'
    $form.MinimumSize = [System.Drawing.Size]::new(1050, 780)
    $form.Size = [System.Drawing.Size]::new(1180, 820)
    $form.BackColor = $colors.Window
    $form.ForeColor = $colors.Text
    $form.Font = $uiFont

    $header = [System.Windows.Forms.Panel]::new()
    $header.Dock = 'Top'
    $header.Height = 94
    $header.BackColor = $colors.Window
    $header.Padding = [System.Windows.Forms.Padding]::new(24, 14, 24, 8)

    $title = [System.Windows.Forms.Label]::new()
    $title.Text = 'BMW E9x Remote Control'
    $title.Font = $titleFont
    $title.ForeColor = $colors.Text
    $title.AutoSize = $true
    $title.Location = [System.Drawing.Point]::new(22, 12)
    [void]$header.Controls.Add($title)

    $safetyBanner = [System.Windows.Forms.Label]::new()
    $safetyBanner.Text = '●  MODE HORS VÉHICULE — aucun bus, GPIO ou actionneur réel'
    $safetyBanner.Font = $smallFont
    $safetyBanner.ForeColor = $colors.Warning
    $safetyBanner.AutoSize = $true
    $safetyBanner.Location = [System.Drawing.Point]::new(26, 58)
    [void]$header.Controls.Add($safetyBanner)

    $sidebar = [System.Windows.Forms.Panel]::new()
    $sidebar.Dock = 'Left'
    $sidebar.Width = 370
    $sidebar.BackColor = $colors.Panel
    $sidebar.Padding = [System.Windows.Forms.Padding]::new(20)

    $sidebarLayout = [System.Windows.Forms.TableLayoutPanel]::new()
    $sidebarLayout.Dock = 'Fill'
    $sidebarLayout.ColumnCount = 1
    $sidebarLayout.RowCount = 10
    $sidebarLayout.BackColor = $colors.Panel
    [void]$sidebarLayout.ColumnStyles.Add(
        [System.Windows.Forms.ColumnStyle]::new(
            [System.Windows.Forms.SizeType]::Percent, 100))
    foreach ($height in @(30, 220, 28, 75, 28, 46, 46, 46, 46, 35)) {
        [void]$sidebarLayout.RowStyles.Add(
            [System.Windows.Forms.RowStyle]::new(
                [System.Windows.Forms.SizeType]::Absolute, $height))
    }

    $scenarioHeading = [System.Windows.Forms.Label]::new()
    $scenarioHeading.Text = 'SCÉNARIOS DISPONIBLES'
    $scenarioHeading.Font = $sectionFont
    $scenarioHeading.ForeColor = $colors.Text
    $scenarioHeading.Dock = 'Fill'
    [void]$sidebarLayout.Controls.Add($scenarioHeading, 0, 0)

    $scenarioList = [System.Windows.Forms.ListBox]::new()
    $scenarioList.Dock = 'Fill'
    $scenarioList.BackColor = $colors.PanelRaised
    $scenarioList.ForeColor = $colors.Text
    $scenarioList.BorderStyle = 'FixedSingle'
    $scenarioList.Font = $uiFont
    $scenarioList.IntegralHeight = $false
    $scenarioList.ItemHeight = 28
    foreach ($scenario in $scenarioDefinitions) {
        [void]$scenarioList.Items.Add($scenario.Label)
    }
    $scenarioList.SelectedIndex = 0
    [void]$sidebarLayout.Controls.Add($scenarioList, 0, 1)

    $descriptionHeading = [System.Windows.Forms.Label]::new()
    $descriptionHeading.Text = 'DESCRIPTION'
    $descriptionHeading.Font = $sectionFont
    $descriptionHeading.ForeColor = $colors.Text
    $descriptionHeading.Dock = 'Fill'
    $descriptionHeading.Padding = [System.Windows.Forms.Padding]::new(0, 5, 0, 0)
    [void]$sidebarLayout.Controls.Add($descriptionHeading, 0, 2)

    $description = [System.Windows.Forms.Label]::new()
    $description.Text = $scenarioDefinitions[0].Description
    $description.ForeColor = $colors.Muted
    $description.Dock = 'Fill'
    $description.Padding = [System.Windows.Forms.Padding]::new(0, 4, 0, 0)
    [void]$sidebarLayout.Controls.Add($description, 0, 3)

    $scenarioCode = [System.Windows.Forms.Label]::new()
    $scenarioCode.Text = "Code : $($scenarioDefinitions[0].Code)"
    $scenarioCode.Font = $smallFont
    $scenarioCode.ForeColor = $colors.Accent
    $scenarioCode.Dock = 'Fill'
    [void]$sidebarLayout.Controls.Add($scenarioCode, 0, 4)

    function New-FlatButton {
        param(
            [Parameter(Mandatory)][string]$Text,
            [System.Drawing.Color]$BackColor = $colors.PanelRaised
        )

        $button = [System.Windows.Forms.Button]::new()
        $button.Text = $Text
        $button.Dock = 'Fill'
        $button.Margin = [System.Windows.Forms.Padding]::new(0, 4, 0, 4)
        $button.FlatStyle = 'Flat'
        $button.FlatAppearance.BorderSize = 1
        $button.FlatAppearance.BorderColor = $colors.Border
        $button.BackColor = $BackColor
        $button.ForeColor = $colors.Text
        $button.Cursor = [System.Windows.Forms.Cursors]::Hand
        return $button
    }

    $runButton = New-FlatButton '▶  Lancer le scénario' $colors.Accent
    [void]$sidebarLayout.Controls.Add($runButton, 0, 5)
    $sandboxButton = New-FlatButton '◉  Ouvrir le bac à sable' $colors.Success
    [void]$sidebarLayout.Controls.Add($sandboxButton, 0, 6)
    $configButton = New-FlatButton '⚙  Tester une configuration…'
    [void]$sidebarLayout.Controls.Add($configButton, 0, 7)
    $traceButton = New-FlatButton '⌁  Inspecter une trace CAN…'
    [void]$sidebarLayout.Controls.Add($traceButton, 0, 8)

    $hoodRequired = [System.Windows.Forms.CheckBox]::new()
    $hoodRequired.Text = "Capot requis pour l'inspection de trace"
    $hoodRequired.Checked = $true
    $hoodRequired.ForeColor = $colors.Muted
    $hoodRequired.Dock = 'Fill'
    [void]$sidebarLayout.Controls.Add($hoodRequired, 0, 9)
    [void]$sidebar.Controls.Add($sidebarLayout)

    $mainPanel = [System.Windows.Forms.Panel]::new()
    $mainPanel.Dock = 'Fill'
    $mainPanel.BackColor = $colors.Window
    $mainPanel.Padding = [System.Windows.Forms.Padding]::new(22, 8, 22, 20)

    $mainLayout = [System.Windows.Forms.TableLayoutPanel]::new()
    $mainLayout.Dock = 'Fill'
    $mainLayout.ColumnCount = 1
    $mainLayout.RowCount = 2
    [void]$mainLayout.ColumnStyles.Add(
        [System.Windows.Forms.ColumnStyle]::new(
            [System.Windows.Forms.SizeType]::Percent, 100))
    [void]$mainLayout.RowStyles.Add(
        [System.Windows.Forms.RowStyle]::new(
            [System.Windows.Forms.SizeType]::Absolute, 64))
    [void]$mainLayout.RowStyles.Add(
        [System.Windows.Forms.RowStyle]::new(
            [System.Windows.Forms.SizeType]::Percent, 100))

    $statusPanel = [System.Windows.Forms.Panel]::new()
    $statusPanel.Dock = 'Fill'
    $statusPanel.BackColor = $colors.Panel
    $statusPanel.Padding = [System.Windows.Forms.Padding]::new(16, 10, 16, 8)
    $statusLabel = [System.Windows.Forms.Label]::new()
    $statusLabel.Text = 'PRÊT — choisissez un scénario'
    $statusLabel.Font = $sectionFont
    $statusLabel.ForeColor = $colors.Muted
    $statusLabel.Dock = 'Fill'
    $statusLabel.TextAlign = 'MiddleLeft'
    $clearButton = New-FlatButton 'Effacer'
    $clearButton.Dock = 'Right'
    $clearButton.Width = 105
    $clearButton.Margin = [System.Windows.Forms.Padding]::new(6, 2, 0, 2)
    $copyButton = New-FlatButton 'Copier le rapport'
    $copyButton.Dock = 'Right'
    $copyButton.Width = 145
    $copyButton.Margin = [System.Windows.Forms.Padding]::new(6, 2, 0, 2)
    [void]$statusPanel.Controls.Add($statusLabel)
    [void]$statusPanel.Controls.Add($clearButton)
    [void]$statusPanel.Controls.Add($copyButton)
    [void]$mainLayout.Controls.Add($statusPanel, 0, 0)

    $output = [System.Windows.Forms.RichTextBox]::new()
    $output.Dock = 'Fill'
    $output.ReadOnly = $true
    $output.BackColor = [System.Drawing.Color]::FromArgb(6, 12, 23)
    $output.ForeColor = $colors.Text
    $output.BorderStyle = 'FixedSingle'
    $output.Font = $monoFont
    $output.WordWrap = $false
    $output.Text = "Le résultat détaillé apparaîtra ici.`r`n`r`nAucune connexion au véhicule n'est utilisée."
    [void]$mainLayout.Controls.Add($output, 0, 1)

    [void]$mainPanel.Controls.Add($mainLayout)
    [void]$form.Controls.Add($mainPanel)
    [void]$form.Controls.Add($sidebar)
    [void]$form.Controls.Add($header)

    function Set-UiBusy {
        param([bool]$Busy)
        $runButton.Enabled = -not $Busy
        $sandboxButton.Enabled = -not $Busy
        $configButton.Enabled = -not $Busy
        $traceButton.Enabled = -not $Busy
        $scenarioList.Enabled = -not $Busy
        $form.UseWaitCursor = $Busy
        [System.Windows.Forms.Application]::DoEvents()
    }

    function Set-RunStatus {
        param(
            [Parameter(Mandatory)][string]$Text,
            [Parameter(Mandatory)][ValidateSet('Ready', 'Running', 'Pass', 'Fail')]
            [string]$Kind
        )
        $statusLabel.Text = $Text
        $statusLabel.ForeColor = switch ($Kind) {
            'Running' { $colors.Warning }
            'Pass' { $colors.Success }
            'Fail' { $colors.Failure }
            default { $colors.Muted }
        }
    }

    function Show-SimulatorResult {
        param(
            [Parameter(Mandatory)][string[]]$Arguments,
            [Parameter(Mandatory)][string]$SuccessMessage,
            [bool]$RequireScenarioPass = $true
        )
        try {
            Set-UiBusy $true
            Set-RunStatus 'EXÉCUTION EN COURS…' 'Running'
            $result = Invoke-SimulatorProcess $Arguments
            $report = "> $($result.Command)`r`n`r`n$($result.StandardOutput)"
            if (-not [string]::IsNullOrWhiteSpace($result.StandardError)) {
                $report += "`r`n--- erreurs ---`r`n$($result.StandardError)"
            }
            $output.Text = $report.TrimEnd()
            $output.SelectionStart = 0
            $output.ScrollToCaret()
            $passed = $result.ExitCode -eq 0 -and
                (-not $RequireScenarioPass -or
                 $result.StandardOutput -match 'scenario_result: PASS')
            if ($passed) {
                Set-RunStatus "RÉUSSI — $SuccessMessage" 'Pass'
            } else {
                Set-RunStatus "ÉCHEC — code $($result.ExitCode)" 'Fail'
            }
        } catch {
            $output.Text = $_ | Out-String
            Set-RunStatus 'ERREUR LORS DU LANCEMENT' 'Fail'
        } finally {
            Set-UiBusy $false
        }
    }

    function Show-SandboxWindow {
        param([string]$PreviewFile)

        $sandboxForm = [System.Windows.Forms.Form]::new()
        $sandboxForm.Text = 'BMW E9x Remote Control — Bac à sable interactif'
        $sandboxForm.StartPosition = 'CenterParent'
        $sandboxForm.MinimumSize = [System.Drawing.Size]::new(1180, 820)
        $sandboxForm.Size = [System.Drawing.Size]::new(1320, 900)
        $sandboxForm.BackColor = $colors.Window
        $sandboxForm.ForeColor = $colors.Text
        $sandboxForm.Font = $uiFont

        $sandboxHeader = [System.Windows.Forms.Panel]::new()
        $sandboxHeader.Dock = 'Top'
        $sandboxHeader.Height = 104
        $sandboxHeader.BackColor = $colors.Panel
        $sandboxHeader.Padding = [System.Windows.Forms.Padding]::new(22, 12, 22, 8)

        $sandboxTitle = [System.Windows.Forms.Label]::new()
        $sandboxTitle.Text = 'REPOS'
        $sandboxTitle.Font = $titleFont
        $sandboxTitle.ForeColor = $colors.Success
        $sandboxTitle.AutoSize = $true
        $sandboxTitle.Location = [System.Drawing.Point]::new(20, 10)
        [void]$sandboxHeader.Controls.Add($sandboxTitle)

        $sandboxSummary = [System.Windows.Forms.Label]::new()
        $sandboxSummary.Text = 'Initialisation de la session…'
        $sandboxSummary.ForeColor = $colors.Muted
        $sandboxSummary.AutoSize = $true
        $sandboxSummary.Location = [System.Drawing.Point]::new(23, 54)
        [void]$sandboxHeader.Controls.Add($sandboxSummary)

        $sandboxSafety = [System.Windows.Forms.Label]::new()
        $sandboxSafety.Text = '●  PROCESSUS LOCAL — aucune connexion au véhicule'
        $sandboxSafety.Font = $smallFont
        $sandboxSafety.ForeColor = $colors.Warning
        $sandboxSafety.AutoSize = $true
        $sandboxSafety.Location = [System.Drawing.Point]::new(775, 20)
        [void]$sandboxHeader.Controls.Add($sandboxSafety)

        $sandboxContent = [System.Windows.Forms.TableLayoutPanel]::new()
        $sandboxContent.Dock = 'Fill'
        $sandboxContent.ColumnCount = 2
        $sandboxContent.RowCount = 1
        $sandboxContent.Padding = [System.Windows.Forms.Padding]::new(14)
        $sandboxContent.BackColor = $colors.Window
        [void]$sandboxContent.ColumnStyles.Add(
            [System.Windows.Forms.ColumnStyle]::new(
                [System.Windows.Forms.SizeType]::Absolute, 500))
        [void]$sandboxContent.ColumnStyles.Add(
            [System.Windows.Forms.ColumnStyle]::new(
                [System.Windows.Forms.SizeType]::Percent, 100))

        $controlsPanel = [System.Windows.Forms.Panel]::new()
        $controlsPanel.Dock = 'Fill'
        $controlsPanel.BackColor = $colors.Panel
        $controlsPanel.Padding = [System.Windows.Forms.Padding]::new(16)

        $vehicleLayout = [System.Windows.Forms.TableLayoutPanel]::new()
        $vehicleLayout.Dock = 'Top'
        $vehicleLayout.Height = 570
        $vehicleLayout.ColumnCount = 2
        $vehicleLayout.RowCount = 15
        [void]$vehicleLayout.ColumnStyles.Add(
            [System.Windows.Forms.ColumnStyle]::new(
                [System.Windows.Forms.SizeType]::Percent, 50))
        [void]$vehicleLayout.ColumnStyles.Add(
            [System.Windows.Forms.ColumnStyle]::new(
                [System.Windows.Forms.SizeType]::Percent, 50))
        foreach ($rowHeight in @(36, 38, 38, 38, 38, 36, 36, 36, 38, 36, 36, 36, 36, 40, 58)) {
            [void]$vehicleLayout.RowStyles.Add(
                [System.Windows.Forms.RowStyle]::new(
                    [System.Windows.Forms.SizeType]::Absolute, $rowHeight))
        }

        $vehicleHeading = [System.Windows.Forms.Label]::new()
        $vehicleHeading.Text = 'ÉTAT DU VÉHICULE'
        $vehicleHeading.Font = $sectionFont
        $vehicleHeading.ForeColor = $colors.Text
        $vehicleHeading.Dock = 'Fill'
        [void]$vehicleLayout.Controls.Add($vehicleHeading, 0, 0)
        $vehicleLayout.SetColumnSpan($vehicleHeading, 2)

        function New-SandboxFieldLabel {
            param([string]$Text)
            $label = [System.Windows.Forms.Label]::new()
            $label.Text = $Text
            $label.ForeColor = $colors.Muted
            $label.Dock = 'Fill'
            $label.TextAlign = 'MiddleLeft'
            return $label
        }

        function New-SandboxCheckBox {
            param([string]$Text, [bool]$Checked)
            $checkBox = [System.Windows.Forms.CheckBox]::new()
            $checkBox.Text = $Text
            $checkBox.Checked = $Checked
            $checkBox.ForeColor = $colors.Text
            $checkBox.Dock = 'Fill'
            return $checkBox
        }

        $rpmLabel = New-SandboxFieldLabel 'Régime moteur (tr/min)'
        [void]$vehicleLayout.Controls.Add($rpmLabel, 0, 1)
        $rpmInput = [System.Windows.Forms.NumericUpDown]::new()
        $rpmInput.Minimum = 0
        $rpmInput.Maximum = 8000
        $rpmInput.Increment = 50
        $rpmInput.BackColor = $colors.PanelRaised
        $rpmInput.ForeColor = $colors.Text
        $rpmInput.Dock = 'Fill'
        [void]$vehicleLayout.Controls.Add($rpmInput, 1, 1)

        $coolantLabel = New-SandboxFieldLabel 'Liquide refroidissement (°C)'
        [void]$vehicleLayout.Controls.Add($coolantLabel, 0, 2)
        $coolantInput = [System.Windows.Forms.NumericUpDown]::new()
        $coolantInput.Minimum = -40
        $coolantInput.Maximum = 215
        $coolantInput.Value = 20
        $coolantInput.BackColor = $colors.PanelRaised
        $coolantInput.ForeColor = $colors.Text
        $coolantInput.Dock = 'Fill'
        [void]$vehicleLayout.Controls.Add($coolantInput, 1, 2)

        $oilLabel = New-SandboxFieldLabel 'Huile moteur (°C)'
        [void]$vehicleLayout.Controls.Add($oilLabel, 0, 3)
        $oilInput = [System.Windows.Forms.NumericUpDown]::new()
        $oilInput.Minimum = -40
        $oilInput.Maximum = 215
        $oilInput.Value = 20
        $oilInput.BackColor = $colors.PanelRaised
        $oilInput.ForeColor = $colors.Text
        $oilInput.Dock = 'Fill'
        [void]$vehicleLayout.Controls.Add($oilInput, 1, 3)

        $transmissionTemperatureLabel = New-SandboxFieldLabel 'Huile boîte (°C)'
        [void]$vehicleLayout.Controls.Add($transmissionTemperatureLabel, 0, 4)
        $transmissionTemperatureInput = [System.Windows.Forms.NumericUpDown]::new()
        $transmissionTemperatureInput.Minimum = -40
        $transmissionTemperatureInput.Maximum = 215
        $transmissionTemperatureInput.Value = 20
        $transmissionTemperatureInput.BackColor = $colors.PanelRaised
        $transmissionTemperatureInput.ForeColor = $colors.Text
        $transmissionTemperatureInput.Dock = 'Fill'
        [void]$vehicleLayout.Controls.Add($transmissionTemperatureInput, 1, 4)

        $dpfActiveInput = New-SandboxCheckBox 'Régénération FAP active' $false
        [void]$vehicleLayout.Controls.Add($dpfActiveInput, 0, 5)
        $vehicleLayout.SetColumnSpan($dpfActiveInput, 2)

        $coldGuardFeatureInput = New-SandboxCheckBox 'Protection moteur froid' $false
        $dpfFeatureInput = New-SandboxCheckBox 'Indicateur FAP' $false
        [void]$vehicleLayout.Controls.Add($coldGuardFeatureInput, 0, 6)
        [void]$vehicleLayout.Controls.Add($dpfFeatureInput, 1, 6)

        $transmissionAlertFeatureInput =
            New-SandboxCheckBox 'Alerte surchauffe boîte' $false
        [void]$vehicleLayout.Controls.Add($transmissionAlertFeatureInput, 0, 7)
        $vehicleLayout.SetColumnSpan($transmissionAlertFeatureInput, 2)

        $gearLabel = New-SandboxFieldLabel 'Rapport de boîte'
        [void]$vehicleLayout.Controls.Add($gearLabel, 0, 8)
        $gearInput = [System.Windows.Forms.ComboBox]::new()
        $gearInput.DropDownStyle = 'DropDownList'
        $gearInput.BackColor = $colors.PanelRaised
        $gearInput.ForeColor = $colors.Text
        $gearInput.Dock = 'Fill'
        [void]$gearInput.Items.AddRange(@('park', 'neutral', 'reverse', 'drive'))
        $gearInput.SelectedItem = 'park'
        [void]$vehicleLayout.Controls.Add($gearInput, 1, 8)

        $doorsClosedInput = New-SandboxCheckBox 'Portes fermées' $true
        $trunkClosedInput = New-SandboxCheckBox 'Coffre fermé' $true
        [void]$vehicleLayout.Controls.Add($doorsClosedInput, 0, 9)
        [void]$vehicleLayout.Controls.Add($trunkClosedInput, 1, 9)

        $hoodAvailableInput = New-SandboxCheckBox 'Signal capot disponible' $true
        $hoodClosedInput = New-SandboxCheckBox 'Capot fermé' $true
        [void]$vehicleLayout.Controls.Add($hoodAvailableInput, 0, 10)
        [void]$vehicleLayout.Controls.Add($hoodClosedInput, 1, 10)

        $brakeInput = New-SandboxCheckBox 'Frein appuyé' $false
        $parkingInput = New-SandboxCheckBox 'Frein parking serré' $true
        [void]$vehicleLayout.Controls.Add($brakeInput, 0, 11)
        [void]$vehicleLayout.Controls.Add($parkingInput, 1, 11)

        $criticalInput = New-SandboxCheckBox 'Défaut critique' $false
        $interlockInput = New-SandboxCheckBox 'Autorisation matérielle' $true
        [void]$vehicleLayout.Controls.Add($criticalInput, 0, 12)
        [void]$vehicleLayout.Controls.Add($interlockInput, 1, 12)

        $hoodModeLabel = New-SandboxFieldLabel 'Surveillance du capot'
        [void]$vehicleLayout.Controls.Add($hoodModeLabel, 0, 13)
        $hoodModeInput = [System.Windows.Forms.ComboBox]::new()
        $hoodModeInput.DropDownStyle = 'DropDownList'
        $hoodModeInput.BackColor = $colors.PanelRaised
        $hoodModeInput.ForeColor = $colors.Text
        $hoodModeInput.Dock = 'Fill'
        [void]$hoodModeInput.Items.AddRange(@('Obligatoire', 'Facultative'))
        $hoodModeInput.SelectedIndex = 0
        [void]$vehicleLayout.Controls.Add($hoodModeInput, 1, 13)

        $applyVehicleButton = New-FlatButton "APPLIQUER L'ÉTAT" $colors.Accent
        [void]$vehicleLayout.Controls.Add($applyVehicleButton, 0, 14)
        $vehicleLayout.SetColumnSpan($applyVehicleButton, 2)

        $actionsLayout = [System.Windows.Forms.TableLayoutPanel]::new()
        $actionsLayout.Dock = 'Fill'
        $actionsLayout.Padding = [System.Windows.Forms.Padding]::new(0, 8, 0, 0)
        $actionsLayout.ColumnCount = 2
        $actionsLayout.RowCount = 4
        [void]$actionsLayout.ColumnStyles.Add(
            [System.Windows.Forms.ColumnStyle]::new(
                [System.Windows.Forms.SizeType]::Percent, 50))
        [void]$actionsLayout.ColumnStyles.Add(
            [System.Windows.Forms.ColumnStyle]::new(
                [System.Windows.Forms.SizeType]::Percent, 50))
        foreach ($index in 1..4) {
            [void]$actionsLayout.RowStyles.Add(
                [System.Windows.Forms.RowStyle]::new(
                    [System.Windows.Forms.SizeType]::Percent, 25))
        }
        $newSessionButton = New-FlatButton 'Nouvelle session'
        $startRemoteButton = New-FlatButton 'Démarrage distant' $colors.Success
        $timerButton = New-FlatButton 'Échéance du timer'
        $stopRemoteButton = New-FlatButton 'Arrêt distant' $colors.Failure
        $takeoverButton = New-FlatButton 'Confirmer la reprise'
        $resetButton = New-FlatButton 'Réarmer le défaut'
        $watchdogButton = New-FlatButton 'Perdre le heartbeat' $colors.Warning
        [void]$actionsLayout.Controls.Add($newSessionButton, 0, 0)
        $actionsLayout.SetColumnSpan($newSessionButton, 2)
        [void]$actionsLayout.Controls.Add($startRemoteButton, 0, 1)
        [void]$actionsLayout.Controls.Add($timerButton, 1, 1)
        [void]$actionsLayout.Controls.Add($stopRemoteButton, 0, 2)
        [void]$actionsLayout.Controls.Add($takeoverButton, 1, 2)
        [void]$actionsLayout.Controls.Add($resetButton, 0, 3)
        [void]$actionsLayout.Controls.Add($watchdogButton, 1, 3)
        [void]$controlsPanel.Controls.Add($actionsLayout)
        [void]$controlsPanel.Controls.Add($vehicleLayout)

        $dashboardPanel = [System.Windows.Forms.TableLayoutPanel]::new()
        $dashboardPanel.Dock = 'Fill'
        $dashboardPanel.BackColor = $colors.Window
        $dashboardPanel.Padding = [System.Windows.Forms.Padding]::new(14, 0, 0, 0)
        $dashboardPanel.ColumnCount = 1
        $dashboardPanel.RowCount = 2
        [void]$dashboardPanel.ColumnStyles.Add(
            [System.Windows.Forms.ColumnStyle]::new(
                [System.Windows.Forms.SizeType]::Percent, 100))
        [void]$dashboardPanel.RowStyles.Add(
            [System.Windows.Forms.RowStyle]::new(
                [System.Windows.Forms.SizeType]::Absolute, 194))
        [void]$dashboardPanel.RowStyles.Add(
            [System.Windows.Forms.RowStyle]::new(
                [System.Windows.Forms.SizeType]::Percent, 100))

        $dashboardCards = [System.Windows.Forms.Panel]::new()
        $dashboardCards.Dock = 'Fill'
        $dashboardCards.BackColor = $colors.Panel
        $dashboardCards.Padding = [System.Windows.Forms.Padding]::new(18, 12, 18, 10)

        $outputsLabel = [System.Windows.Forms.Label]::new()
        $outputsLabel.Text = 'SORTIES  —  ALLUMAGE OFF   |   DÉMARREUR OFF'
        $outputsLabel.Font = $sectionFont
        $outputsLabel.ForeColor = $colors.Success
        $outputsLabel.Dock = 'Top'
        $outputsLabel.Height = 34

        $timerStatusLabel = [System.Windows.Forms.Label]::new()
        $timerStatusLabel.Text = 'TIMER  —  inactif'
        $timerStatusLabel.ForeColor = $colors.Muted
        $timerStatusLabel.Dock = 'Top'
        $timerStatusLabel.Height = 30

        $telemetryStatusLabel = [System.Windows.Forms.Label]::new()
        $telemetryStatusLabel.Text = 'TÉLÉMÉTRIE  —  options désactivées'
        $telemetryStatusLabel.ForeColor = $colors.Muted
        $telemetryStatusLabel.Dock = 'Top'
        $telemetryStatusLabel.Height = 52

        $eventStatusLabel = [System.Windows.Forms.Label]::new()
        $eventStatusLabel.Text = 'Dernier événement : aucun'
        $eventStatusLabel.ForeColor = $colors.Muted
        $eventStatusLabel.Dock = 'Fill'
        [void]$dashboardCards.Controls.Add($eventStatusLabel)
        [void]$dashboardCards.Controls.Add($telemetryStatusLabel)
        [void]$dashboardCards.Controls.Add($timerStatusLabel)
        [void]$dashboardCards.Controls.Add($outputsLabel)

        $sandboxLog = [System.Windows.Forms.RichTextBox]::new()
        $sandboxLog.Dock = 'Fill'
        $sandboxLog.ReadOnly = $true
        $sandboxLog.BackColor = [System.Drawing.Color]::FromArgb(6, 12, 23)
        $sandboxLog.ForeColor = $colors.Text
        $sandboxLog.BorderStyle = 'FixedSingle'
        $sandboxLog.Font = $monoFont
        $sandboxLog.WordWrap = $true
        $sandboxLog.Text = "Journal de la session interactive.`r`n"

        [void]$dashboardPanel.Controls.Add($dashboardCards, 0, 0)
        [void]$dashboardPanel.Controls.Add($sandboxLog, 0, 1)
        [void]$sandboxContent.Controls.Add($controlsPanel, 0, 0)
        [void]$sandboxContent.Controls.Add($dashboardPanel, 1, 0)
        [void]$sandboxForm.Controls.Add($sandboxContent)
        [void]$sandboxForm.Controls.Add($sandboxHeader)

        $stateNames = @{
            idle = 'REPOS'
            authorizing = 'AUTORISATION'
            preparing = 'PRÉPARATION'
            cranking = 'DÉMARRAGE'
            running = 'FONCTIONNEMENT'
            awaiting_takeover = 'ATTENTE REPRISE CONDUCTEUR'
            driver_control = 'CONTRÔLE CONDUCTEUR'
            stopping = 'ARRÊT'
            fault = 'DÉFAUT'
        }
        $sandboxFailureColor = $colors.Failure
        $sandboxSuccessColor = $colors.Success
        $sandboxWarningColor = $colors.Warning
        $sandboxMutedColor = $colors.Muted

        $renderSnapshot = {
            param($Snapshot)
            $translatedState = $stateNames[[string]$Snapshot.state]
            if ([string]::IsNullOrWhiteSpace($translatedState)) {
                $translatedState = ([string]$Snapshot.state).ToUpperInvariant()
            }
            $sandboxTitle.Text = $translatedState
            $sandboxTitle.ForeColor = if ($Snapshot.state -eq 'fault') {
                $sandboxFailureColor
            } elseif ($Snapshot.state -eq 'idle' -or
                      $Snapshot.state -eq 'driver_control') {
                $sandboxSuccessColor
            } else {
                $sandboxWarningColor
            }
            $sandboxSummary.Text =
                "Temps $($Snapshot.time_ms) ms  •  défaut $($Snapshot.fault)  •  superviseur $($Snapshot.supervisor_fault)"

            $ignitionText = if ($Snapshot.ignition_active) { 'ON' } else { 'OFF' }
            $starterText = if ($Snapshot.starter_active) { 'ON' } else { 'OFF' }
            $outputsLabel.Text =
                "SORTIES  —  ALLUMAGE $ignitionText   |   DÉMARREUR $starterText"
            $outputsLabel.ForeColor = if ($Snapshot.supervisor_fault -ne 'none') {
                $sandboxFailureColor
            } elseif ($Snapshot.ignition_active -or $Snapshot.starter_active) {
                $sandboxWarningColor
            } else {
                $sandboxSuccessColor
            }
            if ($Snapshot.timer_armed) {
                $remaining = [int64]$Snapshot.timer_due_ms - [int64]$Snapshot.time_ms
                $timerStatusLabel.Text =
                    "TIMER  —  $remaining ms restantes (durée $($Snapshot.timer_duration_ms) ms)"
            } else {
                $timerStatusLabel.Text = 'TIMER  —  inactif'
            }
            $actions = @($Snapshot.last_actions) -join ', '
            if ([string]::IsNullOrWhiteSpace($actions)) {
                $actions = 'aucune'
            }
            $eventStatusLabel.Text =
                "Dernier événement : $($Snapshot.last_event)`r`nActions : $actions  •  diagnostics : $($Snapshot.diagnostic_records)"

            $rpmInput.Value = [decimal]$Snapshot.vehicle.rpm
            $coolantInput.Value = [decimal]$Snapshot.vehicle.coolant_temperature_c
            $oilInput.Value = [decimal]$Snapshot.vehicle.engine_oil_temperature_c
            $transmissionTemperatureInput.Value =
                [decimal]$Snapshot.vehicle.transmission_oil_temperature_c
            $dpfActiveInput.Checked =
                [bool]$Snapshot.vehicle.dpf_regeneration_active
            $coldGuardFeatureInput.Checked =
                [bool]$Snapshot.features.cold_engine_guard
            $dpfFeatureInput.Checked =
                [bool]$Snapshot.features.dpf_regeneration_indicator
            $transmissionAlertFeatureInput.Checked =
                [bool]$Snapshot.features.transmission_overheat_alert
            $gearInput.SelectedItem = [string]$Snapshot.vehicle.gear
            $doorsClosedInput.Checked = [bool]$Snapshot.vehicle.doors_closed
            $trunkClosedInput.Checked = [bool]$Snapshot.vehicle.trunk_closed
            $hoodAvailableInput.Checked = [bool]$Snapshot.vehicle.hood_available
            $hoodClosedInput.Checked = [bool]$Snapshot.vehicle.hood_closed
            $hoodClosedInput.Enabled = $hoodAvailableInput.Checked
            $brakeInput.Checked = [bool]$Snapshot.vehicle.brake_pressed
            $parkingInput.Checked = [bool]$Snapshot.vehicle.parking_applied
            $criticalInput.Checked = [bool]$Snapshot.vehicle.critical_fault
            $interlockInput.Checked = [bool]$Snapshot.hardware_start_permitted
            $hoodModeInput.SelectedIndex = if ($Snapshot.hood_monitoring_required) { 0 } else { 1 }

            $telemetryAlerts = @($Snapshot.telemetry.alerts) -join ', '
            if ([string]::IsNullOrWhiteSpace($telemetryAlerts)) {
                $telemetryAlerts = 'aucune nouvelle alerte'
            }
            $telemetryStatusLabel.Text =
                "TÉLÉMÉTRIE  —  froid=$($Snapshot.telemetry.cold_engine_guard)  |  FAP=$($Snapshot.telemetry.dpf_regeneration)  |  boîte=$($Snapshot.telemetry.transmission_overheat)`r`nAlertes : $telemetryAlerts"
            $telemetryStatusLabel.ForeColor = if (
                $Snapshot.telemetry.cold_engine_guard -eq 'active' -or
                $Snapshot.telemetry.transmission_overheat -eq 'active') {
                $sandboxFailureColor
            } elseif ($Snapshot.telemetry.dpf_regeneration -eq 'active') {
                $sandboxWarningColor
            } else {
                $sandboxMutedColor
            }
        }.GetNewClosure()

        $sandboxProcess = Start-SandboxProcess
        $sandboxContext = @{
            Process = $sandboxProcess
            Closed = $false
        }
        $invokeSandboxCommand = ${function:Invoke-SandboxCommand}
        $stopSandboxProcess = ${function:Stop-SandboxProcess}

        $sendCommand = {
            param([string]$Command)
            try {
                $sandboxForm.UseWaitCursor = $true
                [System.Windows.Forms.Application]::DoEvents()
                $result = & $invokeSandboxCommand $sandboxContext.Process $Command
                $timestamp = Get-Date -Format 'HH:mm:ss'
                $sandboxLog.AppendText("`r`n[$timestamp] > $Command`r`n")
                if ($result.ok) {
                    $sandboxLog.AppendText(
                        "  état=$($result.state), défaut=$($result.fault), sorties=$($result.ignition_active)/$($result.starter_active)`r`n")
                } else {
                    $sandboxLog.AppendText("  REFUSÉ : $($result.error)`r`n")
                }
                $sandboxLog.SelectionStart = $sandboxLog.TextLength
                $sandboxLog.ScrollToCaret()
                & $renderSnapshot $result
                return $result
            } catch {
                $sandboxError = $_.Exception.Message
                $sandboxLog.AppendText("`r`nERREUR : $sandboxError`r`n")
                $sandboxTitle.Text = 'ERREUR DU SIMULATEUR'
                $sandboxTitle.ForeColor = [System.Drawing.Color]::Red
                if (-not [string]::IsNullOrWhiteSpace($PreviewFile)) {
                    throw "Echec du rendu du bac a sable : $sandboxError"
                }
                return $null
            } finally {
                $sandboxForm.UseWaitCursor = $false
            }
        }.GetNewClosure()

        $hoodAvailableInput.Add_CheckedChanged({
            $hoodClosedInput.Enabled = $hoodAvailableInput.Checked
        }.GetNewClosure())
        $newSessionButton.Add_Click({
            $mode = if ($hoodModeInput.SelectedIndex -eq 0) { 'required' } else { 'optional' }
            [void](& $sendCommand "new $mode")
        }.GetNewClosure())
        $startRemoteButton.Add_Click({ [void](& $sendCommand 'start') }.GetNewClosure())
        $timerButton.Add_Click({ [void](& $sendCommand 'timer') }.GetNewClosure())
        $stopRemoteButton.Add_Click({ [void](& $sendCommand 'stop') }.GetNewClosure())
        $takeoverButton.Add_Click({ [void](& $sendCommand 'takeover') }.GetNewClosure())
        $resetButton.Add_Click({ [void](& $sendCommand 'reset') }.GetNewClosure())
        $watchdogButton.Add_Click({ [void](& $sendCommand 'watchdog') }.GetNewClosure())
        $applyVehicleButton.Add_Click({
            $requestedRpm = [int]$rpmInput.Value
            $requestedCoolant = [int]$coolantInput.Value
            $requestedOil = [int]$oilInput.Value
            $requestedTransmissionTemperature =
                [int]$transmissionTemperatureInput.Value
            $requestedDpf = if ($dpfActiveInput.Checked) { 'on' } else { 'off' }
            $requestedColdGuard = if ($coldGuardFeatureInput.Checked) { 'on' } else { 'off' }
            $requestedDpfFeature = if ($dpfFeatureInput.Checked) { 'on' } else { 'off' }
            $requestedTransmissionAlert =
                if ($transmissionAlertFeatureInput.Checked) { 'on' } else { 'off' }
            [void](& $sendCommand "feature cold_engine_guard $requestedColdGuard")
            [void](& $sendCommand "feature dpf_regeneration_indicator $requestedDpfFeature")
            [void](& $sendCommand "feature transmission_overheat_alert $requestedTransmissionAlert")
            $interlock = if ($interlockInput.Checked) { 'on' } else { 'off' }
            [void](& $sendCommand "interlock $interlock")
            $hood = if (-not $hoodAvailableInput.Checked) {
                'unavailable'
            } elseif ($hoodClosedInput.Checked) {
                'closed'
            } else {
                'open'
            }
            $doors = if ($doorsClosedInput.Checked) { 'closed' } else { 'open' }
            $trunk = if ($trunkClosedInput.Checked) { 'closed' } else { 'open' }
            $brake = if ($brakeInput.Checked) { 'pressed' } else { 'released' }
            $parking = if ($parkingInput.Checked) { 'applied' } else { 'released' }
            $critical = if ($criticalInput.Checked) { 'on' } else { 'off' }
            $vehicleCommand =
                "vehicle rpm=$requestedRpm coolant=$requestedCoolant oil=$requestedOil transmission_temperature=$requestedTransmissionTemperature dpf=$requestedDpf gear=$($gearInput.SelectedItem) doors=$doors hood=$hood trunk=$trunk brake=$brake parking=$parking critical=$critical"
            [void](& $sendCommand $vehicleCommand)
        }.GetNewClosure())

        $sandboxForm.Add_FormClosed({
            if (-not $sandboxContext.Closed) {
                $sandboxContext.Closed = $true
                & $stopSandboxProcess $sandboxContext.Process
                $sandboxContext.Process = $null
            }
        }.GetNewClosure())

        try {
            [void](& $sendCommand 'status')
            if (-not [string]::IsNullOrWhiteSpace($PreviewFile)) {
                $previewFullPath = [System.IO.Path]::GetFullPath($PreviewFile)
                $previewDirectory = Split-Path -Parent $previewFullPath
                if (-not (Test-Path -LiteralPath $previewDirectory -PathType Container)) {
                    throw "Le dossier de prévisualisation n'existe pas : $previewDirectory"
                }
                $sandboxForm.Show()
                $sandboxForm.PerformLayout()
                $sandboxContent.PerformLayout()
                $sandboxForm.Refresh()
                [System.Windows.Forms.Application]::DoEvents()
                $bitmap = [System.Drawing.Bitmap]::new(
                    $sandboxForm.ClientSize.Width, $sandboxForm.ClientSize.Height)
                $sandboxForm.DrawToBitmap($bitmap, $sandboxForm.ClientRectangle)
                $bitmap.Save(
                    $previewFullPath,
                    [System.Drawing.Imaging.ImageFormat]::Png)
                $bitmap.Dispose()
                $sandboxForm.Close()
                Write-Output "sandbox_gui_preview: $previewFullPath"
            } else {
                [void]$sandboxForm.ShowDialog($form)
            }
        } finally {
            if (-not $sandboxContext.Closed) {
                $sandboxContext.Closed = $true
                & $stopSandboxProcess $sandboxContext.Process
            }
            $sandboxForm.Dispose()
        }
    }

    $scenarioList.Add_SelectedIndexChanged({
        if ($scenarioList.SelectedIndex -lt 0) {
            return
        }
        $selected = $scenarioDefinitions[$scenarioList.SelectedIndex]
        $description.Text = $selected.Description
        $scenarioCode.Text = "Code : $($selected.Code)"
        Set-RunStatus 'PRÊT — scénario sélectionné' 'Ready'
    })

    $runSelectedScenario = {
        if ($scenarioList.SelectedIndex -lt 0) {
            return
        }
        $selected = $scenarioDefinitions[$scenarioList.SelectedIndex]
        if ($selected.Code -eq 'user-config-example') {
            $examplePath = Join-Path $projectRoot 'config/user-settings.example.conf'
            Show-SimulatorResult @(
                '--scenario', 'user-config', '--config', $examplePath) `
                "configuration d'exemple validée"
            return
        }
        Show-SimulatorResult @('--scenario', $selected.Code) `
            "scénario $($selected.Code) validé"
    }
    $runButton.Add_Click($runSelectedScenario)
    $scenarioList.Add_DoubleClick($runSelectedScenario)

    $sandboxButton.Add_Click({
        Show-SandboxWindow
    })

    $configButton.Add_Click({
        $dialog = [System.Windows.Forms.OpenFileDialog]::new()
        $dialog.Title = 'Choisir une configuration utilisateur'
        $dialog.Filter = 'Configuration BMW Remote (*.conf)|*.conf|Tous les fichiers (*.*)|*.*'
        $dialog.InitialDirectory = Join-Path $projectRoot 'config'
        if ($dialog.ShowDialog($form) -eq 'OK') {
            Show-SimulatorResult @(
                '--scenario', 'user-config', '--config', $dialog.FileName) `
                'configuration utilisateur validée'
        }
        $dialog.Dispose()
    })

    $traceButton.Add_Click({
        $dialog = [System.Windows.Forms.OpenFileDialog]::new()
        $dialog.Title = 'Choisir une trace CAN canonique'
        $dialog.Filter = 'Trace CAN CSV (*.csv)|*.csv|Tous les fichiers (*.*)|*.*'
        $dialog.InitialDirectory = Join-Path $projectRoot 'scenarios'
        if ($dialog.ShowDialog($form) -eq 'OK') {
            $hoodMode = if ($hoodRequired.Checked) { 'required' } else { 'optional' }
            Show-SimulatorResult @(
                '--trace', $dialog.FileName, '--hood', $hoodMode) `
                'trace inspectée' $false
        }
        $dialog.Dispose()
    })

    $clearButton.Add_Click({
        $output.Clear()
        Set-RunStatus 'PRÊT — sortie effacée' 'Ready'
    })
    $copyButton.Add_Click({
        if (-not [string]::IsNullOrWhiteSpace($output.Text)) {
            [System.Windows.Forms.Clipboard]::SetText($output.Text)
            Set-RunStatus 'RAPPORT COPIÉ DANS LE PRESSE-PAPIERS' 'Ready'
        }
    })

    if (-not [string]::IsNullOrWhiteSpace($SandboxPreviewPath)) {
        Show-SandboxWindow $SandboxPreviewPath
        $form.Dispose()
    } elseif (-not [string]::IsNullOrWhiteSpace($PreviewPath)) {
        $previewFullPath = [System.IO.Path]::GetFullPath($PreviewPath)
        $previewDirectory = Split-Path -Parent $previewFullPath
        if (-not (Test-Path -LiteralPath $previewDirectory -PathType Container)) {
            throw "Le dossier de prévisualisation n'existe pas : $previewDirectory"
        }
        $form.Show()
        $form.PerformLayout()
        $mainLayout.PerformLayout()
        $form.Refresh()
        [System.Windows.Forms.Application]::DoEvents()
        $bitmap = [System.Drawing.Bitmap]::new(
            $form.ClientSize.Width, $form.ClientSize.Height)
        $form.DrawToBitmap($bitmap, $form.ClientRectangle)
        $bitmap.Save(
            $previewFullPath,
            [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
        $form.Close()
        Write-Output "gui_preview: $previewFullPath"
    } else {
        [void]$form.ShowDialog()
    }
} catch {
    if (-not $SelfTest) {
        try {
            Add-Type -AssemblyName System.Windows.Forms
            [void][System.Windows.Forms.MessageBox]::Show(
                ($_ | Out-String),
                'BMW Remote Simulator — erreur',
                [System.Windows.Forms.MessageBoxButtons]::OK,
                [System.Windows.Forms.MessageBoxIcon]::Error)
        } catch {
            Write-Error $_
        }
    }
    throw
}
