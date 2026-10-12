include(FetchContent)
FetchContent_Declare(
    ctoon
    GIT_REPOSITORY ${CTOON_GITHUB_URL}
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ctoon)

# The fetched ctoon defines ctoon_build_octave_mex only when it finds
# Octave + mkoctfile itself (CTOON_BUILD_OCTAVE=ON requires them; AUTO
# detects them). Without it there is nothing to benchmark.
if(NOT TARGET ctoon_build_octave_mex)
    message(STATUS "ctoon_build_octave_mex not available (Octave / mkoctfile "
                    "not found by the fetched ctoon's own CMakeLists.txt) — "
                    "skipping Octave benchmark.")
    return()
endif()

set(CTOON_OCTAVE_BUILD_DIR "${CMAKE_BINARY_DIR}/octave/build")

if(CTOON_BENCH_LOG_DEBUG)
    set(CTOON_OCTAVE_LOG_ARG "'${CTOON_BENCH_LOGS_DIR}/octave.log'")
else()
    set(CTOON_OCTAVE_LOG_ARG "''")
endif()

add_custom_target(ctoon_benchmarks_octave
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Octave benchmark..."
    COMMAND ${OCTAVE_EXECUTABLE} --norc --quiet
            --eval "addpath('${CTOON_OCTAVE_BUILD_DIR}'); addpath('${CMAKE_CURRENT_SOURCE_DIR}'); bench('${CTOON_BENCH_MANIFEST}', '${CTOON_BENCH_RESULTS_DIR}/octave.json', ${CTOON_OCTAVE_LOG_ARG})"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Running Octave benchmark..."
)
add_dependencies(ctoon_benchmarks_octave ctoon_build_octave_mex)
add_dependencies(ctoon_bench_langs ctoon_benchmarks_octave)
