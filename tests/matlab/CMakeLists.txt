# =============================================================================
# CToon MATLAB Tests + Coverage
# =============================================================================
#
# Delegates to MATLAB Build Tool via root buildfile.m:
#   buildtool test      → build + tests + HTML coverage
#   buildtool coverage  → build + tests + HTML + lcov
#
# Targets:
#   ctoon_test_matlab       hooks into ctoon_test
#   ctoon_coverage_matlab   hooks into ctoon_coverage
# =============================================================================

if(NOT Matlab_FOUND OR NOT TARGET ctoon_mex)
    message(STATUS "MATLAB / ctoon_mex not available — skipping MATLAB tests.")
    return()
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/buildfile.m")
    message(STATUS "buildfile.m not found in repo root — MATLAB test targets skipped.")
    return()
endif()

set(_mex_bin ${CMAKE_BINARY_DIR}/src/bindings/matlab)

# Common variables passed to buildtool
set(_BT_VARS
    "global BUILD_DIR COVERAGE_OUTPUT_DIR; \
     BUILD_DIR='${_mex_bin}'; \
     COVERAGE_OUTPUT_DIR='${CTOON_COVERAGE_MATLAB_BINARY_DIR}'")

# ---------------------------------------------------------------------------
# Test target
# ---------------------------------------------------------------------------
add_custom_target(ctoon_test_matlab
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB build tool: test..."
    COMMAND ${Matlab_MAIN_PROGRAM}
            -sd "${PROJECT_SOURCE_DIR}"
            -batch "${_BT_VARS}; buildtool test"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
)
add_dependencies(ctoon_test_matlab ctoon_mex)
add_dependencies(ctoon_test ctoon_test_matlab)

add_test(NAME test_ctoon_matlab
    COMMAND ${Matlab_MAIN_PROGRAM}
            -sd "${PROJECT_SOURCE_DIR}"
            -batch "${_BT_VARS}; buildtool test"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# ---------------------------------------------------------------------------
# Coverage target
# ---------------------------------------------------------------------------
add_custom_target(ctoon_coverage_matlab
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB build tool: coverage..."
    COMMAND ${Matlab_MAIN_PROGRAM}
            -sd "${PROJECT_SOURCE_DIR}"
            -batch "${_BT_VARS}; buildtool coverage"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_coverage_matlab ctoon_mex)
add_dependencies(ctoon_coverage ctoon_coverage_matlab)