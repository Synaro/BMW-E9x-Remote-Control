$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$simulatorExecutable = Join-Path $buildDirectory 'bmw_remote_simulator.exe'

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
    (Join-Path $projectRoot 'src/application/controller.cpp') `
    (Join-Path $projectRoot 'src/application/lock_command_gate.cpp') `
    (Join-Path $projectRoot 'src/application/lock_sequence_detector.cpp') `
    (Join-Path $projectRoot 'src/application/profile_readiness.cpp') `
    (Join-Path $projectRoot 'src/application/safety_policy.cpp') `
    (Join-Path $projectRoot 'src/application/user_settings.cpp') `
    (Join-Path $projectRoot 'src/domain/reference_profiles.cpp') `
    (Join-Path $projectRoot 'src/domain/vehicle_profile.cpp') `
    (Join-Path $projectRoot 'src/domain/vehicle_signal.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/can_lock_command_adapter.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/can_trace_replay.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/diagnostic_journal.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/replay_vehicle_gateway.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/runtime.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/settings_identity.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/settings_payload.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/settings_protocol.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/settings_storage.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/settings_stream.cpp') `
    (Join-Path $projectRoot 'src/infrastructure/vehicle_state_assembler.cpp') `
    (Join-Path $projectRoot 'src/simulation/synthetic_can.cpp') `
    (Join-Path $projectRoot 'tools/can_trace_csv.cpp') `
    (Join-Path $projectRoot 'tools/user_settings_file.cpp') `
    (Join-Path $projectRoot 'tools/vehicle_simulator.cpp') `
    -o $simulatorExecutable

if ($LASTEXITCODE -ne 0) {
    throw "Simulator compilation failed with exit code $LASTEXITCODE"
}

Write-Host "Simulator built: $simulatorExecutable"
