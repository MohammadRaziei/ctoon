# =============================================================================
# CToon MATLAB MEX Binding
# =============================================================================
#
# Responsibilities:
#   - Compile ctoon_mex via MATLAB Build Tool (buildtool mex)
#   - Install MEX binary + .m wrappers
#   - Provide ctoon_matlab_export target (cmake/CToonMatlabExport.cmake)
#
# Test and coverage targets live in tests/matlab/CMakeLists.txt.
# =============================================================================

cmake_minimum_required(VERSION 3.19)

find_package(Matlab COMPONENTS MX_LIBRARY MEX_COMPILER QUIET)

if(NOT Matlab_FOUND)
    message(WARNING
        "MATLAB not found — skipping MEX binding.\n"
        "Set Matlab_ROOT_DIR or the MATLAB_DIR environment variable.")
    return()
endif()

message(STATUS "MATLAB found: ${Matlab_ROOT_DIR}  (${Matlab_VERSION_STRING})")

# ---------------------------------------------------------------------------
# Coverage directory (mirrors pattern for other language bindings)
# ---------------------------------------------------------------------------
set(CTOON_COVERAGE_MATLAB_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/matlab
    CACHE PATH "MATLAB MEX coverage output directory")

# ---------------------------------------------------------------------------
# MEX target — compiled by MATLAB Build Tool via buildtool
# ---------------------------------------------------------------------------
# We declare a custom target (not matlab_add_mex) because the compiler is
# driven by buildfile.m / ctoon_build.m, not by CMake's mex invocation.
# This lets CI use the exact same build path as interactive MATLAB users.

set(_binding_src ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(ctoon_mex ALL
    COMMAND ${CMAKE_COMMAND} -E echo "-- MATLAB buildtool mex..."
    COMMAND ${Matlab_MAIN_PROGRAM}
            -sd "${_binding_src}"
            -batch
            "global BUILD_DIR MEX_SOURCES_PATH MEX_INCLUDE_DIR TEST_DIR; \
             BUILD_DIR='${CMAKE_CURRENT_BINARY_DIR}'; \
             MEX_SOURCES_PATH='${PROJECT_SOURCE_DIR}/src'; \
             MEX_INCLUDE_DIR='${PROJECT_SOURCE_DIR}/include'; \
             TEST_DIR='${PROJECT_SOURCE_DIR}/tests/matlab'; \
             buildtool mex"
    BYPRODUCTS
        ${CMAKE_CURRENT_BINARY_DIR}/ctoon_mex.${Matlab_MEX_EXTENSION}
    WORKING_DIRECTORY ${_binding_src}
    COMMENT "Compiling ctoon_mex via MATLAB Build Tool"
    USES_TERMINAL
)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
set(CTOON_MATLAB_WRAPPERS
    ${_binding_src}/ctoon_encode.m
    ${_binding_src}/ctoon_decode.m
    ${_binding_src}/ctoon_read.m
    ${_binding_src}/ctoon_write.m
    ${_binding_src}/ctoon_build.m
    ${_binding_src}/ctoon_install.m
    ${_binding_src}/buildfile.m
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ctoon_mex.${Matlab_MEX_EXTENSION}
    DESTINATION matlab
)
install(FILES ${CTOON_MATLAB_WRAPPERS} DESTINATION matlab)
install(FILES ${_binding_src}/ctoon_mex.c DESTINATION matlab/src)

# ---------------------------------------------------------------------------
# Export target  (cmake -P cmake/CToonMatlabExport.cmake)
# ---------------------------------------------------------------------------
add_custom_target(ctoon_matlab_export
    COMMAND ${CMAKE_COMMAND}
            -DEXPORT_DIR=${CMAKE_BINARY_DIR}/matlab
            -DMEX_SOURCES_PATH=${PROJECT_SOURCE_DIR}/src
            -DMEX_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/include
            -P ${PROJECT_SOURCE_DIR}/cmake/CToonMatlabExport.cmake
    COMMENT "Exporting self-contained MATLAB binding → ${CMAKE_BINARY_DIR}/matlab"
    USES_TERMINAL
)