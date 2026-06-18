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
    list(APPEND APP_INCLUDES
        ${CMAKE_SOURCE_DIR}/vendor/p-net/include
        ${CMAKE_SOURCE_DIR}/vendor/p-net/src
    )
    # In production, p-net is compiled as a static library
    # For now we just reference headers and link manually
    set(PNET_LINK_LIBS "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/p-net/build/libpnet.a")
        set(PNET_LINK_LIBS "${CMAKE_SOURCE_DIR}/vendor/p-net/build/libpnet.a")
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

    add_executable(pnet-tests ${TEST_SOURCES} ${APP_SOURCES})
    target_include_directories(pnet-tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests/mock
    )
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
