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

# ctoon_build_mex (defined in the fetched ctoon's own
# src/bindings/matlab/CMakeLists.txt) outputs the compiled MEX binary and
# the +ctoon/ package folder to ${CMAKE_BINARY_DIR}/Matlab/build/+ctoon/ —
# NOT into the plain, unprefixed `ctoon/` source folder (that one has no
# '+' prefix, so it's not a real MATLAB package and ctoon.foo() can never
# resolve there no matter what's on the path). CMAKE_BINARY_DIR is the
# same value everywhere in one build tree, so this matches exactly what
# that subdirectory computed for itself without needing PARENT_SCOPE.
set(CTOON_MATLAB_BUILD_DIR "${CMAKE_BINARY_DIR}/Matlab/build")
set(Matlab_EXECUTABLE ${Matlab_MAIN_PROGRAM} -nosplash -noFigureWindows -nodesktop -singleCompThread -nojvm)

add_custom_target(ctoon_benchmarks_matlab
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB benchmark..."
    COMMAND ${Matlab_EXECUTABLE} -sd "${CMAKE_CURRENT_SOURCE_DIR}"
            -batch "addpath('${CTOON_MATLAB_BUILD_DIR}'); addpath('${CMAKE_CURRENT_SOURCE_DIR}'); bench('${CTOON_BENCH_MANIFEST}', '${CTOON_BENCH_RESULTS_DIR}/matlab.json')"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running MATLAB benchmark..."
)
add_dependencies(ctoon_benchmarks_matlab ctoon_build_mex)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_matlab)
