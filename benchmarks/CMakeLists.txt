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
set(CMAKE_CXX_STANDARD 17)
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

# ── Per-tool network proxies ───────────────────────────────────────────────
# Each is empty by default = nothing changes for that tool. Set one and only
# that tool's network steps run through it, e.g.
#   cmake -S . -B build-bench -DUV_PROXY=http://127.0.0.1:8080 -DJULIA_PROXY=socks5h://127.0.0.1:1080
# (remembered in the CMake cache; no global tool config is touched).
include(${CMAKE_CURRENT_LIST_DIR}/cmake/Proxy.cmake)
set(UV_PROXY    "" CACHE STRING "Proxy for uv (venvs, Python and ziglang downloads, ctoon git install) and the PyPI check")
set(GIT_PROXY   "" CACHE STRING "Proxy for the configure-time git clones (FetchContent: corpus, ctoon, toonc)")
set(GO_PROXY    "" CACHE STRING "Proxy for go get / go mod tidy (not the Go module proxy GOPROXY)")
set(CARGO_PROXY "" CACHE STRING "Proxy for cargo (crates.io and git dependencies)")
set(NPM_PROXY   "" CACHE STRING "Proxy for npm install")
set(JULIA_PROXY "" CACHE STRING "Proxy for Julia Pkg (registry, packages, git)")
set(ZIG_PROXY   "" CACHE STRING "Proxy for zig fetch and its git fallback")
foreach(_p UV GIT GO CARGO NPM JULIA ZIG)
    if(${_p}_PROXY)
        # hide user:password@ from the log
        string(REGEX REPLACE "//[^@/]*@" "//***@" _shown "${${_p}_PROXY}")
        message(STATUS "[config] proxy ${_p}: ${_shown}")
    endif()
endforeach()
ctoon_bench_proxy_env(CTOON_UV_PROXY_ENV    "${UV_PROXY}")
ctoon_bench_proxy_env(CTOON_GO_PROXY_ENV    "${GO_PROXY}")
ctoon_bench_proxy_env(CTOON_CARGO_PROXY_ENV "${CARGO_PROXY}" CARGO_HTTP_PROXY)
ctoon_bench_proxy_env(CTOON_NPM_PROXY_ENV   "${NPM_PROXY}" npm_config_proxy npm_config_https_proxy)
ctoon_bench_proxy_env(CTOON_JULIA_PROXY_ENV "${JULIA_PROXY}")
if(JULIA_PROXY)  # Pkg's built-in libgit2 only knows http(s) proxies; the git CLI also does socks5/socks5h
    list(APPEND CTOON_JULIA_PROXY_ENV JULIA_PKG_USE_CLI_GIT=true)
endif()
ctoon_bench_proxy_env(CTOON_ZIG_PROXY_ENV   "${ZIG_PROXY}")
# FetchContent runs git inside this very configure process: put the proxy in its environment.
ctoon_bench_proxy_env(_ctoon_git_env "${GIT_PROXY}")
foreach(_e IN LISTS _ctoon_git_env)
    if(_e MATCHES "^([^=]+)=(.*)$")
        set(ENV{${CMAKE_MATCH_1}} "${CMAKE_MATCH_2}")
    endif()
endforeach()

# ── Corpus: fetch real-world JSON to benchmark against ────────────────────
include(FetchContent)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/BenchLog.cmake)
# Show git's own clone/fetch progress instead of hiding it until the end.
set(FETCHCONTENT_QUIET OFF)

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

ctoon_bench_fetch_note(toon_format_spec https://github.com/toon-format/spec.git ${CTOON_SPEC_TAG})
ctoon_bench_fetch_note(json_schema_test_suite https://github.com/json-schema-org/JSON-Schema-Test-Suite.git main)
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
# uv creates and fills every venv this suite uses: the Python benchmark's and
# the Zig compilers' (Zig is installed from PyPI as `ziglang`, never taken
# from the system).
find_program(UV_EXECUTABLE uv)
set(CTOON_BENCH_PYTHON_VERSION "3.12" CACHE STRING
    "Python version of every venv the benchmarks create (uv venv --python <version>)")

# Optional PyPI mirror for every `uv pip install` of every venv here, e.g.
# -DCTOON_PYPI_INDEX=https://mirror-pypi.runflare.com/simple. Applies only to
# those venvs - no global pip/uv config is touched. Empty = default PyPI.
set(CTOON_PYPI_INDEX "" CACHE STRING
    "PyPI index URL for the benchmark venvs (empty = default PyPI)")
set(CTOON_UV_INDEX_ARGS "")
set(CTOON_PYPI_INDEX_SHOWN "https://pypi.org/simple - default")
set(_ctoon_pypi_probe "https://pypi.org/simple/pip/")
set(_ctoon_pypi_insecure OFF)
if(CTOON_PYPI_INDEX)
    list(APPEND CTOON_UV_INDEX_ARGS --index-url ${CTOON_PYPI_INDEX})
    set(CTOON_PYPI_INDEX_SHOWN "${CTOON_PYPI_INDEX} - from -DCTOON_PYPI_INDEX")
    string(REGEX REPLACE "/+$" "" _ctoon_pypi_probe "${CTOON_PYPI_INDEX}")
    set(_ctoon_pypi_probe "${_ctoon_pypi_probe}/pip/")
    # Like pip's `--trusted-host`: mirrors often have a certificate uv rejects, which
    # breaks the install. Extract the host from the URL and allow it, for this mirror only.
    # ponytail: on by default for a mirror, one switch, no per-host list.
    option(CTOON_PYPI_TRUSTED_HOST "Trust the TLS certificate of the CTOON_PYPI_INDEX host (uv --allow-insecure-host)" ON)
    if(CTOON_PYPI_TRUSTED_HOST AND CTOON_PYPI_INDEX MATCHES "^https?://([^/:]+(:[0-9]+)?)")
        set(_ctoon_pypi_host "${CMAKE_MATCH_1}")
        list(APPEND CTOON_UV_INDEX_ARGS --allow-insecure-host ${_ctoon_pypi_host})
        set(CTOON_PYPI_INDEX_SHOWN "${CTOON_PYPI_INDEX_SHOWN}, trusted host ${_ctoon_pypi_host}")
        set(_ctoon_pypi_insecure ON)
    endif()
endif()

# uv network behaviour for every venv here. uv's own defaults (30 s x 3 retries
# per request, or whatever UV_HTTP_* is set to in the shell, which wins) make
# an unreachable index look like a hang; these fail in about a minute instead.
set(CTOON_UV_HTTP_TIMEOUT "20" CACHE STRING "uv HTTP timeout in seconds (UV_HTTP_TIMEOUT) unless already set in the environment")
set(CTOON_UV_HTTP_RETRIES "3" CACHE STRING "uv HTTP retries (UV_HTTP_RETRIES) unless already set in the environment")
option(CTOON_BENCH_VERBOSE "Pass -v to uv so every HTTP request (index, files) is printed" OFF)
option(CTOON_BENCH_NET_CHECK "Check the PyPI index is reachable before the venv steps (fails in seconds instead of minutes)" ON)
set(CTOON_UV_ENV "")
if(NOT DEFINED ENV{UV_HTTP_TIMEOUT})
    list(APPEND CTOON_UV_ENV UV_HTTP_TIMEOUT=${CTOON_UV_HTTP_TIMEOUT})
endif()
if(NOT DEFINED ENV{UV_HTTP_RETRIES})
    list(APPEND CTOON_UV_ENV UV_HTTP_RETRIES=${CTOON_UV_HTTP_RETRIES})
endif()
list(APPEND CTOON_UV_ENV ${CTOON_UV_PROXY_ENV})
set(CTOON_UV_CMD ${CMAKE_COMMAND} -E env ${CTOON_UV_ENV} ${UV_EXECUTABLE})
set(CTOON_UV_PIP_ARGS "")
if(CTOON_BENCH_VERBOSE)
    list(APPEND CTOON_UV_PIP_ARGS -v)
endif()

if(UV_EXECUTABLE)
    message(STATUS "[config] uv: ${UV_EXECUTABLE}, Python ${CTOON_BENCH_PYTHON_VERSION}, PyPI index: ${CTOON_PYPI_INDEX_SHOWN}")
    set(_ctoon_net_hint "uv would not reach it either. Either set a mirror: cmake -S . -B build-bench -DCTOON_PYPI_INDEX=https://mirror-pypi.runflare.com/simple (remembered in the CMake cache), or set a proxy: -DUV_PROXY=http://host:port, or skip this check with -DCTOON_BENCH_NET_CHECK=OFF.")
    if(CTOON_BENCH_NET_CHECK)
        add_custom_target(ctoon_bench_net_pypi
            COMMAND ${CMAKE_COMMAND} -DURL=${_ctoon_pypi_probe} "-DLABEL=PyPI index" -DTMP_FILE=${CTOON_BENCH_BINARY_DIR}/net_probe.tmp -DINSECURE=${_ctoon_pypi_insecure} "-DPROXY=${UV_PROXY}"
                    "-DHINT=${_ctoon_net_hint}" -P ${CMAKE_CURRENT_LIST_DIR}/cmake/CheckUrl.cmake
            VERBATIM
        )
    else()
        add_custom_target(ctoon_bench_net_pypi COMMAND ${CMAKE_COMMAND} -E echo "-- [net] PyPI reachability check skipped - CTOON_BENCH_NET_CHECK=OFF")
    endif()
endif()

find_program(JULIA_EXECUTABLE julia)
find_program(NODE_EXECUTABLE node)
find_program(NPM_EXECUTABLE npm)
find_package(Matlab COMPONENTS MAIN_PROGRAM MX_LIBRARY MEX_COMPILER QUIET)
find_program(OCTAVE_EXECUTABLE NAMES octave-cli octave)
find_program(MKOCTFILE_EXECUTABLE mkoctfile)

add_custom_target(ctoon_bench_langs
    COMMENT "Running all available CToon language benchmarks"
)

# The fetched ctoon only defines its MATLAB / Octave MEX targets when asked
# (CTOON_BUILD_MATLAB / CTOON_BUILD_OCTAVE, see cmake/CtoonLangGuard.cmake),
# and it is configured exactly once, by the first add_subdirectory() below
# (c/), so these must be set before that. Only when the toolchain was found
# here: a missing one stays a warn-and-skip instead of becoming a FATAL_ERROR
# inside the fetched project.
if(Matlab_FOUND)
    set(CTOON_BUILD_MATLAB ON)
endif()
if(OCTAVE_EXECUTABLE AND MKOCTFILE_EXECUTABLE)
    set(CTOON_BUILD_OCTAVE ON)
endif()

# C and C++ always run: this project itself requires a C/CXX compiler to
# configure at all, so there's nothing to conditionally detect.
add_subdirectory(c)
add_subdirectory(cpp)

if(UV_EXECUTABLE)
    add_subdirectory(python)
else()
    message(WARNING "uv not found - Python benchmark skipped. Install uv (https://docs.astral.sh/uv/).")
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

# Zig (zig/: the ctoon benchmark with Zig 0.16.x and the toon-zig benchmark
# with Zig 0.15.2) is installed from PyPI by uv into private venvs, see
# zig/CMakeLists.txt. Target: ctoon_benchmarks_zig.
if(UV_EXECUTABLE)
    add_subdirectory(zig)
else()
    message(WARNING "uv not found - Zig benchmark skipped (it installs Zig from PyPI with uv). "
                     "Install uv (https://docs.astral.sh/uv/).")
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

# ── Results: system info, optional memory, and copying the JSON out ──────
# Needs only Python3 (stdlib) - independent of which language toolchains
# were detected above. The HTML page is NOT built here: the docs build
# renders it from results/*.json (docs/benchmarks/render_benchmarks.py).
if(Python3_Interpreter_FOUND)
    add_subdirectory(report)
else()
    message(WARNING "Python3 not found - system info and the results copy are skipped "
                     "(per-language JSON is still written to CTOON_BENCH_RESULTS_DIR).")
endif()

# ── Umbrella target: run every language benchmark, THEN copy the JSON ─────
# `ctoon_benchmarks` is the one target people actually run; it fans out to
# ctoon_bench_langs (all per-language benchmarks) and, once those are done,
# ctoon_bench_report (which depends on ctoon_bench_langs itself, so the
# copy always runs last, from fresh JSON).
add_custom_target(ctoon_benchmarks
    COMMENT "CToon: running all language benchmarks and writing results/*.json"
)
# First dependency on purpose: an unreachable PyPI is reported before any
# language has been built, not minutes into the run.
if(TARGET ctoon_bench_net_pypi)
    add_dependencies(ctoon_benchmarks ctoon_bench_net_pypi)
endif()
add_dependencies(ctoon_benchmarks ctoon_bench_langs)
if(TARGET ctoon_bench_report)
    add_dependencies(ctoon_benchmarks ctoon_bench_report)
endif()
