# ctoon is fetched by `zig fetch --save`, the same way `go get` fetches
# ctoon for the Go benchmark — run before every build so build.zig.zon's
# dependency hash always matches ctoon's current master, instead of
# committing a fixed version.
add_custom_target(ctoon_benchmarks_zig_deps
    COMMAND ${ZIG_EXECUTABLE} fetch --save git+https://github.com/mohammadraziei/ctoon.git
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Fetching Zig benchmark dependency (ctoon) from GitHub"
    USES_TERMINAL
)

if(CTOON_BENCH_LOG_DEBUG)
    set(CTOON_ZIG_LOG_ARG ${CTOON_BENCH_LOGS_DIR}/zig.log)
else()
    set(CTOON_ZIG_LOG_ARG "")
endif()

# On some Linux setups (notably Debian/Ubuntu multiarch layouts, and
# some WSL/conda environments), Zig's bundled lld linker looks for libc
# pieces (libm.so.6, etc.) under /lib64 by default and doesn't find them
# if the system instead keeps them under a triplet directory like
# /usr/lib/x86_64-linux-gnu. Rather than assume any particular layout —
# or ask anyone to add a system-wide /lib64 symlink — ask the C compiler
# CMake already found where IT looks for libraries by default
# (CMAKE_C_IMPLICIT_LINK_DIRECTORIES, populated by CMake's own compiler
# detection) and hand each of those to zig via --search-prefix. No-op on
# setups that don't need it (e.g. /lib64 already resolves correctly).
set(CTOON_ZIG_SEARCH_PREFIX_ARGS "")
foreach(_dir IN LISTS CMAKE_C_IMPLICIT_LINK_DIRECTORIES)
    list(APPEND CTOON_ZIG_SEARCH_PREFIX_ARGS --search-prefix "${_dir}")
endforeach()

add_custom_target(ctoon_benchmarks_zig
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Zig benchmark..."
    COMMAND ${ZIG_EXECUTABLE} build ${CTOON_ZIG_SEARCH_PREFIX_ARGS} --release=fast run --
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR}/zig.json ${CTOON_ZIG_LOG_ARG}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks_zig ctoon_benchmarks_zig_deps)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_zig)
