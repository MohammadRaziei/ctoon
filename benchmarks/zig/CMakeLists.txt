# Zig benchmarks: two independent projects, because two Zig versions are
# needed and one compiler cannot serve both.
#   ctoon/     ctoon,    built with Zig 0.16.x  (CTOON_ZIG_VERSION)
#   zig-toon/  toon-zig, built with Zig 0.15.2  (CTOON_TOONZIG_ZIG_VERSION)
# Zig is never taken from the system: each version is installed from PyPI
# (`ziglang`) into its own pinned venv with its own Zig cache, see
# ZigToolchain.cmake. zig-toon/ runs after ctoon/ and adds its rows to the
# same results/zig.json.
set(CTOON_ZIG_VERSION "0.16.0" CACHE STRING
    "ziglang version for the ctoon Zig benchmark (zig/ctoon/build.zig targets Zig 0.16.x)")
set(CTOON_TOONZIG_ZIG_VERSION "0.15.2" CACHE STRING
    "ziglang version for the toon-zig benchmark (toon-zig's minimum_zig_version is 0.15.2)")
set(CTOON_TOONZIG_REF "v0.1.3" CACHE STRING "toon-zig git ref (tag) the benchmark fetches")

include("${CMAKE_CURRENT_LIST_DIR}/ZigToolchain.cmake")
ctoon_bench_zig_toolchain(ctoon ${CTOON_ZIG_VERSION})
ctoon_bench_zig_toolchain(toonzig ${CTOON_TOONZIG_ZIG_VERSION})

add_subdirectory(ctoon)
add_subdirectory(zig-toon)

# ctoon_benchmarks_zig runs both.
add_custom_target(ctoon_benchmarks_zig)
add_dependencies(ctoon_benchmarks_zig ctoon_benchmarks_zig_ctoon ctoon_benchmarks_zig_toonzig)
add_dependencies(ctoon_bench_langs ctoon_benchmarks_zig)
