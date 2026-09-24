# ---------------------------------------------------------------------------
# Rust binding tests
# ---------------------------------------------------------------------------
# Mirrors the pattern used by tests/go/CMakeLists.txt. The actual test file
# lives at tests/rust/integration_test.rs (this directory) rather than in
# Cargo's usual in-crate `tests/` folder — the root Cargo.toml's
# `[[test]] path` points there, keeping every language's tests under one
# root `tests/` folder.
# Requires Rust >= 1.70 (this crate's declared rust-version) on PATH.
# ---------------------------------------------------------------------------

set(CTOON_RUST_CRATE_DIR ${PROJECT_SOURCE_DIR}/src/bindings/rust)

# ── Locate Rust toolchain ────────────────────────────────────────────────────
find_program(CARGO_EXECUTABLE cargo)

ctoon_require_lang(CTOON_BUILD_RUST CARGO_EXECUTABLE "Rust"
    "Install Rust >= 1.70 and make sure 'cargo' is on PATH.")

if(NOT CARGO_EXECUTABLE)
    return()
endif()

execute_process(
    COMMAND ${CARGO_EXECUTABLE} --version
    OUTPUT_VARIABLE CARGO_VERSION_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Found Cargo: ${CARGO_VERSION_OUTPUT}")

# ── CTest integration ────────────────────────────────────────────────────────
add_test(
    NAME ctoon_rust_tests
    COMMAND ${CARGO_EXECUTABLE} test 
    WORKING_DIRECTORY ${CTOON_RUST_CRATE_DIR}
)
set_tests_properties(ctoon_rust_tests PROPERTIES TIMEOUT 180)
if(CTOON_SPEC_FIXTURES_DIR)
    # Set centrally in tests/CMakeLists.txt (FetchContent of toon-format/spec).
    # Empty when the fetch didn't run -- tests/rust/native_json_roundtrip_test.rs
    # must skip (early-return, not panic) when unset, same convention as
    # tests/python/conftest.py's spec_fixtures_dir fixture.
    set_tests_properties(ctoon_rust_tests PROPERTIES
        ENVIRONMENT "CTOON_SPEC_FIXTURES_DIR=${CTOON_SPEC_FIXTURES_DIR};CTOON_SPEC_EXAMPLES_DIR=${CTOON_SPEC_EXAMPLES_DIR}")
endif()

# ── ctoon_test_rust custom target ────────────────────────────────────────────
add_custom_target(ctoon_test_rust
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Rust tests..."
    COMMAND ${CMAKE_COMMAND} -E env
            "CTOON_SPEC_FIXTURES_DIR=${CTOON_SPEC_FIXTURES_DIR}"
            "CTOON_SPEC_EXAMPLES_DIR=${CTOON_SPEC_EXAMPLES_DIR}"
            ${CARGO_EXECUTABLE} test 
    WORKING_DIRECTORY ${CTOON_RUST_CRATE_DIR}
    USES_TERMINAL
)

add_dependencies(ctoon_test ctoon_test_rust)

# ── ctoon_coverage_rust custom target ────────────────────────────────────────
# Uses cargo-llvm-cov (https://github.com/taiki-e/cargo-llvm-cov), which
# outputs lcov natively — unlike Go's `go tool cover` output, no format
# conversion script is needed here (compare tests/go/CMakeLists.txt's
# GoToLcov.cmake step).
set(CTOON_COVERAGE_RUST_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/rust)

execute_process(
    COMMAND ${CARGO_EXECUTABLE} llvm-cov --version
    OUTPUT_QUIET
    ERROR_QUIET
    RESULT_VARIABLE CARGO_LLVM_COV_CHECK
)

if(CARGO_LLVM_COV_CHECK EQUAL 0)
    file(MAKE_DIRECTORY ${CTOON_COVERAGE_RUST_BINARY_DIR})

    add_custom_target(ctoon_coverage_rust
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CTOON_COVERAGE_RUST_BINARY_DIR}/html
        COMMAND ${CMAKE_COMMAND} -E make_directory   ${CTOON_COVERAGE_RUST_BINARY_DIR}/html

        # lcov output (for merge with C/C++/Python/Go) — reruns the test
        # suite under instrumentation; a second run below does the same
        # for the HTML report, since cargo-llvm-cov doesn't emit both
        # report formats from a single invocation.
        COMMAND ${CMAKE_COMMAND} -E echo "-- Capturing Rust coverage as lcov..."
        COMMAND ${CARGO_EXECUTABLE} llvm-cov --lcov
                    --output-path ${CTOON_COVERAGE_RUST_BINARY_DIR}/coverage.lcov
        COMMAND ${CMAKE_COMMAND}
                    -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
                    -DCOV_FILE=${CTOON_COVERAGE_RUST_BINARY_DIR}/coverage.lcov
                    -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake

        # HTML report
        COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report..."
        COMMAND ${CARGO_EXECUTABLE} llvm-cov --html
                    --output-dir ${CTOON_COVERAGE_RUST_BINARY_DIR}/html-raw

        # cargo-llvm-cov's --html writes to <dir>/html/index.html itself;
        # flatten that one level so the layout matches every other
        # language's coverage dir (<lang>/html/index.html).
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CTOON_COVERAGE_RUST_BINARY_DIR}/html-raw/html
                    ${CTOON_COVERAGE_RUST_BINARY_DIR}/html
        COMMAND ${CMAKE_COMMAND} -E remove_directory
                    ${CTOON_COVERAGE_RUST_BINARY_DIR}/html-raw

        COMMAND ${CMAKE_COMMAND} -E echo
                "-- HTML entry point: ${CTOON_COVERAGE_RUST_BINARY_DIR}/html/index.html"

        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        USES_TERMINAL
    )

    add_dependencies(ctoon_coverage_rust ctoon_test_rust)
    add_dependencies(ctoon_coverage ctoon_coverage_rust)

    add_custom_command(TARGET ctoon_coverage_rust POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DIFRAME_SRC="html/index.html"
            -DSECTION_TITLE="Rust%20Bindings"
            -DLOGO_SVG="${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg"
            -DPROJECT_NAME="${PROJECT_NAME}"
            -DOUTPUT_DIR="${CTOON_COVERAGE_RUST_BINARY_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/GenerateIframe.cmake"
        COMMENT "Generating iframe wrapper for Rust coverage report"
        WORKING_DIRECTORY ${CTOON_TEST_SOURCE_DIR}
    )
else()
    message(WARNING "cargo-llvm-cov not found; Rust coverage target will be skipped. "
                     "Install with: cargo install cargo-llvm-cov "
                     "(also needs 'rustup component add llvm-tools-preview').")
endif()
