# =============================================================================
# CToon MATLAB Tests + Coverage
# =============================================================================
#
# Delegates to MATLAB Build Tool:
#   buildtool test      → run tests + HTML coverage
#   buildtool coverage  → run tests + HTML + lcov + Cobertura XML
#
# Targets:
#   ctoon_test_matlab       hooks into ctoon_test
#   ctoon_coverage_matlab   hooks into ctoon_coverage
# =============================================================================

if(NOT Matlab_FOUND OR NOT TARGET ctoon_mex)
    message(STATUS "MATLAB / ctoon_mex not available — skipping MATLAB tests.")
    return()
endif()

set(_binding_src ${PROJECT_SOURCE_DIR}/src/bindings/matlab)
set(_mex_bin     ${CMAKE_BINARY_DIR}/src/bindings/matlab)

if(NOT EXISTS "${_binding_src}/buildfile.m")
    message(STATUS "buildfile.m not found — MATLAB test targets skipped.")
    return()
endif()

# ---------------------------------------------------------------------------
# Common -batch prefix using -sd and pre-set workspace variables
# ---------------------------------------------------------------------------
set(_BT_VARS
    "global BUILD_DIR MEX_SOURCES_PATH MEX_INCLUDE_DIR BUILD_COVERAGE_DIR TEST_DIR; \
     BUILD_DIR='${_mex_bin}'; \
     MEX_SOURCES_PATH='${PROJECT_SOURCE_DIR}/src'; \
     MEX_INCLUDE_DIR='${PROJECT_SOURCE_DIR}/include'; \
     BUILD_COVERAGE_DIR='${CTOON_COVERAGE_MATLAB_BINARY_DIR}'; \
     TEST_DIR='${PROJECT_SOURCE_DIR}/tests/matlab'")

# ---------------------------------------------------------------------------
# Test target
# ---------------------------------------------------------------------------
add_custom_target(ctoon_test_matlab
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB build tool: test..."
    COMMAND ${Matlab_MAIN_PROGRAM}
            -sd "${_binding_src}"
            -batch "${_BT_VARS}; buildtool test"
    WORKING_DIRECTORY ${_binding_src}
    USES_TERMINAL
)
add_dependencies(ctoon_test_matlab ctoon_mex)
add_dependencies(ctoon_test ctoon_test_matlab)

add_test(NAME test_ctoon_matlab
    COMMAND ${Matlab_MAIN_PROGRAM}
            -sd "${_binding_src}"
            -batch "${_BT_VARS}; buildtool test"
    WORKING_DIRECTORY ${_binding_src}
)

# ---------------------------------------------------------------------------
# Coverage target
# ---------------------------------------------------------------------------
add_custom_target(ctoon_coverage_matlab
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB build tool: coverage..."
    COMMAND ${Matlab_MAIN_PROGRAM}
            -sd "${_binding_src}"
            -batch "${_BT_VARS}; buildtool coverage"
    WORKING_DIRECTORY ${_binding_src}
    USES_TERMINAL
)
add_dependencies(ctoon_coverage_matlab ctoon_mex)
add_dependencies(ctoon_coverage ctoon_coverage_matlab)