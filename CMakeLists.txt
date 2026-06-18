cmake_minimum_required(VERSION 3.14)
project(pnet-manager C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build options
option(USE_REAL_PNET "Use real p-net from vendor/ (requires Linux)" OFF)
option(BUILD_TESTS "Build test suite" ON)

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
    # ASan disabled on macOS due to compatibility issues
    if(NOT APPLE)
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
endif()

# === Platform detection ===
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(PLATFORM_LINUX)
    set(PLATFORM_LINUX TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_definitions(PLATFORM_MACOS)
endif()

# === Source files ===
set(APP_SOURCES
    src/app/io_device.c
    src/net/network_config.c
    src/security/security_policy.c
    src/realtime/rt_setup.c
    src/config/app_config.c
)

# === Include directories ===
set(APP_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# === p-net integration ===
if(USE_REAL_PNET AND PLATFORM_LINUX)
    message(STATUS "Using real p-net stack from vendor/p-net")
    add_compile_definitions(USE_REAL_PNET)

    # Generate headers from templates (evaluation submodule ships .in files)
    set(PNET_EXPORT_H "${CMAKE_CURRENT_BINARY_DIR}/pnet_export.h")
    file(WRITE "${PNET_EXPORT_H}"
        "#ifndef PNET_EXPORT_H\n"
        "#define PNET_EXPORT_H\n"
        "\n"
        "#define PNET_EXPORT\n"
        "#define PNET_NO_EXPORT\n"
        "#define PNET_DEPRECATED\n"
        "#define PNET_DEPRECATED_EXPORT\n"
        "#define PNET_DEPRECATED_NO_EXPORT\n"
        "#define PNET_DECL_EXPORT\n"
        "\n"
        "#endif /* PNET_EXPORT_H */\n"
    )

    # pnet_options.h defaults
    set(PNET_OPTION_FAST_STARTUP OFF)
    set(PNET_OPTION_PARAMETER_SERVER OFF)
    set(PNET_OPTION_IR OFF)
    set(PNET_OPTION_SR OFF)
    set(PNET_OPTION_REDUNDANCY OFF)
    set(PNET_OPTION_AR_VENDOR_BLOCKS OFF)
    set(PNET_OPTION_RS OFF)
    set(PNET_OPTION_MC_CR OFF)
    set(PNET_OPTION_SRL OFF)
    set(PNET_OPTION_SNMP OFF)
    set(PNET_OPTION_DRIVER_ENABLE OFF)
    set(PNET_USE_ATOMICS ON)
    set(PNET_MAX_AR 2)
    set(PNET_MAX_API 1)
    set(PNET_MAX_CR 2)
    set(PNET_MAX_SLOTS 4)
    set(PNET_MAX_SUBSLOTS 8)
    set(PNET_MAX_DFP_IOCR 0)
    set(PNET_MAX_PHYSICAL_PORTS 1)
    set(PNET_MAX_LOG_BOOK_ENTRIES 10)
    set(PNET_MAX_ALARMS 4)
    set(PNET_MAX_ALARM_PAYLOAD_DATA_SIZE 200)
    set(PNET_MAX_DIAG_ITEMS 100)
    set(PNET_MAX_DIAG_MANUF_DATA_SIZE 200)
    set(PNET_MAX_MC_CR 1)
    set(PNET_MAX_AR_VENDOR_BLOCKS 1)
    set(PNET_MAX_AR_VENDOR_BLOCK_DATA_LENGTH 100)
    set(PNET_MAX_MAN_SPECIFIC_FAST_STARTUP_DATA_LENGTH 512)
    set(PNET_MAX_SESSION_BUFFER_SIZE 4096)
    set(PNET_MAX_DIRECTORYPATH_SIZE 128)
    set(PNET_MAX_FILENAME_SIZE 64)
    set(PNET_MAX_PORT_DESCRIPTION_SIZE 64)
    set(LOG_LEVEL WARNING)
    set(PF_ETH_LOG DEFAULT)
    set(PF_LLDP_LOG DEFAULT)
    set(PF_SNMP_LOG DEFAULT)
    set(PF_CPM_LOG DEFAULT)
    set(PF_PPM_LOG DEFAULT)
    set(PF_DCP_LOG DEFAULT)
    set(PF_RPC_LOG DEFAULT)
    set(PF_ALARM_LOG DEFAULT)
    set(PF_AL_BUF_LOG DEFAULT)
    set(PF_PNAL_LOG DEFAULT)
    set(PNET_LOG DEFAULT)
    configure_file(
        "${CMAKE_SOURCE_DIR}/vendor/p-net/pnet_options.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/pnet_options.h"
    )

    # pnet_version.h defaults
    set(PNET_VERSION_MAJOR 1)
    set(PNET_VERSION_MINOR 0)
    set(PNET_VERSION_PATCH 2)
    configure_file(
        "${CMAKE_SOURCE_DIR}/vendor/p-net/pnet_version.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/pnet_version.h"
    )

    list(APPEND APP_INCLUDES
        ${CMAKE_SOURCE_DIR}/vendor/p-net/include
        ${CMAKE_SOURCE_DIR}/vendor/p-net/src
        ${CMAKE_SOURCE_DIR}/include           # pnal_config.h lives here
        ${CMAKE_CURRENT_BINARY_DIR}           # generated pnet_options.h etc.
    )

    # Link against pre-built p-net static library
    # Build it first with: ./scripts/build_pnet.sh
    set(PNET_BUILD_DIR "${CMAKE_SOURCE_DIR}/vendor/p-net/build")
    set(PNET_LINK_LIBS "")

    # Look for p-net library in common locations
    find_library(PNET_LIB
        NAMES pnet
        PATHS
            "${PNET_BUILD_DIR}"
            "${PNET_BUILD_DIR}/src"
            "/usr/local/lib"
        NO_DEFAULT_PATH
    )

    if(PNET_LIB)
        message(STATUS "Found p-net library: ${PNET_LIB}")
        set(PNET_LINK_LIBS ${PNET_LIB})
    else()
        message(FATAL_ERROR
            "p-net library not found.\n"
            "  Build it first with:  ./scripts/build_pnet.sh\n"
            "  Or use the mock API:  cmake -DUSE_REAL_PNET=OFF"
        )
    endif()
else()
    message(STATUS "Using mock p-net API (testing mode)")
    list(APPEND APP_INCLUDES
        ${CMAKE_SOURCE_DIR}/tests/mock
    )
    list(APPEND APP_SOURCES
        ${CMAKE_SOURCE_DIR}/tests/mock/pnet_api_mock.c
    )
    set(PNET_LINK_LIBS "")
endif()

# === Main application library (reusable) ===
add_library(pnet_manager_lib STATIC ${APP_SOURCES})
target_include_directories(pnet_manager_lib PUBLIC ${APP_INCLUDES})
target_link_libraries(pnet_manager_lib ${PNET_LINK_LIBS} m)

# === Main executable ===
add_executable(pnet-manager src/main.c)
target_link_libraries(pnet-manager pnet_manager_lib)

# === Tests ===
if(BUILD_TESTS)
    enable_testing()

    set(TEST_SOURCES
        tests/test_main.c
        tests/test_io_device.c
        tests/test_network.c
        tests/test_security.c
        tests/test_realtime.c
        tests/test_config.c
        tests/mock/pnet_api_mock.c
    )

    set(TEST_INCLUDES
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests/mock
    )

    if(USE_REAL_PNET AND PLATFORM_LINUX)
        list(APPEND TEST_INCLUDES
            ${CMAKE_SOURCE_DIR}/vendor/p-net/include
            ${CMAKE_SOURCE_DIR}/vendor/p-net/src
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}
        )
    endif()

    add_executable(pnet-tests ${TEST_SOURCES} ${APP_SOURCES})
    target_include_directories(pnet-tests PRIVATE ${TEST_INCLUDES})
    if(USE_REAL_PNET AND PLATFORM_LINUX)
        target_link_libraries(pnet-tests pthread)
    endif()
    target_link_libraries(pnet-tests m)
    target_compile_definitions(pnet-tests PRIVATE TESTING)

    add_test(NAME pnet_unit_tests COMMAND pnet-tests)
endif()

# === Install ===
install(TARGETS pnet-manager DESTINATION bin)
install(FILES deploy/pnet-manager.service DESTINATION lib/systemd/system)
install(FILES deploy/pnet-manager.conf DESTINATION etc/pnet-manager)

# === Info ===
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Tests: ${BUILD_TESTS}")
