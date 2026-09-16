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

# ── Build the shim + test shell out to `julia deps/build.jl` themselves; ────
# nothing to configure here. Both the CTest entry and the custom target
# below run build.jl first (idempotent — it just recompiles the shared
# lib) then the test suite, same two-step shape as Zig's `zig build test`
# implicitly does in one command.
set(CTOON_JULIA_RUN_TESTS
    ${JULIA_EXECUTABLE} --project=${CTOON_JULIA_BINDING_DIR}
    -e "import Pkg; Pkg.instantiate(); include(\"deps/build.jl\"); include(\"test/runtests.jl\")"
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

# ── ctoon_coverage_julia — not implemented ───────────────────────────────────
# Deliberately absent for now, same slot/reasoning as ctoon_coverage_zig
# right below in tests/zig/CMakeLists.txt: no single, stable, widely-used
# coverage-to-lcov path for a ccall-based Julia binding yet (Julia's own
# coverage tooling covers .jl source lines, not the C side this binding
# is mostly testing). Revisit once that's settled — see
# tests/CMakeLists.txt's merge/dashboard logic for how it would plug in.
