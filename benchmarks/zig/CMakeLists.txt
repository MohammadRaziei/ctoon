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

add_custom_target(ctoon_benchmarks_zig
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Zig benchmark..."
    COMMAND ${ZIG_EXECUTABLE} build run -Doptimize=ReleaseFast --
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR}/zig.json ${CTOON_ZIG_LOG_ARG}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks_zig ctoon_benchmarks_zig_deps)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_zig)
