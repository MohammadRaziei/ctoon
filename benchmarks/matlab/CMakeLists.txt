include(FetchContent)
FetchContent_Declare(
    ctoon
    GIT_REPOSITORY ${CTOON_GITHUB_URL}
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ctoon)

if(NOT TARGET ctoon_build_mex)
    message(STATUS "ctoon_build_mex not available (MATLAB not found by the "
                    "fetched ctoon's own CMakeLists.txt) — skipping MATLAB benchmark.")
    return()
endif()

set(CTOON_MATLAB_SOURCE_DIR "${ctoon_SOURCE_DIR}/src/bindings/matlab")
set(Matlab_EXECUTABLE ${Matlab_MAIN_PROGRAM} -nosplash -noFigureWindows -nodesktop -singleCompThread -nojvm)

add_custom_target(ctoon_benchmarks_matlab
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB benchmark..."
    COMMAND ${Matlab_EXECUTABLE} -sd "${CTOON_MATLAB_SOURCE_DIR}"
            -batch "addpath('${CMAKE_CURRENT_SOURCE_DIR}'); bench('${CTOON_BENCH_MANIFEST}', '${CTOON_BENCH_RESULTS_DIR}/matlab.json')"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running MATLAB benchmark..."
)
add_dependencies(ctoon_benchmarks_matlab ctoon_build_mex)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_matlab)
