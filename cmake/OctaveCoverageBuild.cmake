# Builds an INSTRUMENTED copy of the Octave MEX binding for coverage.
# Run with: cmake -D... -P OctaveCoverageBuild.cmake
#
# Inputs:
#   MKOCTFILE      path to mkoctfile
#   PROJECT_ROOT   ctoon source root
#   OBJ_DIR        where the .o / .gcno files are kept (gcda land here too,
#                  so this directory must outlive the test run)
#   BUILD_DIR      output: <BUILD_DIR>/ctoon_mex.mex + <BUILD_DIR>/+ctoon/*.m
#
# Why not just call ctoon_build() with CFLAGS set? `mkoctfile --mex file.c`
# compiles in a temporary directory and deletes the objects, taking the
# .gcno notes files with it -- lcov then has nothing to map the .gcda counters
# back to. So compile (-c) and link as two steps, keeping the objects.

foreach(_var MKOCTFILE PROJECT_ROOT OBJ_DIR BUILD_DIR)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "OctaveCoverageBuild.cmake: ${_var} is not set")
    endif()
endforeach()

set(_gateway "${PROJECT_ROOT}/src/bindings/matlab/ctoon_mex.c")
set(_core    "${PROJECT_ROOT}/src/ctoon.c")
set(_inc     "${PROJECT_ROOT}/include")

# Start from a clean slate: stale .gcda from an earlier run would be merged
# into this one's counters.
file(REMOVE_RECURSE "${OBJ_DIR}" "${BUILD_DIR}")
file(MAKE_DIRECTORY "${OBJ_DIR}" "${BUILD_DIR}/+ctoon")

set(ENV{CFLAGS}  "--coverage -O0 -g")
set(ENV{LDFLAGS} "--coverage")

function(_run_step DESCRIPTION)
    execute_process(
        COMMAND ${ARGN}
        WORKING_DIRECTORY "${OBJ_DIR}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE  _out)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${DESCRIPTION} failed (${_rc}):\n${_out}")
    endif()
endfunction()

_run_step("compiling ctoon_mex.c" "${MKOCTFILE}" --mex -c "-I${_inc}" "${_gateway}" -o ctoon_mex.o)
_run_step("compiling ctoon.c"     "${MKOCTFILE}" --mex -c "-I${_inc}" "${_core}"    -o ctoon.o)
_run_step("linking ctoon_mex.mex" "${MKOCTFILE}" --mex -o "${BUILD_DIR}/ctoon_mex" ctoon_mex.o ctoon.o)

file(GLOB _wrappers "${PROJECT_ROOT}/src/bindings/matlab/ctoon/*.m")
file(COPY ${_wrappers} DESTINATION "${BUILD_DIR}/+ctoon")

message(STATUS "Instrumented Octave MEX built: ${BUILD_DIR}/ctoon_mex.mex")
