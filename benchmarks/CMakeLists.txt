cmake_minimum_required(VERSION 3.19)

# ==============================================================================
# CToon Benchmarks — fully standalone CMake project.
#
# This has NO relationship to the repository root — not even for ctoon
# itself. Every language folder fetches ctoon the exact same way it fetches
# any competitor: from https://github.com/mohammadraziei/ctoon.git (C/C++),
# `go get github.com/mohammadraziei/ctoon` (Go), or
# `pip install git+https://github.com/mohammadraziei/ctoon.git` (Python).
# ctoon is never a special case here — it's benchmarked exactly like
# gotoon, toon-go, or TOONc: one more entry in the same results table.
#
# Build it on its own:
#
#   cd benchmarks
#   cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-bench --target ctoon_benchmarks
#
# Layout mirrors tests/: one folder per language (c, cpp, python, go,
# matlab), each with its own CMakeLists.txt that fetches its own
# dependencies and defines a ctoon_benchmarks_<lang> target. A language is
# skipped with a warning only when its toolchain isn't found — see
# "Toolchain detection" below.
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
    message(WARNING "Benchmarks are being built in Debug mode — numbers will "
                     "not reflect real performance. Pass -DCMAKE_BUILD_TYPE=Release.")
endif()

set(CTOON_BENCH_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CTOON_BENCH_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CTOON_GITHUB_URL "https://github.com/mohammadraziei/ctoon.git")

# Every language benchmark writes its own JSON results file here — kept
# separate per language rather than merged, since each language's set of
# libraries/operations differs.
set(CTOON_BENCH_RESULTS_DIR ${CTOON_BENCH_BINARY_DIR}/results)
file(MAKE_DIRECTORY ${CTOON_BENCH_RESULTS_DIR})

# ── Corpus: fetch real-world JSON to benchmark against ────────────────────
include(FetchContent)

# Small, varied JSON shapes covering the format's edge cases — the same
# fixtures the main repo's own spec-conformance tests use.
FetchContent_Declare(
    toon_format_spec
    GIT_REPOSITORY https://github.com/toon-format/spec.git
    GIT_TAG        d6db4b04303bdea132351ce45aed612311c850b2
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

# A real "repo full of JSON" for the JSON→TOON conversion benchmark:
# JSON Schema Test Suite. ~540 real JSON files, deeply nested, varied
# shapes — not synthetic data generated just for this benchmark.
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
# isn't found. Everything else — venvs, pip installs, go module fetches,
# FetchContent — happens inside that language's own CMakeLists.txt once we
# know the toolchain exists.
find_package(Python3 COMPONENTS Interpreter)
find_program(GO_EXECUTABLE go)
find_program(CARGO_EXECUTABLE cargo)
find_program(RUSTC_EXECUTABLE rustc)
find_package(Matlab COMPONENTS MAIN_PROGRAM MX_LIBRARY MEX_COMPILER QUIET)

add_custom_target(ctoon_benchmarks
    COMMENT "Running all available CToon language benchmarks"
)

# C and C++ always run: this project itself requires a C/CXX compiler to
# configure at all, so there's nothing to conditionally detect.
add_subdirectory(c)
add_subdirectory(cpp)

if(Python3_Interpreter_FOUND)
    add_subdirectory(python)
else()
    message(WARNING "Python3 not found — Python benchmark skipped.")
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
                         "one module) — Go benchmark skipped.")
    endif()
else()
    message(WARNING "Go toolchain not found — Go benchmark skipped. "
                     "Install Go >= 1.23 and make sure 'go' is on PATH.")
endif()

if(Matlab_FOUND)
    add_subdirectory(matlab)
else()
    message(WARNING "MATLAB not found — MATLAB benchmark skipped. "
                     "Set Matlab_ROOT_DIR or the MATLAB_DIR environment variable.")
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
                         "stabilized in that release) — Rust benchmark skipped.")
    endif()
else()
    message(WARNING "Rust toolchain (cargo/rustc) not found — Rust benchmark "
                     "skipped. Install Rust >= 1.87 and make sure 'cargo' is on PATH.")
endif()
