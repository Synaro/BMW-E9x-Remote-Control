param(
    [switch]$SkipBuild,
    [switch]$SelfTest,
    [string]$PreviewPath
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
    $sidebarLayout.RowCount = 9
    $sidebarLayout.BackColor = $colors.Panel
    [void]$sidebarLayout.ColumnStyles.Add(
        [System.Windows.Forms.ColumnStyle]::new(
            [System.Windows.Forms.SizeType]::Percent, 100))
    foreach ($height in @(30, 260, 28, 75, 28, 46, 46, 46, 35)) {
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
    $configButton = New-FlatButton '⚙  Tester une configuration…'
    [void]$sidebarLayout.Controls.Add($configButton, 0, 6)
    $traceButton = New-FlatButton '⌁  Inspecter une trace CAN…'
    [void]$sidebarLayout.Controls.Add($traceButton, 0, 7)

    $hoodRequired = [System.Windows.Forms.CheckBox]::new()
    $hoodRequired.Text = "Capot requis pour l'inspection de trace"
    $hoodRequired.Checked = $true
    $hoodRequired.ForeColor = $colors.Muted
    $hoodRequired.Dock = 'Fill'
    [void]$sidebarLayout.Controls.Add($hoodRequired, 0, 8)
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

    if (-not [string]::IsNullOrWhiteSpace($PreviewPath)) {
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
