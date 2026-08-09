set(BMW_REMOTE_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
file(STRINGS
    "${BMW_REMOTE_PROJECT_ROOT}/cmake/source-manifest.txt"
    BMW_REMOTE_SOURCE_MANIFEST)

set(BMW_REMOTE_FIRMWARE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
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
    if(group STREQUAL "core")
        list(APPEND
            BMW_REMOTE_FIRMWARE_SOURCES
            "${BMW_REMOTE_PROJECT_ROOT}/${source}")
    endif()
endforeach()

idf_component_register(
    SRCS ${BMW_REMOTE_FIRMWARE_SOURCES}
    INCLUDE_DIRS
        "${BMW_REMOTE_PROJECT_ROOT}/include"
        "${BMW_REMOTE_PROJECT_ROOT}/libs/can-core/include"
        "${BMW_REMOTE_PROJECT_ROOT}"
    REQUIRES
        esp_driver_gpio
        esp_driver_usb_serial_jtag
        esp_timer
        driver
        nvs_flash)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(
    ${COMPONENT_LIB}
    PRIVATE
        -Wall
        -Wextra
        -Wconversion)
