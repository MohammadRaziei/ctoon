cmake_minimum_required(VERSION 3.19)

# ==============================================================================
# CToon Benchmarks - fully standalone CMake project.
#
# This has NO relationship to the repository root - not even for ctoon
# itself. Every language folder fetches ctoon the exact same way it fetches
# any competitor: from https://github.com/mohammadraziei/ctoon.git (C/C++),
# `go get github.com/mohammadraziei/ctoon` (Go), a git dependency in
# Cargo.toml (Rust), `zig fetch --save` (Zig), or
# `pip install git+https://github.com/mohammadraziei/ctoon.git` (Python).
# ctoon is never a special case here - it's benchmarked exactly like
# gotoon, toon-go, or TOONc: one more entry in the same results table.
#
# Build it on its own:
#
#   cd benchmarks
#   cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-bench --target ctoon_benchmarks
#
# Layout mirrors tests/: one folder per language (c, cpp, python, go,
# rust, zig, matlab, octave), each with its own CMakeLists.txt that fetches its
# own dependencies and defines a ctoon_benchmarks_<lang> target. A
# language is skipped with a warning only when its toolchain isn't found
# - see "Toolchain detection" below.
# ==============================================================================

project(CToonBenchmarks LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "Benchmarks are being built in Debug mode - numbers will "
                     "not reflect real performance. Pass -DCMAKE_BUILD_TYPE=Release.")
endif()

set(CTOON_BENCH_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CTOON_BENCH_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CTOON_GITHUB_URL "https://github.com/mohammadraziei/ctoon.git")

# Every language benchmark writes its own JSON results file here - kept
# separate per language rather than merged, since each language's set of
# libraries/operations differs.
set(CTOON_BENCH_RESULTS_DIR ${CTOON_BENCH_BINARY_DIR}/results)
file(MAKE_DIRECTORY ${CTOON_BENCH_RESULTS_DIR})

# When ON, every language benchmark writes a per-file diagnostic log
# (which files failed, and why, for each library/operation) into
# build-bench/logs/<language>.log - off by default since it's extra I/O
# and only useful when actively investigating a success-rate anomaly.
option(CTOON_BENCH_LOG_DEBUG
    "Write per-file diagnostic logs (which files failed and why) to build-bench/logs/" OFF)
set(CTOON_BENCH_LOGS_DIR ${CTOON_BENCH_BINARY_DIR}/logs)
if(CTOON_BENCH_LOG_DEBUG)
    file(MAKE_DIRECTORY ${CTOON_BENCH_LOGS_DIR})
    message(STATUS "CToon benchmarks: debug logging ON -> ${CTOON_BENCH_LOGS_DIR}")
endif()

# ── Corpus: fetch real-world JSON to benchmark against ────────────────────
include(FetchContent)

# Small, varied JSON shapes covering the format's edge cases - the same
# fixtures the main repo's own spec-conformance tests use.
# The spec version is NOT hardcoded here: it is read from the repository's
# supported_spec.conf, the same single source of truth the root build and
# tests/CMakeLists.txt use (a bare commit hash here silently drifted from it).
set(_ctoon_spec_conf "${CMAKE_CURRENT_LIST_DIR}/../supported_spec.conf")
if(NOT EXISTS "${_ctoon_spec_conf}")
    message(FATAL_ERROR "benchmarks: ${_ctoon_spec_conf} not found; it defines "
        "the toon-format/spec tag (CTOON_SPEC_TAG) used for the corpus.")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ctoon_spec_conf}")
file(STRINGS "${_ctoon_spec_conf}" _ctoon_spec_lines REGEX "^CTOON_SPEC_TAG=")
if(NOT _ctoon_spec_lines MATCHES "^CTOON_SPEC_TAG=(.+)$")
    message(FATAL_ERROR "benchmarks: no CTOON_SPEC_TAG=<tag> line in ${_ctoon_spec_conf}")
endif()
set(CTOON_SPEC_TAG "${CMAKE_MATCH_1}")
message(STATUS "benchmarks: TOON spec corpus ${CTOON_SPEC_TAG} (from supported_spec.conf)")

FetchContent_Declare(
    toon_format_spec
    GIT_REPOSITORY https://github.com/toon-format/spec.git
    GIT_TAG        ${CTOON_SPEC_TAG}
    # NOT GIT_SHALLOW: a shallow clone only contains the branch tip, so a
    # pinned commit that is no longer the tip fails with "reference is not
    # a tree". The spec repo is tiny, a full clone is cheap.
    GIT_PROGRESS   TRUE
)

# A real "repo full of JSON" for the JSON→TOON conversion benchmark:
# JSON Schema Test Suite. ~540 real JSON files, deeply nested, varied
# shapes - not synthetic data generated just for this benchmark.
FetchContent_Declare(
    json_schema_test_suite
    GIT_REPOSITORY https://github.com/json-schema-org/JSON-Schema-Test-Suite.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(toon_format_spec json_schema_test_suite)

file(GLOB_RECURSE CTOON_BENCH_JSON_SPEC
    ${toon_format_spec_SOURCE_DIR}/tests/fixtures/*.json)
file(GLOB_RECURSE CTOON_BENCH_JSON_SCHEMA
    ${json_schema_test_suite_SOURCE_DIR}/tests/*.json)

set(CTOON_BENCH_CORPUS
    ${CTOON_BENCH_JSON_SPEC}
    ${CTOON_BENCH_JSON_SCHEMA}
)
list(LENGTH CTOON_BENCH_CORPUS CTOON_BENCH_CORPUS_COUNT)

set(CTOON_BENCH_MANIFEST ${CTOON_BENCH_BINARY_DIR}/corpus_manifest.txt)
list(JOIN CTOON_BENCH_CORPUS "\n" CTOON_BENCH_CORPUS_JOINED)
file(WRITE ${CTOON_BENCH_MANIFEST} "${CTOON_BENCH_CORPUS_JOINED}\n")

message(STATUS "CToon benchmarks: corpus manifest has ${CTOON_BENCH_CORPUS_COUNT} "
               "JSON files -> ${CTOON_BENCH_MANIFEST}")

# ── Toolchain detection ────────────────────────────────────────────────────
# A language is skipped (with a warning) only when its toolchain itself
# isn't found. Everything else - venvs, pip installs, go module fetches,
# FetchContent - happens inside that language's own CMakeLists.txt once we
# know the toolchain exists.
find_package(Python3 COMPONENTS Interpreter)
find_program(GO_EXECUTABLE go)
find_program(CARGO_EXECUTABLE cargo)
find_program(RUSTC_EXECUTABLE rustc)
# `pip install ziglang` puts the compiler on PATH as `python-zig`, not `zig`.
find_program(ZIG_EXECUTABLE NAMES zig python-zig)
find_program(JULIA_EXECUTABLE julia)
find_program(NODE_EXECUTABLE node)
find_program(NPM_EXECUTABLE npm)
find_package(Matlab COMPONENTS MAIN_PROGRAM MX_LIBRARY MEX_COMPILER QUIET)
find_program(OCTAVE_EXECUTABLE NAMES octave-cli octave)
find_program(MKOCTFILE_EXECUTABLE mkoctfile)

add_custom_target(ctoon_bench_langs
    COMMENT "Running all available CToon language benchmarks"
)

# C and C++ always run: this project itself requires a C/CXX compiler to
# configure at all, so there's nothing to conditionally detect.
add_subdirectory(c)
add_subdirectory(cpp)

if(Python3_Interpreter_FOUND)
    add_subdirectory(python)
else()
    message(WARNING "Python3 not found - Python benchmark skipped.")
endif()

if(GO_EXECUTABLE)
    execute_process(
        COMMAND ${GO_EXECUTABLE} version
        OUTPUT_VARIABLE CTOON_GO_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX MATCH "go([0-9]+)\\.([0-9]+)" _unused "${CTOON_GO_VERSION_OUTPUT}")
    set(CTOON_GO_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")

    if(CTOON_GO_VERSION VERSION_GREATER_EQUAL "1.23")
        add_subdirectory(go)
    else()
        message(WARNING "Go ${CTOON_GO_VERSION} found, but this benchmark's "
                         "go.mod requires >= 1.23 (needed by toon-go, one of "
                         "the three peer implementations tested together in "
                         "one module) - Go benchmark skipped.")
    endif()
else()
    message(WARNING "Go toolchain not found - Go benchmark skipped. "
                     "Install Go >= 1.23 and make sure 'go' is on PATH.")
endif()

if(Matlab_FOUND)
    add_subdirectory(matlab)
else()
    message(WARNING "MATLAB not found - MATLAB benchmark skipped. "
                     "Set Matlab_ROOT_DIR or the MATLAB_DIR environment variable.")
endif()

# GNU Octave needs both the interpreter and mkoctfile (the MEX gateway is
# compiled with it, at build time only).
if(OCTAVE_EXECUTABLE AND MKOCTFILE_EXECUTABLE)
    add_subdirectory(octave)
else()
    message(WARNING "GNU Octave (octave-cli + mkoctfile) not found - Octave "
                     "benchmark skipped. On Debian/Ubuntu: apt install octave octave-dev.")
endif()

if(CARGO_EXECUTABLE AND RUSTC_EXECUTABLE)
    execute_process(
        COMMAND ${RUSTC_EXECUTABLE} --version
        OUTPUT_VARIABLE CTOON_RUSTC_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX MATCH "rustc ([0-9]+)\\.([0-9]+)" _unused "${CTOON_RUSTC_VERSION_OUTPUT}")
    set(CTOON_RUSTC_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")

    if(CTOON_RUSTC_VERSION VERSION_GREATER_EQUAL "1.87")
        add_subdirectory(rust)
    else()
        message(WARNING "rustc ${CTOON_RUSTC_VERSION} found, but toon-rust "
                         "requires >= 1.87 (uses a stdlib integer method "
                         "stabilized in that release) - Rust benchmark skipped.")
    endif()
else()
    message(WARNING "Rust toolchain (cargo/rustc) not found - Rust benchmark "
                     "skipped. Install Rust >= 1.87 and make sure 'cargo' is on PATH.")
endif()

# zig/build.zig targets Zig 0.16 (the same version the CI uses): Zig 0.17
# removed `b.args`, so the benchmark's build script no longer compiles there.
# `pip install ziglang` installs the newest release, so say so up front
# instead of failing deep inside `zig build`.
set(CTOON_ZIG_USABLE FALSE)
if(ZIG_EXECUTABLE)
    execute_process(COMMAND ${ZIG_EXECUTABLE} version
                    OUTPUT_VARIABLE CTOON_ZIG_VERSION
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE _zig_version_rc)
    if(NOT _zig_version_rc EQUAL 0)
        message(WARNING "'${ZIG_EXECUTABLE} version' failed - Zig benchmark skipped.")
    elseif(CTOON_ZIG_VERSION VERSION_LESS 0.16 OR CTOON_ZIG_VERSION VERSION_GREATER_EQUAL 0.17)
        message(WARNING "Zig ${CTOON_ZIG_VERSION} found, but the Zig benchmark needs Zig 0.16.x - "
                         "Zig benchmark skipped. Install it with: pip install ziglang==0.16.0")
    else()
        set(CTOON_ZIG_USABLE TRUE)
    endif()
endif()

if(CTOON_ZIG_USABLE)
    add_subdirectory(zig)
elseif(ZIG_EXECUTABLE)
    # Already explained above (wrong or unusable version).
else()
    message(WARNING "Zig toolchain not found - Zig benchmark skipped. "
                     "Install Zig >= 0.16.0 and make sure 'zig' is on PATH.")
endif()

if(JULIA_EXECUTABLE)
    add_subdirectory(julia)
else()
    message(WARNING "Julia toolchain not found - Julia benchmark skipped. "
                     "Install Julia and make sure 'julia' is on PATH.")
endif()

# ctoon has no Node.js binding (see benchmarks/node/CMakeLists.txt's own
# comment), so this measures only the reference @toon-format/toon
# package - kept as a standalone reference point, not a ctoon-vs-X
# comparison like every other language here.
if(NODE_EXECUTABLE AND NPM_EXECUTABLE)
    add_subdirectory(node)
else()
    message(WARNING "Node.js toolchain (node/npm) not found - Node.js benchmark "
                     "skipped. Install Node.js and make sure 'node'/'npm' are on PATH.")
endif()

# ── Report: combine every language's JSON into one standalone HTML ───────
# Needs only Python3 + jinja2 (installed into its own throwaway venv) -
# independent of which language toolchains were detected above. Modeled
# on https://github.com/MohammadRaziei/pygixml/tree/main/benchmarks/report.
if(Python3_Interpreter_FOUND)
    add_subdirectory(report)
else()
    message(WARNING "Python3 not found - HTML report skipped (per-language "
                     "JSON results are still written to CTOON_BENCH_RESULTS_DIR).")
endif()

# ── Umbrella target: run every language benchmark, THEN build the report ──
# `ctoon_benchmarks` is the one target people actually run; it fans out to
# ctoon_bench_langs (all per-language benchmarks) and, once those are done,
# ctoon_bench_report (which depends on ctoon_bench_langs itself, so the
# report always renders last, from fresh JSON).
add_custom_target(ctoon_benchmarks
    COMMENT "CToon: running all language benchmarks and building the standalone HTML report"
)
add_dependencies(ctoon_benchmarks ctoon_bench_langs)
if(TARGET ctoon_bench_report)
    add_dependencies(ctoon_benchmarks ctoon_bench_report)
endif()
