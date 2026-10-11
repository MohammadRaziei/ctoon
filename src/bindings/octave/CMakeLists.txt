# =============================================================================
# CToon Octave MEX Binding
# =============================================================================
#
# There is no separate Octave source tree: GNU Octave loads the very same
# MEX gateway and +ctoon wrappers as MATLAB (src/bindings/matlab), compiled
# with `mkoctfile --mex` instead of MATLAB's `mex`. This folder only holds
# the CMake glue that builds that sources with Octave. `ctoon.manager` and
# end users do the same thing from the MATLAB source distribution
# (`ctoon_build` detects Octave and calls mkoctfile), so Octave has no
# prebuilt-binary release of its own.
#
# Targets:
#   ctoon_build_octave_mex  (ALL) — runs ctoon_build() under Octave, output
#                                   goes to ${CMAKE_BINARY_DIR}/octave/build:
#                                     ctoon_mex.mex
#                                     +ctoon/*.m
#                                   (addpath() that one folder to use it.)
#
# Needs mkoctfile (package `octave-dev` on Debian/Ubuntu) to BUILD. The
# resulting binary needs only Octave, no compiler, to run and to be tested.
# =============================================================================

cmake_minimum_required(VERSION 3.19)

ctoon_lang_wanted(CTOON_BUILD_OCTAVE _ctoon_octave_wanted)
if(NOT _ctoon_octave_wanted)
    return()
endif()

include(CtoonOctave)
ctoon_find_octave()

ctoon_require_lang(CTOON_BUILD_OCTAVE Octave_FOUND "GNU Octave"
    "${CTOON_OCTAVE_INSTALL_HINT}")

if(NOT Octave_FOUND)
    return()
endif()

ctoon_require_lang_version("${CTOON_OCTAVE_VERSION}" "${CTOON_OCTAVE_MIN_VERSION}" "GNU Octave")

message(STATUS "Octave found: ${CTOON_OCTAVE_EXECUTABLE}  (${CTOON_OCTAVE_VERSION})")

set(CTOON_OCTAVE_BINARY_DIR "${CMAKE_BINARY_DIR}/octave")
set(CTOON_OCTAVE_BUILD_DIR  "${CTOON_OCTAVE_BINARY_DIR}/build")
set(_ctoon_matlab_src       "${PROJECT_SOURCE_DIR}/src/bindings/matlab")

file(GLOB _ctoon_octave_wrappers CONFIGURE_DEPENDS "${_ctoon_matlab_src}/ctoon/*.m")

set(_ctoon_octave_mex "${CTOON_OCTAVE_BUILD_DIR}/ctoon_mex.mex")

add_custom_command(
    OUTPUT  "${_ctoon_octave_mex}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CTOON_OCTAVE_BUILD_DIR}"
    COMMAND "${CTOON_OCTAVE_EXECUTABLE}" --norc --quiet --eval
            "addpath('${_ctoon_matlab_src}'); ctoon_build('${CTOON_OCTAVE_BUILD_DIR}', true);"
    DEPENDS "${_ctoon_matlab_src}/ctoon_mex.c"
            "${_ctoon_matlab_src}/ctoon_build.m"
            "${PROJECT_SOURCE_DIR}/src/ctoon.c"
            "${PROJECT_SOURCE_DIR}/include/ctoon.h"
            ${_ctoon_octave_wrappers}
    COMMENT "Building the CToon MEX binding with Octave ${CTOON_OCTAVE_VERSION} (mkoctfile)..."
    VERBATIM
)

add_custom_target(ctoon_build_octave_mex ALL DEPENDS "${_ctoon_octave_mex}")
