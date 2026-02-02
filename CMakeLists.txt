# libe3 - Vendor-Neutral E3AP C++ Library
#
# SPDX-License-Identifier: Apache-2.0
#
# Modern CMake (3.16+) configuration for building libe3

cmake_minimum_required(VERSION 3.16)

project(libe3
    VERSION 0.1.0
    DESCRIPTION "Vendor-neutral E3AP C++ library for RAN integration"
    LANGUAGES CXX
)

# ============================================================================
# Build Options
# ============================================================================
option(LIBE3_BUILD_TESTS "Build unit tests" ON)
option(LIBE3_BUILD_EXAMPLES "Build examples" ON)
option(LIBE3_ENABLE_ZMQ "Enable ZeroMQ transport" ON)
option(LIBE3_ENABLE_ASN1 "Enable ASN.1 encoding (requires asn1c)" OFF)
option(LIBE3_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LIBE3_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# ============================================================================
# Compiler Settings
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Position-independent code (required for shared libraries)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Compiler Warnings
# ============================================================================
add_library(libe3_warnings INTERFACE)
target_compile_options(libe3_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wold-style-cast
        -Wcast-qual
        -Wformat=2
        -Werror=return-type
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /WX
        /permissive-
    >
)

# ============================================================================
# Sanitizers
# ============================================================================
add_library(libe3_sanitizers INTERFACE)

if(LIBE3_ENABLE_ASAN)
    target_compile_options(libe3_sanitizers INTERFACE
        -fsanitize=address
        -fno-omit-frame-pointer
    )
    target_link_options(libe3_sanitizers INTERFACE
        -fsanitize=address
    )
endif()

if(LIBE3_ENABLE_TSAN)
    target_compile_options(libe3_sanitizers INTERFACE
        -fsanitize=thread
        -fno-omit-frame-pointer
    )
    target_link_options(libe3_sanitizers INTERFACE
        -fsanitize=thread
    )
endif()

# ============================================================================
# Dependencies
# ============================================================================

# Required: pthreads
find_package(Threads REQUIRED)

# Optional: ZeroMQ
if(LIBE3_ENABLE_ZMQ)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ZMQ IMPORTED_TARGET libzmq)
    endif()
    
    if(NOT ZMQ_FOUND)
        find_library(ZMQ_LIBRARIES NAMES zmq)
        find_path(ZMQ_INCLUDE_DIRS NAMES zmq.h)
        if(ZMQ_LIBRARIES AND ZMQ_INCLUDE_DIRS)
            set(ZMQ_FOUND TRUE)
            add_library(PkgConfig::ZMQ INTERFACE IMPORTED)
            target_link_libraries(PkgConfig::ZMQ INTERFACE ${ZMQ_LIBRARIES})
            target_include_directories(PkgConfig::ZMQ INTERFACE ${ZMQ_INCLUDE_DIRS})
        endif()
    endif()
    
    if(ZMQ_FOUND)
        message(STATUS "ZeroMQ found: ${ZMQ_LIBRARIES}")
    else()
        message(WARNING "ZeroMQ not found, ZMQ transport disabled")
        set(LIBE3_ENABLE_ZMQ OFF)
    endif()
endif()

# ============================================================================
# Source Files
# ============================================================================

set(LIBE3_PUBLIC_HEADERS
    include/libe3/types.hpp
    include/libe3/logger.hpp
    include/libe3/e3_connector.hpp
    include/libe3/e3_encoder.hpp
    include/libe3/subscription_manager.hpp
    include/libe3/response_queue.hpp
    include/libe3/sm_interface.hpp
    include/libe3/e3_interface.hpp
    include/libe3/e3_agent.hpp
    include/libe3/libe3.hpp
)

set(LIBE3_SOURCES
    # Core
    src/core/e3_agent.cpp
    src/core/e3_interface.cpp
    src/core/subscription_manager.cpp
    src/core/response_queue.cpp
    src/core/sm_registry.cpp
    
    # Encoder
    src/encoder/e3_encoder.cpp
    src/encoder/json_encoder.cpp
    src/encoder/encoder_factory.cpp
    
    # Connector
    src/connector/connector_factory.cpp
)

# Conditionally add ZMQ connector
if(LIBE3_ENABLE_ZMQ)
    list(APPEND LIBE3_SOURCES src/connector/zmq_connector.cpp)
endif()

# Always add POSIX connector (fallback)
list(APPEND LIBE3_SOURCES src/connector/posix_connector.cpp)

# ============================================================================
# Library Target
# ============================================================================

add_library(libe3 STATIC ${LIBE3_SOURCES})
add_library(libe3::libe3 ALIAS libe3)

target_include_directories(libe3
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(libe3
    PUBLIC
        Threads::Threads
    PRIVATE
        libe3_warnings
        libe3_sanitizers
)

if(LIBE3_ENABLE_ZMQ)
    target_compile_definitions(libe3 PUBLIC LIBE3_HAS_ZMQ=1)
    target_link_libraries(libe3 PRIVATE PkgConfig::ZMQ)
else()
    target_compile_definitions(libe3 PUBLIC LIBE3_HAS_ZMQ=0)
endif()

# Set library version
set_target_properties(libe3 PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${LIBE3_PUBLIC_HEADERS}"
)

# ============================================================================
# Shared Library (optional)
# ============================================================================

add_library(libe3_shared SHARED ${LIBE3_SOURCES})
add_library(libe3::shared ALIAS libe3_shared)

target_include_directories(libe3_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(libe3_shared
    PUBLIC
        Threads::Threads
    PRIVATE
        libe3_warnings
        libe3_sanitizers
)

if(LIBE3_ENABLE_ZMQ)
    target_compile_definitions(libe3_shared PUBLIC LIBE3_HAS_ZMQ=1)
    target_link_libraries(libe3_shared PRIVATE PkgConfig::ZMQ)
else()
    target_compile_definitions(libe3_shared PUBLIC LIBE3_HAS_ZMQ=0)
endif()

set_target_properties(libe3_shared PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME libe3
)

# ============================================================================
# Unit Tests
# ============================================================================

if(LIBE3_BUILD_TESTS)
    enable_testing()
    
    # Simple test framework header
    add_library(libe3_test_framework INTERFACE)
    target_include_directories(libe3_test_framework INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    
    # Test: Types and Encoding
    add_executable(test_types tests/test_types.cpp)
    target_link_libraries(test_types
        PRIVATE
            libe3::libe3
            libe3_test_framework
            libe3_warnings
            libe3_sanitizers
    )
    add_test(NAME test_types COMMAND test_types)
    
    # Test: Subscription Manager
    add_executable(test_subscription_manager tests/test_subscription_manager.cpp)
    target_link_libraries(test_subscription_manager
        PRIVATE
            libe3::libe3
            libe3_test_framework
            libe3_warnings
            libe3_sanitizers
    )
    add_test(NAME test_subscription_manager COMMAND test_subscription_manager)
    
    # Test: Response Queue
    add_executable(test_response_queue tests/test_response_queue.cpp)
    target_link_libraries(test_response_queue
        PRIVATE
            libe3::libe3
            libe3_test_framework
            libe3_warnings
            libe3_sanitizers
    )
    add_test(NAME test_response_queue COMMAND test_response_queue)
    
    # Test: JSON Encoder
    add_executable(test_json_encoder tests/test_json_encoder.cpp)
    target_link_libraries(test_json_encoder
        PRIVATE
            libe3::libe3
            libe3_test_framework
            libe3_warnings
            libe3_sanitizers
    )
    add_test(NAME test_json_encoder COMMAND test_json_encoder)
    
    # Test: E3Agent Integration
    add_executable(test_e3_agent tests/test_e3_agent.cpp)
    target_link_libraries(test_e3_agent
        PRIVATE
            libe3::libe3
            libe3_test_framework
            libe3_warnings
            libe3_sanitizers
    )
    add_test(NAME test_e3_agent COMMAND test_e3_agent)
endif()

# ============================================================================
# Examples
# ============================================================================

if(LIBE3_BUILD_EXAMPLES)
    # Example: Simple E3 Agent
    add_executable(example_simple_agent examples/simple_agent.cpp)
    target_link_libraries(example_simple_agent PRIVATE libe3::libe3)
    
    # Example: Custom Service Model
    add_executable(example_custom_sm examples/custom_service_model.cpp)
    target_link_libraries(example_custom_sm PRIVATE libe3::libe3)
    
    # Example: Simulation Mode
    add_executable(example_simulation examples/simulation_mode.cpp)
    target_link_libraries(example_simulation PRIVATE libe3::libe3)
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library
install(TARGETS libe3 libe3_shared libe3_warnings libe3_sanitizers
    EXPORT libe3Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libe3
)

# Install headers
install(DIRECTORY include/libe3
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# Export targets
install(EXPORT libe3Targets
    FILE libe3Targets.cmake
    NAMESPACE libe3::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libe3
)

# Config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/libe3Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/libe3Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libe3
)

# Version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/libe3ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libe3Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/libe3ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libe3
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "libe3 Configuration Summary")
message(STATUS "===========================")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Build Tests:      ${LIBE3_BUILD_TESTS}")
message(STATUS "  Build Examples:   ${LIBE3_BUILD_EXAMPLES}")
message(STATUS "  ZeroMQ Support:   ${LIBE3_ENABLE_ZMQ}")
message(STATUS "  ASN.1 Support:    ${LIBE3_ENABLE_ASN1}")
message(STATUS "  AddressSanitizer: ${LIBE3_ENABLE_ASAN}")
message(STATUS "  ThreadSanitizer:  ${LIBE3_ENABLE_TSAN}")
message(STATUS "")
