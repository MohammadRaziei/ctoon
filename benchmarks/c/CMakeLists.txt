# ── ctoon (fetched from GitHub, same as every other library here) ────────
include(FetchContent)
FetchContent_Declare(
    ctoon
    GIT_REPOSITORY ${CTOON_GITHUB_URL}
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ctoon)

# ── TOONc (github.com/UsboKirishima/TOONc) ────────────────────────────────
# Plain Makefile project, no CMakeLists.txt of its own — FetchContent just
# populates the source (MakeAvailable skips add_subdirectory() when there's
# no top-level CMakeLists.txt), and we build the one .c file directly.
FetchContent_Declare(
    toonc
    GIT_REPOSITORY https://github.com/UsboKirishima/TOONc.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(toonc)

add_library(toonc_upstream STATIC ${toonc_SOURCE_DIR}/toonc.c)
target_include_directories(toonc_upstream PUBLIC ${toonc_SOURCE_DIR})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(toonc_upstream PRIVATE -O3 -w) # -w: not our warnings to fix
endif()

add_executable(bench_c bench.c)
target_link_libraries(bench_c PRIVATE ctoon::ctoon toonc_upstream)
target_compile_definitions(bench_c PRIVATE
    CTOON_BENCH_MANIFEST="${CTOON_BENCH_MANIFEST}"
    CTOON_BENCH_RESULTS_JSON="${CTOON_BENCH_RESULTS_DIR}/c.json")

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bench_c PRIVATE -O3)
endif()

add_custom_target(ctoon_benchmarks_c
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running C benchmark..."
    COMMAND $<TARGET_FILE:bench_c>
    DEPENDS bench_c
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_c)
