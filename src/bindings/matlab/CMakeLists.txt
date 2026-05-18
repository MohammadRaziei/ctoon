# =============================================================================
# CToon MATLAB MEX Binding
# =============================================================================
#
# Targets:
#   ctoon_build_mex  (ALL) — compile MEX via FindMatlab, output to
#                            ${CTOON_MATLAB_BINARY_DIR}/build/+ctoon/private/
#                            then copy .m wrappers into +ctoon/
#
#   ctoon_matlab_export    — copy self-contained export tree to
#                            ${CTOON_MATLAB_BINARY_DIR}/export/
#
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

set(CTOON_MATLAB_BINARY_DIR "${CMAKE_BINARY_DIR}/Matlab")

# ---------------------------------------------------------------------------
# MEX target
# ---------------------------------------------------------------------------
matlab_add_mex(
    NAME        ctoon_build_mex
    SRC         "${CMAKE_CURRENT_SOURCE_DIR}/ctoon_mex.c"
                "${PROJECT_SOURCE_DIR}/src/ctoon.c"
    OUTPUT_NAME ctoon_mex
    NO_IMPLICIT_LINK_TO_MATLAB_LIBRARIES
)

target_include_directories(ctoon_build_mex PRIVATE "${PROJECT_SOURCE_DIR}/include")

set_target_properties(ctoon_build_mex PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CTOON_MATLAB_BINARY_DIR}/build/+ctoon/private"
    RUNTIME_OUTPUT_DIRECTORY "${CTOON_MATLAB_BINARY_DIR}/build/+ctoon/private"
)

add_custom_command(TARGET ctoon_build_mex PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CTOON_MATLAB_BINARY_DIR}/build/+ctoon/private"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/ctoon"
            "${CTOON_MATLAB_BINARY_DIR}/build/+ctoon"
    COMMENT "Creating +ctoon/private/ and copying .m wrappers..."
)

# ---------------------------------------------------------------------------
# Export target
# ---------------------------------------------------------------------------
# Layout:
#   Matlab/export/
#     ctoon/          ← .m wrappers folder
#     ctoon_mex.c
#     ctoon_build.m
#     ctoon_install.m
#     ctoon_clean.m
#     ctoon.c
#     ctoon.h
add_custom_target(ctoon_matlab_export
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CTOON_MATLAB_BINARY_DIR}/export"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/ctoon"
            "${CTOON_MATLAB_BINARY_DIR}/export/ctoon"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/ctoon_build.m"
            "${CMAKE_CURRENT_SOURCE_DIR}/ctoon_install.m"
            "${CMAKE_CURRENT_SOURCE_DIR}/ctoon_clean.m"
            "${CMAKE_CURRENT_SOURCE_DIR}/ctoon_mex.c"
            "${PROJECT_SOURCE_DIR}/src/ctoon.c"
            "${PROJECT_SOURCE_DIR}/include/ctoon.h"
            "${CTOON_MATLAB_BINARY_DIR}/export"
    COMMENT "Exporting self-contained MATLAB binding → ${CTOON_MATLAB_BINARY_DIR}/export"
    VERBATIM
)