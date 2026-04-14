cmake_minimum_required(VERSION 3.19)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(DynamicVersion)

extract_version_string(
    HEADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/includes/ctoon.h"
    VERSION_PREFIX "CTOON_"
    OUTPUT_VAR PROJECT_VERSION
)

project(CToon LANGUAGES CXX C VERSION ${PROJECT_VERSION})
message(STATUS "Project: ${PROJECT_NAME}@v${PROJECT_VERSION}")
include(Startup)

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


set(CTOON_SOURCE_DIR ${PROJECT_SOURCE_DIR})

# Create library
file(GLOB CTOON_SOURCES_C ${PROJECT_SOURCE_DIR}/src/sources/*.c)
file(GLOB CTOON_SOURCES_CPP ${PROJECT_SOURCE_DIR}/src/sources/*.cpp)
set(CTOON_SOURCES ${CTOON_SOURCES_CPP} ${CTOON_SOURCES_C})

add_library(ctoon ${CTOON_SOURCES})
add_library(ctoon::ctoon ALIAS ctoon)

# Include directories
target_include_directories(ctoon 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/includes
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/sources
)

target_compile_definitions(ctoon PUBLIC
    # CTOON_VERSION is defined in header, use CTOON_VERSION_NUMBER for CMake
    CTOON_VERSION_NUMBER=${PROJECT_VERSION}
)

# Compiler options
target_compile_options(ctoon PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# Apply optimization flags
apply_optimization_flags(ctoon)

# Add CLI executable
add_executable(ctoon_cli src/sources/cli.cpp)
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

# Tests
if(CTOON_BUILD_TESTS)
    enable_testing()
    set(CMAKE_CTEST_ARGUMENTS "--output-on-failure" "-V")
    add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
endif()

# Installation
install(DIRECTORY includes DESTINATION include)

# Print optimization summary
print_optimization_summary()
