# =============================================================================
# CToon Octave Tests + Coverage
# =============================================================================
#
# Targets:
#   ctoon_test_octave       run the MOxUnit suite against the built binding,
#                           hooks into ctoon_test
#   ctoon_coverage_octave   build an INSTRUMENTED copy of the MEX gateway
#                           (gcc --coverage), run the suite against it and
#                           turn the gcov data into lcov + HTML. This covers
#                           the C code that actually runs under Octave
#                           (ctoon_mex.c and ctoon.c); the one-line .m
#                           wrappers are not worth line coverage. Hooks into
#                           ctoon_coverage and is merged into the lcov total.
#
# The binding is built by src/bindings/octave (ctoon_build_octave_mex), which
# needs mkoctfile. Nothing here compiles anything at test time: the tests
# only load the already-built binary, so Octave alone is enough to run them.
# (Only the coverage target compiles, because it needs its own instrumented
# build.)
#
# MOxUnit (https://github.com/MOxUnit/MOxUnit) is the test framework: Octave
# has no matlab.unittest, so tests/octave/test_ctoon.m is a MOxUnit port of
# tests/matlab/test_ctoon.m. It is fetched at configure time, pinned to a
# commit, unless -DCTOON_MOXUNIT_DIR=<checkout> points at an existing copy
# (offline builds).
# =============================================================================

ctoon_lang_wanted(CTOON_BUILD_OCTAVE _ctoon_octave_wanted)
if(NOT _ctoon_octave_wanted)
    return()
endif()

if(NOT TARGET ctoon_build_octave_mex)
    message(STATUS "Octave / ctoon_build_octave_mex not available - skipping Octave tests.")
    return()
endif()

include(CtoonOctave)
ctoon_find_octave()

# Same locations src/bindings/octave/CMakeLists.txt builds into (directory
# scope: those variables are not visible here, so they are recomputed).
set(CTOON_OCTAVE_BINARY_DIR "${CMAKE_BINARY_DIR}/octave")
set(CTOON_OCTAVE_BUILD_DIR  "${CTOON_OCTAVE_BINARY_DIR}/build")
# The coverage target makes its own instrumented build and may run before (or
# without) ctoon_build_octave_mex, so its WORKING_DIRECTORY has to exist.
file(MAKE_DIRECTORY "${CTOON_OCTAVE_BINARY_DIR}")

# ---------------------------------------------------------------------------
# MOxUnit
# ---------------------------------------------------------------------------

set(CTOON_MOXUNIT_DIR "" CACHE PATH
    "Existing MOxUnit checkout (folder containing MOxUnit/moxunit_runtests.m); fetched when empty")

if(NOT CTOON_MOXUNIT_DIR)
    include(FetchContent)
    FetchContent_Declare(
        moxunit
        GIT_REPOSITORY https://github.com/MOxUnit/MOxUnit.git
        # Pinned commit (the repository publishes no release tags). Keep this
        # in sync with the `ref:` used for MOxUnit in .github/workflows/mex.yml.
        GIT_TAG        77efb9190bc279de54de3e45780f2f148035affe
        GIT_PROGRESS   TRUE
    )
    FetchContent_MakeAvailable(moxunit)
    set(_ctoon_moxunit_dir "${moxunit_SOURCE_DIR}")
else()
    set(_ctoon_moxunit_dir "${CTOON_MOXUNIT_DIR}")
endif()

if(NOT EXISTS "${_ctoon_moxunit_dir}/MOxUnit/moxunit_runtests.m")
    message(WARNING "MOxUnit not found in ${_ctoon_moxunit_dir} - skipping Octave tests.")
    return()
endif()

set(_ctoon_octave_tests_dir "${CTOON_TEST_SOURCE_DIR}/octave")
set(_ctoon_octave_junit     "${CTOON_OCTAVE_BINARY_DIR}/junit.xml")

# eval string for run_tests(moxunitDir, buildDir[, junitFile])
function(_ctoon_octave_eval OUT_VAR BUILD_DIR JUNIT_FILE)
    set(_expr "addpath('${_ctoon_octave_tests_dir}'); run_tests('${_ctoon_moxunit_dir}', '${BUILD_DIR}'")
    if(JUNIT_FILE)
        string(APPEND _expr ", '${JUNIT_FILE}'")
    endif()
    string(APPEND _expr ");")
    set(${OUT_VAR} "${_expr}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Test target
# ---------------------------------------------------------------------------

_ctoon_octave_eval(_ctoon_octave_test_eval "${CTOON_OCTAVE_BUILD_DIR}" "${_ctoon_octave_junit}")

add_custom_target(ctoon_test_octave
    COMMAND ${CMAKE_COMMAND} -E env "CTOON_SPEC_FIXTURES_DIR=${CTOON_SPEC_FIXTURES_DIR}"
            "${CTOON_OCTAVE_EXECUTABLE}" --norc --quiet --eval "${_ctoon_octave_test_eval}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running Octave tests..."
)
add_dependencies(ctoon_test_octave ctoon_build_octave_mex)
add_dependencies(ctoon_test        ctoon_test_octave)

add_test(NAME test_ctoon_octave
    COMMAND "${CTOON_OCTAVE_EXECUTABLE}" --norc --quiet --eval "${_ctoon_octave_test_eval}"
)
set_tests_properties(test_ctoon_octave PROPERTIES TIMEOUT 300)
if(CTOON_SPEC_FIXTURES_DIR)
    set_tests_properties(test_ctoon_octave PROPERTIES
        ENVIRONMENT "CTOON_SPEC_FIXTURES_DIR=${CTOON_SPEC_FIXTURES_DIR}")
endif()

# ---------------------------------------------------------------------------
# Coverage target (gcc/gcov, Linux only)
# ---------------------------------------------------------------------------
# The instrumented build is made by cmake/OctaveCoverageBuild.cmake and the
# lcov steps below mirror tests/c/CMakeLists.txt, so the result drops into the
# same merged total and dashboard as the other languages.

if(NOT (LCOV_EXECUTABLE AND GENHTML_EXECUTABLE))
    message(STATUS "lcov/genhtml not found - ctoon_coverage_octave not available.")
    return()
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Octave coverage uses gcc/gcov and is only set up on Linux - ctoon_coverage_octave not available.")
    return()
endif()

set(CTOON_COVERAGE_OCTAVE_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/octave)
set(_ctoon_octave_cov_obj   "${CTOON_OCTAVE_BINARY_DIR}/coverage-obj")
set(_ctoon_octave_cov_build "${CTOON_OCTAVE_BINARY_DIR}/coverage-build")

_ctoon_octave_eval(_ctoon_octave_cov_eval "${_ctoon_octave_cov_build}" "")

add_custom_target(ctoon_coverage_octave
    # Clean previous coverage data (only html, keep index.html wrapper)
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CTOON_COVERAGE_OCTAVE_BINARY_DIR}/html
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CTOON_COVERAGE_OCTAVE_BINARY_DIR}/html

    # 1. Instrumented build (also wipes stale .gcda from earlier runs)
    COMMAND ${CMAKE_COMMAND} -E echo "-- Building instrumented Octave MEX..."
    COMMAND ${CMAKE_COMMAND}
            -DMKOCTFILE=${CTOON_OCTAVE_MKOCTFILE}
            -DPROJECT_ROOT=${PROJECT_SOURCE_DIR}
            -DOBJ_DIR=${_ctoon_octave_cov_obj}
            -DBUILD_DIR=${_ctoon_octave_cov_build}
            -P ${PROJECT_SOURCE_DIR}/cmake/OctaveCoverageBuild.cmake

    # 2. Run the suite against it (the .gcda files are written when Octave exits)
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Octave tests under coverage..."
    COMMAND ${CMAKE_COMMAND} -E env "CTOON_SPEC_FIXTURES_DIR=${CTOON_SPEC_FIXTURES_DIR}"
            "${CTOON_OCTAVE_EXECUTABLE}" --norc --quiet --eval "${_ctoon_octave_cov_eval}"

    # 3. Capture, keeping only ctoon's own sources (not Octave's headers)
    COMMAND ${CMAKE_COMMAND} -E echo "-- Capturing Octave coverage..."
    COMMAND ${LCOV_EXECUTABLE}
            --capture
            --base-directory ${PROJECT_SOURCE_DIR}
            --directory ${_ctoon_octave_cov_obj}
            --no-external
            --rc function_coverage=1
            --output-file ${CTOON_COVERAGE_OCTAVE_BINARY_DIR}/coverage_temp.lcov
    COMMAND ${LCOV_EXECUTABLE}
            --extract
            ${CTOON_COVERAGE_OCTAVE_BINARY_DIR}/coverage_temp.lcov
            "*/ctoon.c" "*/ctoon.h" "*/ctoon_mex.c"
            --output-file ${CTOON_COVERAGE_OCTAVE_BINARY_DIR}/coverage.lcov
    COMMAND ${CMAKE_COMMAND} -E remove ${CTOON_COVERAGE_OCTAVE_BINARY_DIR}/coverage_temp.lcov

    # 4. HTML report
    COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report..."
    COMMAND ${GENHTML_EXECUTABLE} --quiet
            ${CTOON_COVERAGE_OCTAVE_BINARY_DIR}/coverage.lcov
            --output-directory ${CTOON_COVERAGE_OCTAVE_BINARY_DIR}/html
            --prefix ${PROJECT_SOURCE_DIR}
            --title "CToon Octave Coverage"
            --css-file ${CTOON_TEST_SOURCE_DIR}/genhtml.css
            --dark-mode
    COMMAND ${CMAKE_COMMAND} -E echo "-- HTML entry point created here: ${CTOON_COVERAGE_OCTAVE_BINARY_DIR}/html/index.html"

    # Fix paths to be relative (for SonarQube, etc.)
    COMMAND ${CMAKE_COMMAND}
            -DCOV_FILE=${CTOON_COVERAGE_OCTAVE_BINARY_DIR}/coverage.lcov
            -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
            -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake
    COMMAND ${CMAKE_COMMAND} -E echo "-- Coverage summary:"
    COMMAND ${LCOV_EXECUTABLE} --list ${CTOON_COVERAGE_OCTAVE_BINARY_DIR}/coverage.lcov

    WORKING_DIRECTORY ${CTOON_OCTAVE_BINARY_DIR}
    USES_TERMINAL
    VERBATIM
)
add_dependencies(ctoon_coverage ctoon_coverage_octave)

# Iframe wrapper for the dashboard
file(MAKE_DIRECTORY ${CTOON_COVERAGE_OCTAVE_BINARY_DIR})
add_custom_command(TARGET ctoon_coverage_octave POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DIFRAME_SRC="html/index.html"
            -DSECTION_TITLE="Octave%20MEX"
            -DLOGO_SVG="${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg"
            -DPROJECT_NAME="${PROJECT_NAME}"
            -DOUTPUT_DIR="${CTOON_COVERAGE_OCTAVE_BINARY_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/GenerateIframe.cmake"
    COMMENT "Generating iframe wrapper for Octave coverage report"
    WORKING_DIRECTORY ${CTOON_TEST_SOURCE_DIR}
)
