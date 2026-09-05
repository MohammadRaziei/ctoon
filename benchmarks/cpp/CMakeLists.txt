include(FetchContent)
FetchContent_Declare(
    ctoon
    GIT_REPOSITORY ${CTOON_GITHUB_URL}
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ctoon)

add_executable(bench_cpp bench.cpp)
target_link_libraries(bench_cpp PRIVATE ctoon::ctoonpp)
target_compile_definitions(bench_cpp PRIVATE
    CTOON_BENCH_MANIFEST="${CTOON_BENCH_MANIFEST}"
    CTOON_BENCH_RESULTS_JSON="${CTOON_BENCH_RESULTS_DIR}/cpp.json")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bench_cpp PRIVATE -O3)
endif()

add_custom_target(ctoon_benchmarks_cpp
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running C++ benchmark..."
    COMMAND $<TARGET_FILE:bench_cpp>
    DEPENDS bench_cpp
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_cpp)
