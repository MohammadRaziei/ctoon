# ---------------------------------------------------------------------------
# Rust binding tests
# ---------------------------------------------------------------------------
# Mirrors the pattern used by tests/go/CMakeLists.txt. The actual test file
# lives at tests/rust/integration_test.rs (this directory) rather than in
# Cargo's usual in-crate `tests/` folder — src/bindings/rust/Cargo.toml
# points `[[test]] path` there, keeping every language's tests under one
# root `tests/` folder.
# Requires Rust >= 1.70 (this crate's declared rust-version) on PATH.
# ---------------------------------------------------------------------------

set(CTOON_RUST_CRATE_DIR ${PROJECT_SOURCE_DIR}/src/bindings/rust)

# ── Locate Rust toolchain ────────────────────────────────────────────────────
find_program(CARGO_EXECUTABLE cargo)

if(NOT CARGO_EXECUTABLE)
    message(WARNING "Rust toolchain not found. Rust tests will be skipped.")
    message(WARNING "Install Rust >= 1.70 and make sure 'cargo' is on PATH.")
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

# ── ctoon_test_rust custom target ────────────────────────────────────────────
add_custom_target(ctoon_test_rust
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Rust tests..."
    COMMAND ${CARGO_EXECUTABLE} test 
    WORKING_DIRECTORY ${CTOON_RUST_CRATE_DIR}
    USES_TERMINAL
)

add_dependencies(ctoon_test ctoon_test_rust)
