if(NOT Matlab_FOUND OR NOT TARGET ctoon_build_mex)
    message(STATUS "MATLAB / ctoon_build_mex not available — skipping MATLAB benchmark.")
    return()
endif()

set(CTOON_MATLAB_SOURCE_DIR "${CTOON_ROOT_DIR}/src/bindings/matlab")
set(Matlab_EXECUTABLE ${Matlab_MAIN_PROGRAM} -nosplash -noFigureWindows -nodesktop -singleCompThread -nojvm)

add_custom_target(ctoon_benchmark_matlab
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB benchmark..."
    COMMAND ${Matlab_EXECUTABLE} -sd "${CTOON_MATLAB_SOURCE_DIR}"
            -batch "addpath('${CMAKE_CURRENT_SOURCE_DIR}'); bench_ctoon('${CTOON_BENCH_MANIFEST}')"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running MATLAB benchmark..."
)
add_dependencies(ctoon_benchmark_matlab ctoon_build_mex)
add_dependencies(ctoon_benchmark ctoon_benchmark_matlab)
