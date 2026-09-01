cmake_minimum_required(VERSION 3.19)

# ==============================================================================
# CToon Benchmarks — standalone CMake project.
#
# This is NOT add_subdirectory()'d by the root CMakeLists.txt. It runs the
# other way around: this project pulls the ctoon repo root in as a
# subdirectory to get the ctoon::ctoon / ctoon::ctoonpp targets. Build it on
# its own:
#
#   cmake -S benchmarks -B build-bench -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-bench --target ctoon_benchmark
#
# Each language gets its own folder (mirrors tests/: c, cpp, python, go,
# matlab), each with its own CMakeLists.txt. A language is skipped with a
# warning if its toolchain isn't found on this machine — but if the
# toolchain IS present and a per-language dependency is missing (pip
# packages, etc.), that dependency is installed automatically rather than
# skipping.
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
set(CTOON_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# ── Pull in the ctoon project root (the "reverse" link) ──────────────────
# CTOON_BUILD_TESTS/EXAMPLES/PYTHON/DOCS all default to
# ${PROJECT_IS_TOP_LEVEL}, which CMake resolves to FALSE for a project
# included via add_subdirectory() — so this brings in just the ctoon /
# ctoon::ctoonpp libraries, none of the root project's own tests/docs.
add_subdirectory(${CTOON_ROOT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/ctoon_root)

# ── Corpus: fetch real-world JSON to benchmark against ────────────────────
include(FetchContent)

# Same pinned checkout tests/cpp uses for spec conformance — reused here so
# the correctness fixtures double as benchmark input (small, varied JSON
# shapes covering the format's edge cases).
FetchContent_Declare(
    toon_format_spec
    GIT_REPOSITORY https://github.com/toon-format/spec.git
    GIT_TAG        d6db4b04303bdea132351ce45aed612311c850b2
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

# A real "repo full of JSON" for the JSON→TOON conversion benchmark, as
# requested: JSON Schema Test Suite. ~540 real JSON files, deeply nested,
# varied shapes (arrays, objects, edge-case values) — not synthetic data
# generated just for this benchmark.
FetchContent_Declare(
    json_schema_test_suite
    GIT_REPOSITORY https://github.com/json-schema-org/JSON-Schema-Test-Suite.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(toon_format_spec json_schema_test_suite)

# ── Build the corpus manifest ──────────────────────────────────────────────
# One shared list of JSON file paths, generated once at configure time, so
# every language benchmark reads the exact same corpus — required for the
# throughput numbers to be comparable across languages.
file(GLOB_RECURSE CTOON_BENCH_JSON_SPEC
    ${toon_format_spec_SOURCE_DIR}/tests/fixtures/*.json)
file(GLOB_RECURSE CTOON_BENCH_JSON_SCHEMA
    ${json_schema_test_suite_SOURCE_DIR}/tests/*.json)
file(GLOB CTOON_BENCH_JSON_LOCAL
    ${CTOON_ROOT_DIR}/tests/data/*.json)

set(CTOON_BENCH_CORPUS
    ${CTOON_BENCH_JSON_LOCAL}
    ${CTOON_BENCH_JSON_SPEC}
    ${CTOON_BENCH_JSON_SCHEMA}
)
list(LENGTH CTOON_BENCH_CORPUS CTOON_BENCH_CORPUS_COUNT)

set(CTOON_BENCH_MANIFEST ${CTOON_BENCH_BINARY_DIR}/corpus_manifest.txt)
list(JOIN CTOON_BENCH_CORPUS "\n" CTOON_BENCH_CORPUS_JOINED)
file(WRITE ${CTOON_BENCH_MANIFEST} "${CTOON_BENCH_CORPUS_JOINED}\n")

message(STATUS "CToon benchmarks: corpus manifest has ${CTOON_BENCH_CORPUS_COUNT} "
               "JSON files -> ${CTOON_BENCH_MANIFEST}")

# ── Per-language toolchain detection ──────────────────────────────────────
# Presence of a toolchain is the only thing that gets a language skipped.
# A missing per-language *dependency* (pip package, etc.) is installed
# automatically instead — see each language's CMakeLists.txt.
find_package(Python3 COMPONENTS Interpreter)
find_program(GO_EXECUTABLE go)
find_package(Matlab COMPONENTS MAIN_PROGRAM MX_LIBRARY MEX_COMPILER QUIET)

add_custom_target(ctoon_benchmark
    COMMENT "Running all available CToon language benchmarks"
)

add_subdirectory(c)
add_subdirectory(cpp)

if(Python3_Interpreter_FOUND)
    add_subdirectory(python)
else()
    message(WARNING "Python3 not found — Python benchmark skipped.")
endif()

if(GO_EXECUTABLE)
    add_subdirectory(go)
else()
    message(WARNING "Go toolchain not found — Go benchmark skipped. "
                     "Install Go >= 1.21 and make sure 'go' is on PATH.")
endif()

if(Matlab_FOUND)
    add_subdirectory(matlab)
else()
    message(WARNING "MATLAB not found — MATLAB benchmark skipped. "
                     "Set Matlab_ROOT_DIR or the MATLAB_DIR environment variable.")
endif()
