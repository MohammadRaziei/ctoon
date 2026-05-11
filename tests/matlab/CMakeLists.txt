# =============================================================================
# CToon MATLAB Tests + Coverage
# =============================================================================
#
# Delegates all test execution and coverage reporting to the MATLAB Build Tool
# via `buildtool` — the authoritative build system for the MATLAB side.
#
# buildtool tasks used:
#   mex       compile ctoon_mex (defined as mexTask in buildfile.m)
#   test      run tests + produce HTML coverage report  (TestTask)
#   coverage  run tests + produce HTML + Cobertura XML  (TestTask)
#
# The buildfile.m lives in src/bindings/matlab/ next to ctoon_mex.c.
#
# Targets created:
#   ctoon_test_matlab       cmake --build . --target ctoon_test_matlab
#   ctoon_coverage_matlab   cmake --build . --target ctoon_coverage_matlab
# =============================================================================

if(NOT Matlab_FOUND OR NOT TARGET ctoon_mex)
    message(STATUS "MATLAB / ctoon_mex not available — skipping MATLAB tests.")
    return()
endif()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(CTOON_MATLAB_BINDING_DIR ${PROJECT_SOURCE_DIR}/src/bindings/matlab)
set(CTOON_MATLAB_BUILDFILE    ${CTOON_MATLAB_BINDING_DIR}/buildfile.m)

if(NOT EXISTS "${CTOON_MATLAB_BUILDFILE}")
    message(STATUS
        "buildfile.m not found at ${CTOON_MATLAB_BUILDFILE} — "
        "ctoon_test_matlab / ctoon_coverage_matlab skipped.")
    return()
endif()

# ---------------------------------------------------------------------------
# Helper: locate the buildtool executable
#
# `buildtool` is a MATLAB command, not a standalone binary.
# We invoke it as: matlab -batch "buildtool <task>"
# ---------------------------------------------------------------------------
set(_BT_PREFIX "cd('${CTOON_MATLAB_BINDING_DIR}'); buildtool")

# ---------------------------------------------------------------------------
# Test target  — runs `buildtool test` (mex → TestTask + HTML coverage)
# ---------------------------------------------------------------------------
add_custom_target(ctoon_test_matlab
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB build tool: test..."
    COMMAND ${Matlab_MAIN_PROGRAM} -batch "${_BT_PREFIX} test"
    WORKING_DIRECTORY ${CTOON_MATLAB_BINDING_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_test_matlab ctoon_mex)
add_dependencies(ctoon_test ctoon_test_matlab)

# CTest registration
add_test(NAME test_ctoon_matlab
    COMMAND ${Matlab_MAIN_PROGRAM} -batch "${_BT_PREFIX} test"
    WORKING_DIRECTORY ${CTOON_MATLAB_BINDING_DIR}
)

# ---------------------------------------------------------------------------
# Coverage target  — runs `buildtool coverage` (mex → TestTask + XML report)
# ---------------------------------------------------------------------------
add_custom_target(ctoon_coverage_matlab
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB build tool: coverage..."
    COMMAND ${Matlab_MAIN_PROGRAM} -batch "${_BT_PREFIX} coverage"
    WORKING_DIRECTORY ${CTOON_MATLAB_BINDING_DIR}
)
add_dependencies(ctoon_coverage_matlab ctoon_mex)
add_dependencies(ctoon_coverage ctoon_coverage_matlab)
