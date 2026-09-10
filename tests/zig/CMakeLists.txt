# ---------------------------------------------------------------------------
# Zig binding tests
# ---------------------------------------------------------------------------
# Mirrors the pattern used by tests/rust/CMakeLists.txt. The integration
# test file lives at tests/zig/integration_test.zig (this directory) rather
# than inside src/bindings/zig, so every language's tests stay under one
# root tests/ folder; src/bindings/zig/build.zig points its "test" step at
# this file directly (no package-manager fetch step needed, same spirit as
# Cargo's [[test]] path in the Rust binding).
# Requires Zig >= 0.13 on PATH.
# ---------------------------------------------------------------------------

set(CTOON_ZIG_BINDING_DIR ${PROJECT_SOURCE_DIR}/src/bindings/zig)

# ── Locate Zig toolchain ─────────────────────────────────────────────────────
find_program(ZIG_EXECUTABLE zig)

if(NOT ZIG_EXECUTABLE)
    message(WARNING "Zig toolchain not found. Zig tests will be skipped.")
    message(WARNING "Install Zig >= 0.13 and make sure 'zig' is on PATH.")
    return()
endif()

execute_process(
    COMMAND ${ZIG_EXECUTABLE} version
    OUTPUT_VARIABLE ZIG_VERSION_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Found Zig: ${ZIG_VERSION_OUTPUT}")

# ── CTest integration ────────────────────────────────────────────────────────
add_test(
    NAME ctoon_zig_tests
    COMMAND ${ZIG_EXECUTABLE} build test
    WORKING_DIRECTORY ${CTOON_ZIG_BINDING_DIR}
)
set_tests_properties(ctoon_zig_tests PROPERTIES TIMEOUT 180)

# ── ctoon_test_zig custom target ─────────────────────────────────────────────
add_custom_target(ctoon_test_zig
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Zig tests..."
    COMMAND ${ZIG_EXECUTABLE} build test
    WORKING_DIRECTORY ${CTOON_ZIG_BINDING_DIR}
    USES_TERMINAL
)

add_dependencies(ctoon_test ctoon_test_zig)
