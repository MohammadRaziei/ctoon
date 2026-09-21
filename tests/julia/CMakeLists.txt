# ---------------------------------------------------------------------------
# Julia binding tests
# ---------------------------------------------------------------------------
# Mirrors the pattern used by tests/rust/CMakeLists.txt and
# tests/zig/CMakeLists.txt. The binding itself (CToon.jl) lives under
# src/bindings/julia (like the other bindings), but its test file stays
# under this root tests/ folder — same split every other language here
# uses. No Julia code is compiled by this project's own CMake graph
# (Julia's `deps/build.jl` handles that itself, the same way Cargo/zig
# build do for their own bindings); this file only drives it via CTest.
# Requires Julia on PATH.
# ---------------------------------------------------------------------------

set(CTOON_JULIA_BINDING_DIR ${PROJECT_SOURCE_DIR}/src/bindings/julia)

# A local, throwaway Julia depot (registries, packages, compiled cache,
# artifacts -- everything Julia would otherwise put in ~/.julia)
# instead of the user's real one -- same idea as a Python venv, and the
# same thing benchmarks/julia/CMakeLists.txt does for the benchmark
# suite's own Julia environment. Nothing the test suite or coverage
# target below does ever touches ~/.julia; `rm -rf build` wipes every
# trace of it, General registry download included.
set(CTOON_JULIA_DEPOT ${CMAKE_CURRENT_BINARY_DIR}/depot)
file(MAKE_DIRECTORY ${CTOON_JULIA_DEPOT})
set(CTOON_JULIA_ENV ${CMAKE_COMMAND} -E env JULIA_DEPOT_PATH=${CTOON_JULIA_DEPOT})

# ── Locate Julia toolchain ───────────────────────────────────────────────────
find_program(JULIA_EXECUTABLE julia)

ctoon_require_lang(CTOON_BUILD_JULIA JULIA_EXECUTABLE "Julia"
    "Install Julia (https://julialang.org/downloads/) and make sure 'julia' is on PATH.")

if(NOT JULIA_EXECUTABLE)
    return()
endif()

execute_process(
    COMMAND ${JULIA_EXECUTABLE} --version
    OUTPUT_VARIABLE JULIA_VERSION_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Found Julia: ${JULIA_VERSION_OUTPUT}")

# ── Build the shim + run tests via run_tests.jl (see that file's ───────────
# comment for why this is a real script rather than an inline `-e`
# string) — Pkg.instantiate() + deps/build.jl + test/runtests.jl, same
# two-step shape as Zig's `zig build test` implicitly does in one command.
set(CTOON_JULIA_RUN_TESTS
    ${CTOON_JULIA_ENV}
    ${JULIA_EXECUTABLE} --project=${CTOON_JULIA_BINDING_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.jl ${CTOON_JULIA_BINDING_DIR}
)

# ── CTest integration ────────────────────────────────────────────────────────
add_test(
    NAME ctoon_julia_tests
    COMMAND ${CTOON_JULIA_RUN_TESTS}
    WORKING_DIRECTORY ${CTOON_JULIA_BINDING_DIR}
)
set_tests_properties(ctoon_julia_tests PROPERTIES TIMEOUT 180)

# ── ctoon_test_julia custom target ───────────────────────────────────────────
add_custom_target(ctoon_test_julia
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Julia tests..."
    COMMAND ${CTOON_JULIA_RUN_TESTS}
    WORKING_DIRECTORY ${CTOON_JULIA_BINDING_DIR}
    USES_TERMINAL
)

add_dependencies(ctoon_test ctoon_test_julia)

# ── ctoon_coverage_julia custom target ───────────────────────────────────────
# Uses Coverage.jl (https://github.com/JuliaTesting/Coverage.jl), the
# standard tool in the Julia ecosystem — mature enough to implement this
# properly, unlike Zig's coverage situation (see tests/zig/CMakeLists.txt's
# comment for why that one's still deliberately skipped). Coverage.jl
# writes native .lcov output, same as cargo-llvm-cov, so no format
# conversion script is needed here either (compare tests/go/CMakeLists.txt's
# GoToLcov.cmake step).
#
# Note on scope: `--code-coverage` instruments the .jl source lines this
# binding's own Julia code runs (src/CToon.jl) — same caveat as any FFI
# binding's coverage (Rust's included): it doesn't reach into the C side
# behind the ccalls, which the C/C++ coverage targets already cover.
set(CTOON_COVERAGE_JULIA_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/julia)

# Deliberately NOT run against the isolated CTOON_JULIA_DEPOT above:
# Coverage.jl here is a globally pre-installed dev tool (same footing
# as GENHTML_EXECUTABLE/lcov, found via find_program against the
# system, not vendored), not a project dependency declared anywhere —
# the warning below even tells people to `Pkg.add("Coverage")` globally.
# Isolating this check would make it always fail (nothing's ever
# installed in a brand new depot) and silently break coverage for
# everyone who already has it set up.
execute_process(
    COMMAND ${JULIA_EXECUTABLE} -e "using Coverage"
    OUTPUT_QUIET
    ERROR_QUIET
    RESULT_VARIABLE JULIA_COVERAGE_CHECK
)

if(JULIA_COVERAGE_CHECK EQUAL 0)
    file(MAKE_DIRECTORY ${CTOON_COVERAGE_JULIA_BINARY_DIR})

    add_custom_target(ctoon_coverage_julia
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CTOON_COVERAGE_JULIA_BINARY_DIR}/html
        COMMAND ${CMAKE_COMMAND} -E make_directory   ${CTOON_COVERAGE_JULIA_BINARY_DIR}/html

        # Run the test suite instrumented (--code-coverage=user writes
        # per-source .cov files next to src/CToon.jl), then have
        # Coverage.jl fold those into one lcov file and clean up the
        # raw .cov files it leaves behind. Also deliberately not using
        # CTOON_JULIA_ENV/CTOON_JULIA_RUN_TESTS here, for the same
        # reason as the "using Coverage" check above.
        COMMAND ${CMAKE_COMMAND} -E echo "-- Capturing Julia coverage as lcov..."
        COMMAND ${JULIA_EXECUTABLE} --project=${CTOON_JULIA_BINDING_DIR} --code-coverage=user
                    ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.jl ${CTOON_JULIA_BINDING_DIR}
        COMMAND ${JULIA_EXECUTABLE} --project=${CTOON_JULIA_BINDING_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/run_coverage_report.jl ${CTOON_COVERAGE_JULIA_BINARY_DIR}/coverage.lcov ${CTOON_JULIA_BINDING_DIR}

        COMMAND ${CMAKE_COMMAND}
                    -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
                    -DCOV_FILE=${CTOON_COVERAGE_JULIA_BINARY_DIR}/coverage.lcov
                    -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake

        # HTML report
        COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report..."
        COMMAND ${GENHTML_EXECUTABLE} --quiet
                    ${CTOON_COVERAGE_JULIA_BINARY_DIR}/coverage.lcov
                    --output-directory ${CTOON_COVERAGE_JULIA_BINARY_DIR}/html
                    --prefix ${PROJECT_SOURCE_DIR}
                    --title "CToon Julia Binding Coverage"

        COMMAND ${CMAKE_COMMAND} -E echo
                "-- HTML entry point: ${CTOON_COVERAGE_JULIA_BINARY_DIR}/html/index.html"

        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        USES_TERMINAL
    )

    add_dependencies(ctoon_coverage_julia ctoon_test_julia)
    add_dependencies(ctoon_coverage ctoon_coverage_julia)

    add_custom_command(TARGET ctoon_coverage_julia POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DIFRAME_SRC="html/index.html"
            -DSECTION_TITLE="Julia%20Bindings"
            -DLOGO_SVG="${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg"
            -DPROJECT_NAME="${PROJECT_NAME}"
            -DOUTPUT_DIR="${CTOON_COVERAGE_JULIA_BINARY_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/GenerateIframe.cmake"
        COMMENT "Generating iframe wrapper for Julia coverage report"
        WORKING_DIRECTORY ${CTOON_TEST_SOURCE_DIR}
    )
else()
    message(WARNING "Coverage.jl not found; Julia coverage target will be skipped. "
                     "Install with: julia -e 'import Pkg; Pkg.add(\"Coverage\")'")
endif()
