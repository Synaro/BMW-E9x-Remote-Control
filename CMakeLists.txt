cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{IDF_PATH})
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src")
    # Build only the firmware component, the component that owns the required
    # erratum Kconfig option, and their declared dependencies. Host tools and
    # simulators stay out of the embedded image.
    # PlatformIO's generated __pio_env component carries the generic compiler
    # environment and must remain in the selected component set.
    # Keep the legacy driver component because the Phase 3/3H adapter uses the
    # stable ESP-IDF TWAI driver and still requires the listen-only erratum gate.
    set(COMPONENTS __pio_env src driver)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(bmw_e9x_remote_control_firmware)
    return()
endif()

project(bmw_e9x_remote_control LANGUAGES CXX)

file(STRINGS
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/source-manifest.txt"
    BMW_REMOTE_SOURCE_MANIFEST)
foreach(entry IN LISTS BMW_REMOTE_SOURCE_MANIFEST)
    if(entry MATCHES "^[ \\t]*#" OR entry STREQUAL "")
        continue()
    endif()
    string(REPLACE "|" ";" fields "${entry}")
    list(LENGTH fields field_count)
    if(NOT field_count EQUAL 2)
        message(FATAL_ERROR "Invalid source manifest entry: ${entry}")
    endif()
    list(GET fields 0 group)
    list(GET fields 1 source)
    string(TOUPPER "${group}" group)
    list(APPEND BMW_REMOTE_${group}_SOURCES "${source}")
endforeach()

add_library(bmw_remote_core ${BMW_REMOTE_CORE_SOURCES})

target_include_directories(
    bmw_remote_core PUBLIC include libs/can-core/include)
target_compile_features(bmw_remote_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(bmw_remote_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(bmw_remote_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_library(bmw_remote_host_tools ${BMW_REMOTE_HOST_SOURCES})
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

add_library(bmw_remote_simulation ${BMW_REMOTE_SIMULATION_SOURCES})
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
        ${BMW_REMOTE_SANDBOX_SOURCES}
        ${BMW_REMOTE_SIMULATOR_SOURCES})
    target_link_libraries(
        bmw_remote_simulator PRIVATE bmw_remote_simulation bmw_remote_host_tools)
endif()

if(BMW_REMOTE_BUILD_CONFIGURATOR)
    add_executable(
        bmw_remote_configurator ${BMW_REMOTE_CONFIGURATOR_SOURCES})
    target_link_libraries(
        bmw_remote_configurator PRIVATE bmw_remote_host_tools)
endif()

option(BMW_REMOTE_BUILD_TESTS "Build the host-side unit tests" ON)

if(BMW_REMOTE_BUILD_TESTS)
    enable_testing()
    add_executable(
        bmw_remote_tests
        ${BMW_REMOTE_TESTS_SOURCES}
        ${BMW_REMOTE_SANDBOX_SOURCES})
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
