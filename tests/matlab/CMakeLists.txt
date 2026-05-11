# =============================================================================
# CToon MATLAB Tests + Coverage
# =============================================================================
#
# Mirrors the pattern of tests/c/CMakeLists.txt and tests/cpp/CMakeLists.txt.
#
# Prerequisites:
#   - ctoon_mex target must already be defined (src/bindings/matlab)
#   - Matlab_FOUND, Matlab_MAIN_PROGRAM set by find_package(Matlab)
#   - LCOV_EXECUTABLE / GENHTML_EXECUTABLE for coverage (optional)
#
# Targets created:
#   ctoon_test_matlab       run MATLAB unit tests (hooks into ctoon_test)
#   ctoon_coverage_matlab   lcov/genhtml coverage  (hooks into ctoon_coverage)
# =============================================================================

# Nothing to do if MATLAB was not found (binding silently skipped)
if(NOT Matlab_FOUND OR NOT TARGET ctoon_mex)
    message(STATUS "MATLAB / ctoon_mex not available — skipping MATLAB tests.")
    return()
endif()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(CTOON_MATLAB_TEST_DIR  ${CMAKE_CURRENT_SOURCE_DIR})
set(CTOON_MATLAB_TEST_FILE ${CTOON_MATLAB_TEST_DIR}/test_ctoon_mex.m)
# Wrappers live in the binding source tree; they are also copied next to the
# MEX binary by the POST_BUILD command in src/bindings/matlab/CMakeLists.txt.
set(CTOON_MATLAB_SRC_DIR   ${PROJECT_SOURCE_DIR}/src/bindings/matlab)
set(CTOON_MEX_BUILD_DIR    $<TARGET_FILE_DIR:ctoon_mex>)
set(CTOON_COVERAGE_MATLAB_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/matlab)

# ---------------------------------------------------------------------------
# Helper: the -batch command used both for tests and coverage
# ---------------------------------------------------------------------------
set(_MATLAB_RUN_CMD
    "addpath('${CTOON_MATLAB_SRC_DIR}'); \
     addpath('${CTOON_MEX_BUILD_DIR}'); \
     addpath('${CTOON_MATLAB_TEST_DIR}'); \
     test_ctoon_mex"
)

# ---------------------------------------------------------------------------
# Test target
# ---------------------------------------------------------------------------
if(NOT EXISTS "${CTOON_MATLAB_TEST_FILE}")
    message(STATUS
        "MATLAB test file not found at ${CTOON_MATLAB_TEST_FILE} — "
        "ctoon_test_matlab skipped.")
    return()
endif()

add_custom_target(ctoon_test_matlab
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB tests..."
    COMMAND ${Matlab_MAIN_PROGRAM} -batch "${_MATLAB_RUN_CMD}"
    WORKING_DIRECTORY ${CTOON_MATLAB_TEST_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_test_matlab ctoon_mex)
add_dependencies(ctoon_test ctoon_test_matlab)

# CTest registration
add_test(NAME test_ctoon_matlab
    COMMAND ${Matlab_MAIN_PROGRAM} -batch "${_MATLAB_RUN_CMD}"
    WORKING_DIRECTORY ${CTOON_MATLAB_TEST_DIR}
)

# ---------------------------------------------------------------------------
# Coverage target
# ---------------------------------------------------------------------------
if(NOT LCOV_EXECUTABLE OR NOT GENHTML_EXECUTABLE)
    message(STATUS "lcov/genhtml not found — ctoon_coverage_matlab skipped.")
    return()
endif()

# Instrument the MEX binary with gcov
target_compile_options(ctoon_mex PRIVATE -O0 -g --coverage)
target_link_options(ctoon_mex    PRIVATE --coverage)

add_custom_target(ctoon_coverage_matlab
    # ---- reset ----
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/html
    COMMAND ${CMAKE_COMMAND} -E make_directory   ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/html

    # ---- exercise ----
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB tests for coverage..."
    COMMAND ${Matlab_MAIN_PROGRAM} -batch "${_MATLAB_RUN_CMD}"

    # ---- capture ----
    COMMAND ${CMAKE_COMMAND} -E echo "-- Capturing MEX coverage data..."
    COMMAND ${LCOV_EXECUTABLE}
            --capture
            --base-directory ${PROJECT_SOURCE_DIR}
            --directory ${CMAKE_BINARY_DIR}/src/bindings/matlab
            --exclude "*/tests/*"
            --no-external
            --rc function_coverage=1
            --output-file ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage_temp.lcov

    # ---- filter: keep only ctoon sources ----
    COMMAND ${CMAKE_COMMAND} -E echo "-- Filtering ctoon sources..."
    COMMAND ${LCOV_EXECUTABLE}
            --extract
            ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage_temp.lcov
            "*/ctoon*"
            --output-file ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage.lcov
    COMMAND ${CMAKE_COMMAND} -E remove
            ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage_temp.lcov

    # ---- HTML report ----
    COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report..."
    COMMAND ${GENHTML_EXECUTABLE} --quiet
            ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage.lcov
            --output-directory ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/html
            --prefix           ${PROJECT_SOURCE_DIR}
            --title            "CToon MATLAB MEX Coverage"
            --css-file         ${CTOON_TEST_SOURCE_DIR}/genhtml.css
            --dark-mode
    COMMAND ${CMAKE_COMMAND} -E echo
            "-- Report: ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/html/index.html"

    # ---- normalise paths for SonarQube / CI ----
    COMMAND ${CMAKE_COMMAND} -E echo "-- Normalising source paths..."
    COMMAND ${CMAKE_COMMAND}
            -DCOV_FILE=${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage.lcov
            -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
            -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake

    # ---- summary ----
    COMMAND ${CMAKE_COMMAND} -E echo "-- Coverage summary:"
    COMMAND ${LCOV_EXECUTABLE}
            --list ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage.lcov

    WORKING_DIRECTORY ${CTOON_TEST_BINARY_DIR}
    USES_TERMINAL
)

add_dependencies(ctoon_coverage_matlab ctoon_test_matlab)
add_dependencies(ctoon_coverage ctoon_coverage_matlab)

# iframe wrapper (matches pattern in tests/c and tests/cpp)
file(MAKE_DIRECTORY ${CTOON_COVERAGE_MATLAB_BINARY_DIR})
add_custom_command(TARGET ctoon_coverage_matlab POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DIFRAME_SRC="html/index.html"
            -DSECTION_TITLE="MATLAB%20MEX"
            -DLOGO_SVG="${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg"
            -DPROJECT_NAME="${PROJECT_NAME}"
            -DOUTPUT_DIR="${CTOON_COVERAGE_MATLAB_BINARY_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/GenerateIframe.cmake"
    COMMENT "Generating iframe wrapper for MATLAB MEX coverage report"
    WORKING_DIRECTORY ${CTOON_TEST_SOURCE_DIR}
)
