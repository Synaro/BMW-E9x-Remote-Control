$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$testExecutable = Join-Path $buildDirectory 'bmw_remote_tests.exe'

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
    (Join-Path $projectRoot 'src/infrastructure/actuator_safety_supervisor.cpp') `
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
    (Join-Path $projectRoot 'tools/sandbox_session.cpp') `
    (Join-Path $projectRoot 'tools/settings_device_client.cpp') `
    (Join-Path $projectRoot 'tools/serial_settings_channel.cpp') `
    (Join-Path $projectRoot 'tools/user_settings_file.cpp') `
    (Join-Path $projectRoot 'tests/test_main.cpp') `
    -o $testExecutable

if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed with exit code $LASTEXITCODE"
}

& $testExecutable

if ($LASTEXITCODE -ne 0) {
    throw "Tests failed with exit code $LASTEXITCODE"
}
