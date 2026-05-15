# =============================================================================
# CToon MATLAB MEX Binding
# =============================================================================
#
# Responsibilities:
#   - Compile ctoon_mex via MATLAB Build Tool  (buildtool build)
#   - Install MEX binary + .m wrappers
#   - Provide ctoon_matlab_export target
#
# buildfile.m lives in the repository root (consistent with CMakeLists.txt,
# pyproject.toml, ctoon.go) and is picked up by matlab-actions/run-build.
#
# Test and coverage targets live in tests/matlab/CMakeLists.txt.
# =============================================================================

cmake_minimum_required(VERSION 3.19)

find_package(Matlab COMPONENTS MX_LIBRARY MEX_COMPILER MAIN_PROGRAM QUIET)

if(NOT Matlab_FOUND)
    message(WARNING
        "MATLAB not found — skipping MEX binding.\n"
        "Set Matlab_ROOT_DIR or the MATLAB_DIR environment variable.")
    return()
endif()

message(STATUS "MATLAB found: ${Matlab_ROOT_DIR}  (${Matlab_VERSION_STRING})")

# ---------------------------------------------------------------------------
# Coverage directory
# ---------------------------------------------------------------------------
set(CTOON_COVERAGE_MATLAB_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/matlab
    CACHE PATH "MATLAB MEX coverage output directory")

# ---------------------------------------------------------------------------
# MEX target — compiled by buildtool via root buildfile.m
# ---------------------------------------------------------------------------
add_custom_target(ctoon_mex ALL
    COMMAND ${CMAKE_COMMAND} -E echo "-- MATLAB buildtool build..."
    COMMAND ${Matlab_MAIN_PROGRAM}
            -sd "${PROJECT_SOURCE_DIR}"
            -batch
            "global BUILD_DIR COVERAGE_OUTPUT_DIR; \
             BUILD_DIR='${CMAKE_CURRENT_BINARY_DIR}'; \
             COVERAGE_OUTPUT_DIR='${CTOON_COVERAGE_MATLAB_BINARY_DIR}'; \
             buildtool build"
    BYPRODUCTS
        ${CMAKE_CURRENT_BINARY_DIR}/ctoon_mex.${Matlab_MEX_EXTENSION}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Compiling ctoon_mex via MATLAB Build Tool"
    USES_TERMINAL
)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
file(GLOB CTOON_MATLAB_WRAPPERS "${CMAKE_CURRENT_SOURCE_DIR}/*.m")

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ctoon_mex.${Matlab_MEX_EXTENSION}
    DESTINATION matlab
)
install(FILES ${CTOON_MATLAB_WRAPPERS} DESTINATION matlab)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_mex.c DESTINATION matlab/src)

# ---------------------------------------------------------------------------
# Export target
# ---------------------------------------------------------------------------
add_custom_target(ctoon_matlab_export
    COMMAND ${CMAKE_COMMAND}
            -DEXPORT_DIR=${CMAKE_BINARY_DIR}/matlab
            -DMEX_SOURCES_PATH=${PROJECT_SOURCE_DIR}/src
            -DMEX_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/include
            -P ${CMAKE_CURRENT_SOURCE_DIR}/CToonMatlabExport.cmake
    COMMENT "Exporting self-contained MATLAB binding → ${CMAKE_BINARY_DIR}/matlab"
    USES_TERMINAL
)