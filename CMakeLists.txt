cmake_minimum_required(VERSION 3.16)

project(bmw_e9x_remote_control LANGUAGES CXX)

add_library(bmw_remote_core
    src/application/controller.cpp
    src/application/lock_sequence_detector.cpp
    src/application/profile_readiness.cpp
    src/application/safety_policy.cpp
    src/domain/reference_profiles.cpp
    src/domain/vehicle_profile.cpp
    src/domain/vehicle_signal.cpp
    src/infrastructure/can_trace_replay.cpp
    src/infrastructure/replay_vehicle_gateway.cpp
    src/infrastructure/runtime.cpp
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

if(BMW_REMOTE_BUILD_SIMULATOR)
    add_executable(bmw_remote_simulator tools/vehicle_simulator.cpp)
    target_link_libraries(
        bmw_remote_simulator PRIVATE bmw_remote_simulation bmw_remote_host_tools)
endif()

option(BMW_REMOTE_BUILD_TESTS "Build the host-side unit tests" ON)

if(BMW_REMOTE_BUILD_TESTS)
    enable_testing()
    add_executable(bmw_remote_tests tests/test_main.cpp)
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
    endif()
endif()
