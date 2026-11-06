# toon-zig benchmark. Built and run with Zig 0.15.2 (its own pinned
# `ziglang` venv, created by ../CMakeLists.txt with ../ZigToolchain.cmake),
# separately from ../ctoon (Zig 0.16.x): a single executable cannot use both compilers.
#
# Its rows end up in the SAME results/zig.json as ctoon's: after the harness
# runs, merge_results.py adds them to the file ../ctoon wrote, so the report
# shows toon-zig as one more library of the Zig table. For that this target
# runs after ctoon_benchmarks_zig_ctoon, which also provides the shared
# decode input (below).
set(TOONZIG_ZIG "${CTOON_ZIG_toonzig_EXE}")

# toon-zig is fetched the same way ../ctoon fetches ctoon: `zig fetch --save`
# into build.zig.zon, from GitHub, at a pinned ref (a release tag; override
# with -DCTOON_TOONZIG_REF=...).
add_custom_target(ctoon_benchmarks_zig_toonzig_deps
    COMMAND ${CMAKE_COMMAND} -E env ${CTOON_ZIG_toonzig_ENV}
            ${TOONZIG_ZIG} fetch --save "git+https://github.com/LatentEvals/toon-zig.git#${CTOON_TOONZIG_REF}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Fetching Zig benchmark dependency (toon-zig ${CTOON_TOONZIG_REF}) from GitHub"
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks_zig_toonzig_deps ctoon_zig_toolchain_toonzig)

if(CTOON_BENCH_LOG_DEBUG)
    set(CTOON_TOONZIG_LOG_ARG ${CTOON_BENCH_LOGS_DIR}/zig-toon.log)
else()
    set(CTOON_TOONZIG_LOG_ARG "")
endif()

# Written outside results/ on purpose: results/*.json are the per-language
# files the verifier and the report read, and this partial one would look
# like a second "zig" language.
set(CTOON_TOONZIG_PARTIAL ${CMAKE_CURRENT_BINARY_DIR}/toon-zig.partial.json)

# The harness reads the shared TOON input from ctoon's dump
# (results/dump_zig_ctoon_json_to_toon.frames, written by ../ctoon) and writes
# its own dumps next to it for the verifier. toon-zig needs no libc, so
# unlike ../ctoon there are no libc / search-prefix arguments here.
add_custom_target(ctoon_benchmarks_zig_toonzig
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running toon-zig benchmark (Zig ${CTOON_TOONZIG_ZIG_VERSION})..."
    COMMAND ${CMAKE_COMMAND} -E env ${CTOON_ZIG_toonzig_ENV}
            ${TOONZIG_ZIG} build --release=fast run --
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR} ${CTOON_TOONZIG_PARTIAL} ${CTOON_TOONZIG_LOG_ARG}
    COMMAND ${CTOON_ZIG_toonzig_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/merge_results.py
            ${CTOON_BENCH_RESULTS_DIR}/zig.json ${CTOON_TOONZIG_PARTIAL}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks_zig_toonzig ctoon_benchmarks_zig_toonzig_deps ctoon_benchmarks_zig_ctoon)
