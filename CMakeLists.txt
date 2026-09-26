cmake_minimum_required(VERSION 3.19)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(DynamicVersion)
include(SpecVersion)

extract_version_string(
    HEADER_FILE ${CMAKE_CURRENT_LIST_DIR}/include/ctoon.h
    VERSION_PREFIX CTOON_
    OUTPUT_VAR PROJECT_VERSION
)

project(CToon LANGUAGES CXX C VERSION ${PROJECT_VERSION})
message(STATUS "Project: ${PROJECT_NAME}@v${PROJECT_VERSION}")
include(Startup)

# Single source of truth for which toon-format/spec version this project
# builds against - cached in supported_spec.conf (see cmake/SpecVersion.cmake
# for how/when it's refreshed vs read from cache).
resolve_spec_version(
    TAG_VAR     CTOON_SPEC_TAG
    VERSION_VAR CTOON_SPEC_VERSION
    DATE_VAR    CTOON_SPEC_DATE
)
if(CTOON_SPEC_TAG)
    message(STATUS "TOON spec: ${CTOON_SPEC_TAG} (spec v${CTOON_SPEC_VERSION} ${CTOON_SPEC_DATE})")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include optimization flags
include(Optimize)

# Project options
option(CTOON_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(CTOON_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(CTOON_BUILD_PYTHON "Build Python" ${PROJECT_IS_TOP_LEVEL})
option(CTOON_BUILD_DOCS "Build documentation" OFF)

# Self-building language bindings (Go, Rust, Zig, MATLAB, Julia): each has its
# own external toolchain and is never compiled by this project's own
# CMake graph the way Python is — these flags don't turn on a build step
# here, they tell tests/, docs/, and src/bindings/matlab (each of which
# detects these toolchains independently) that a missing toolchain
# should be a hard error instead of a silent skip. See
# cmake/CtoonLangGuard.cmake. This is the main ctoon project only —
# benchmarks/ is a fully standalone CMake project and always
# warn-and-skips regardless, since its whole point is running whatever
# it can find.
option(CTOON_BUILD_ALL_LANGS "Require every language binding's toolchain (error, don't skip, if any is missing)" OFF)
option(CTOON_BUILD_GO "Require the Go toolchain" OFF)
option(CTOON_BUILD_RUST "Require the Rust toolchain" OFF)
option(CTOON_BUILD_ZIG "Require the Zig toolchain" OFF)
option(CTOON_BUILD_MATLAB "Require the MATLAB toolchain" OFF)
option(CTOON_BUILD_JULIA "Require the Julia toolchain" OFF)

include(CtoonLangGuard)


set(CTOON_SOURCE_DIR ${PROJECT_SOURCE_DIR})

# Create library
file(GLOB CTOON_SOURCES_C ${PROJECT_SOURCE_DIR}/src/*.c)

add_library(ctoon ${CTOON_SOURCES_C})
add_library(ctoon::ctoon ALIAS ctoon)

set_target_properties(ctoon PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Include directories
target_include_directories(ctoon 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/sources
)


# C++ wrapper library (header-only interface)
add_library(ctoonpp INTERFACE)
add_library(ctoon::ctoonpp ALIAS ctoonpp)

target_include_directories(ctoonpp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/cpp
)

target_link_libraries(ctoonpp INTERFACE ctoon::ctoon)

target_compile_definitions(ctoonpp INTERFACE
    CTOON_VERSION_NUMBER=${PROJECT_VERSION}
)

# Compiler options
target_compile_options(ctoon PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# Apply optimization flags
apply_optimization_flags(ctoon)

# Add CLI executable — no longer depends on yyjson; uses ctoon JSON API directly
add_executable(ctoon_cli
    ${PROJECT_SOURCE_DIR}/src/cli/cli.cpp
)
target_include_directories(ctoon_cli PRIVATE
    ${PROJECT_SOURCE_DIR}/src/bindings/cpp
    ${PROJECT_SOURCE_DIR}/src/cli
)
target_link_libraries(ctoon_cli PRIVATE ctoon::ctoon)
set_target_properties(ctoon_cli PROPERTIES OUTPUT_NAME ctoon)

# Apply optimization flags to CLI
apply_optimization_flags(ctoon_cli)

if(CTOON_BUILD_PYTHON OR DEFINED SKBUILD)
    add_subdirectory(${PROJECT_SOURCE_DIR}/src/bindings/python)
    if(DEFINED SKBUILD)
        return()
    endif()
endif()

# Installation
install(TARGETS ctoon_cli
    RUNTIME DESTINATION bin
)

add_subdirectory(${PROJECT_SOURCE_DIR}/src/bindings/matlab)


# Tests
if(CTOON_BUILD_TESTS)
    enable_testing()
    set(CMAKE_CTEST_ARGUMENTS "--output-on-failure" "-V")
    add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
endif()

# Installation
install(DIRECTORY include DESTINATION .)

# Print optimization summary
# print_optimization_summary()

# Documentation (Doxygen + Sphinx)
if(CTOON_BUILD_DOCS)
    add_subdirectory(${PROJECT_SOURCE_DIR}/docs)
endif()