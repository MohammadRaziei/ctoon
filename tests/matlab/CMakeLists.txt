# =============================================================================
# CToon MATLAB Tests + Coverage
# =============================================================================
#
# Targets:
#   ctoon_test_matlab       run test suite, hooks into ctoon_test
#   ctoon_coverage_matlab   run tests + Cobertura XML, hooks into ctoon_coverage
# =============================================================================
find_package(Matlab COMPONENTS MAIN_PROGRAM MX_LIBRARY MEX_COMPILER QUIET)

if(NOT Matlab_FOUND OR NOT TARGET ctoon_build_mex)
    message(STATUS "MATLAB / ctoon_build_mex not available — skipping MATLAB tests.")
    return()
endif()

set(CTOON_MATLAB_BINARY_DIR "${CMAKE_BINARY_DIR}/Matlab")
set(CTOON_MATLAB_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src/bindings/matlab")
set(Matlab_EXECUTABLE ${Matlab_MAIN_PROGRAM} -nosplash -noFigureWindows -nodesktop -singleCompThread -nojvm)

# ---------------------------------------------------------------------------
# Test target
# ---------------------------------------------------------------------------

add_custom_target(ctoon_test_matlab
    COMMAND ${Matlab_EXECUTABLE} -sd "${CTOON_MATLAB_SOURCE_DIR}"
            -batch "buildtool config(buildDir='${CTOON_MATLAB_BINARY_DIR}/build', coverageOutputDir='${CTOON_COVERAGE_MATLAB_BINARY_DIR}') test"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running MATLAB tests..."
)
add_dependencies(ctoon_test_matlab ctoon_build_mex)
add_dependencies(ctoon_test        ctoon_test_matlab)

add_test(NAME test_ctoon_matlab
    COMMAND ${Matlab_EXECUTABLE} -sd "${CTOON_MATLAB_SOURCE_DIR}"
            -batch "buildtool config(buildDir='${CTOON_MATLAB_BINARY_DIR}/build', coverageOutputDir='${CTOON_COVERAGE_MATLAB_BINARY_DIR}') test"
)

# ---------------------------------------------------------------------------
# Coverage target
# ---------------------------------------------------------------------------

add_custom_target(ctoon_coverage_matlab
    COMMAND ${Matlab_EXECUTABLE} -sd "${CTOON_MATLAB_SOURCE_DIR}" 
            -batch "buildtool coverage"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running MATLAB tests with coverage → ${CTOON_COVERAGE_MATLAB_BINARY_DIR}"
)
add_dependencies(ctoon_coverage_matlab ctoon_build_mex)
add_dependencies(ctoon_coverage        ctoon_coverage_matlab)