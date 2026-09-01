add_executable(bench_ctoon_c bench_ctoon.c)
target_link_libraries(bench_ctoon_c PRIVATE ctoon::ctoon)
target_compile_definitions(bench_ctoon_c PRIVATE
    CTOON_BENCH_MANIFEST="${CTOON_BENCH_MANIFEST}")

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bench_ctoon_c PRIVATE -O3)
endif()

# ── Competitor: TOONc (github.com/UsboKirishima/TOONc) ─────────────────
# Plain Makefile project, no CMakeLists.txt of its own — FetchContent just
# populates the source (MakeAvailable skips add_subdirectory() when there's
# no top-level CMakeLists.txt to add), and we build the one .c file
# directly. TOONc only implements TOON -> JSON (it has no JSON parser and
# no TOON writer), so that's the only direction it's compared on.
include(FetchContent)
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

target_link_libraries(bench_ctoon_c PRIVATE toonc_upstream)
target_compile_definitions(bench_ctoon_c PRIVATE CTOON_BENCH_HAVE_TOONC=1)

add_custom_target(ctoon_benchmark_c
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running C benchmark..."
    COMMAND $<TARGET_FILE:bench_ctoon_c>
    DEPENDS bench_ctoon_c
    USES_TERMINAL
)
add_dependencies(ctoon_benchmark ctoon_benchmark_c)
