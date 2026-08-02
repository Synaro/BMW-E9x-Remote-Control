cmake_minimum_required(VERSION 3.16)

project(bmw_e9x_remote_control LANGUAGES CXX)

add_library(bmw_remote_core
    src/infrastructure/actuator_safety_supervisor.cpp
    src/application/controller.cpp
    src/application/lock_command_gate.cpp
    src/application/lock_sequence_detector.cpp
    src/application/profile_readiness.cpp
    src/application/safety_policy.cpp
    src/application/user_settings.cpp
    src/domain/reference_profiles.cpp
    src/domain/vehicle_profile.cpp
    src/domain/vehicle_signal.cpp
    src/infrastructure/can_lock_command_adapter.cpp
    src/infrastructure/can_trace_replay.cpp
    src/infrastructure/diagnostic_journal.cpp
    src/infrastructure/replay_vehicle_gateway.cpp
    src/infrastructure/runtime.cpp
    src/infrastructure/settings_identity.cpp
    src/infrastructure/settings_payload.cpp
    src/infrastructure/settings_protocol.cpp
    src/infrastructure/settings_storage.cpp
    src/infrastructure/settings_stream.cpp
    src/infrastructure/vehicle_state_assembler.cpp
)

target_include_directories(bmw_remote_core PUBLIC include)
target_compile_features(bmw_remote_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(bmw_remote_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(bmw_remote_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_library(bmw_remote_host_tools
    tools/can_trace_csv.cpp
    tools/serial_settings_channel.cpp
    tools/settings_device_client.cpp
    tools/user_settings_file.cpp
)
target_link_libraries(bmw_remote_host_tools PUBLIC bmw_remote_core)
target_include_directories(
    bmw_remote_host_tools PUBLIC include ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(bmw_remote_host_tools PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(bmw_remote_host_tools PRIVATE /W4 /permissive-)
else()
    target_compile_options(
        bmw_remote_host_tools PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_library(bmw_remote_simulation
    src/simulation/synthetic_can.cpp
)
target_link_libraries(bmw_remote_simulation PUBLIC bmw_remote_core)
target_include_directories(bmw_remote_simulation PUBLIC include)
target_compile_features(bmw_remote_simulation PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(bmw_remote_simulation PRIVATE /W4 /permissive-)
else()
    target_compile_options(
        bmw_remote_simulation PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

option(BMW_REMOTE_BUILD_SIMULATOR "Build the offline vehicle simulator" ON)
option(BMW_REMOTE_BUILD_CONFIGURATOR "Build the user settings configurator" ON)

if(BMW_REMOTE_BUILD_SIMULATOR)
    add_executable(
        bmw_remote_simulator
        tools/sandbox_session.cpp
        tools/vehicle_simulator.cpp)
    target_link_libraries(
        bmw_remote_simulator PRIVATE bmw_remote_simulation bmw_remote_host_tools)
endif()

if(BMW_REMOTE_BUILD_CONFIGURATOR)
    add_executable(
        bmw_remote_configurator tools/settings_configurator.cpp)
    target_link_libraries(
        bmw_remote_configurator PRIVATE bmw_remote_host_tools)
endif()

option(BMW_REMOTE_BUILD_TESTS "Build the host-side unit tests" ON)

if(BMW_REMOTE_BUILD_TESTS)
    enable_testing()
    add_executable(
        bmw_remote_tests
        tests/test_main.cpp
        tools/sandbox_session.cpp)
    target_link_libraries(
        bmw_remote_tests PRIVATE bmw_remote_simulation bmw_remote_host_tools)
    add_test(NAME bmw_remote_tests COMMAND bmw_remote_tests)

    if(BMW_REMOTE_BUILD_SIMULATOR)
        add_test(
            NAME bmw_remote_simulator_nominal
            COMMAND bmw_remote_simulator --scenario nominal)
        add_test(
            NAME bmw_remote_simulator_hood_required
            COMMAND bmw_remote_simulator --scenario hood-required)
        add_test(
            NAME bmw_remote_simulator_hood_optional
            COMMAND bmw_remote_simulator --scenario hood-optional)
        add_test(
            NAME bmw_remote_simulator_takeover_timeout
            COMMAND bmw_remote_simulator --scenario takeover-timeout)
        add_test(
            NAME bmw_remote_simulator_takeover_confirmed
            COMMAND bmw_remote_simulator --scenario takeover-confirmed)
        add_test(
            NAME bmw_remote_simulator_signal_loss
            COMMAND bmw_remote_simulator --scenario signal-loss)
        add_test(
            NAME bmw_remote_simulator_signal_delay
            COMMAND bmw_remote_simulator --scenario signal-delay)
        add_test(
            NAME bmw_remote_simulator_frame_corruption
            COMMAND bmw_remote_simulator --scenario frame-corruption)
        add_test(
            NAME bmw_remote_simulator_user_config
            COMMAND bmw_remote_simulator
                --scenario user-config
                --config ${CMAKE_CURRENT_SOURCE_DIR}/config/user-settings.example.conf)
        add_test(
            NAME bmw_remote_simulator_settings_recovery
            COMMAND bmw_remote_simulator --scenario settings-recovery)
        add_test(
            NAME bmw_remote_simulator_settings_link
            COMMAND bmw_remote_simulator --scenario settings-link)
        add_test(
            NAME bmw_remote_simulator_lock_replay_guard
            COMMAND bmw_remote_simulator --scenario lock-replay-guard)
        add_test(
            NAME bmw_remote_simulator_qualified_lock_adapter
            COMMAND bmw_remote_simulator --scenario qualified-lock-adapter)
        add_test(
            NAME bmw_remote_simulator_actuator_supervisor
            COMMAND bmw_remote_simulator --scenario actuator-supervisor)
        add_test(
            NAME bmw_remote_simulator_supervised_runtime
            COMMAND bmw_remote_simulator --scenario supervised-runtime)
    endif()

    if(BMW_REMOTE_BUILD_CONFIGURATOR)
        add_test(
            NAME bmw_remote_configurator_defaults
            COMMAND bmw_remote_configurator --print-defaults)
        add_test(
            NAME bmw_remote_configurator_example
            COMMAND bmw_remote_configurator
                --check
                --config ${CMAKE_CURRENT_SOURCE_DIR}/config/user-settings.example.conf)
        add_test(
            NAME bmw_remote_configurator_bench_safe
            COMMAND bmw_remote_configurator
                --check
                --config ${CMAKE_CURRENT_SOURCE_DIR}/config/bench-safe.example.conf)
    endif()
endif()
