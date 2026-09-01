add_executable(bench_ctoon_cpp bench_ctoon.cpp)
target_link_libraries(bench_ctoon_cpp PRIVATE ctoon::ctoonpp)
target_compile_definitions(bench_ctoon_cpp PRIVATE
    CTOON_BENCH_MANIFEST="${CTOON_BENCH_MANIFEST}")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bench_ctoon_cpp PRIVATE -O3)
endif()

add_custom_target(ctoon_benchmark_cpp
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running C++ benchmark..."
    COMMAND $<TARGET_FILE:bench_ctoon_cpp>
    DEPENDS bench_ctoon_cpp
    USES_TERMINAL
)
add_dependencies(ctoon_benchmark ctoon_benchmark_cpp)
