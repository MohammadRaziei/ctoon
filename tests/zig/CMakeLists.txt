# ---------------------------------------------------------------------------
# Zig binding tests
# ---------------------------------------------------------------------------
# Mirrors the pattern used by tests/rust/CMakeLists.txt. The integration
# test file lives at tests/zig/integration_test.zig (this directory) rather
# than inside src/bindings/zig, so every language's tests stay under one
# root tests/ folder; build.zig (at the repo root, like Cargo.toml) points
# its "test" step at this file directly (no package-manager fetch step
# needed, same spirit as Cargo's [[test]] path in the Rust binding).
# Requires Zig >= 0.16.0 on PATH.
# ---------------------------------------------------------------------------

set(CTOON_ZIG_PROJECT_ROOT ${PROJECT_SOURCE_DIR})

# ── Locate Zig toolchain ─────────────────────────────────────────────────────
find_program(ZIG_EXECUTABLE zig)

ctoon_require_lang(CTOON_BUILD_ZIG ZIG_EXECUTABLE "Zig"
    "Install Zig >= 0.16.0 and make sure 'zig' is on PATH.")

if(NOT ZIG_EXECUTABLE)
    return()
endif()

execute_process(
    COMMAND ${ZIG_EXECUTABLE} version
    OUTPUT_VARIABLE ZIG_VERSION_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Found Zig: ${ZIG_VERSION_OUTPUT}")

# On some Linux setups (notably Debian/Ubuntu multiarch layouts, and
# some WSL/conda environments), Zig's bundled lld linker looks for libc
# pieces (libm.so.6, etc.) under /lib64 by default and doesn't find them
# if the system instead keeps them under a triplet directory like
# /usr/lib/x86_64-linux-gnu. Rather than assume any particular layout,
# ask the C compiler CMake already found where IT looks for libraries by
# default (CMAKE_C_IMPLICIT_LINK_DIRECTORIES) and hand each of those to
# zig via --search-prefix. No-op on setups that don't need it.
set(CTOON_ZIG_SEARCH_PREFIX_ARGS "")
set(CTOON_ZIG_LIB_PATH_LIST "")
foreach(_dir IN LISTS CMAKE_C_IMPLICIT_LINK_DIRECTORIES)
    # zig's --search-prefix requires every path given to actually exist
    # as a directory — GCC's own implicit-link-directory list routinely
    # includes paths that don't exist on disk (this is normal for GCC;
    # it's an optional search list, not a guarantee), so filter those
    # out before handing the list to zig.
    if(IS_DIRECTORY "${_dir}")
        list(APPEND CTOON_ZIG_SEARCH_PREFIX_ARGS --search-prefix "${_dir}")
        list(APPEND CTOON_ZIG_LIB_PATH_LIST "${_dir}")
    endif()
endforeach()
# Belt-and-suspenders: also set LD_LIBRARY_PATH to the same list, in case
# --search-prefix and LD_LIBRARY_PATH aren't both honored the same way
# in every zig/lld version — whichever one actually gets picked up is
# enough. Harmless to set when unneeded.
string(REPLACE ";" ":" CTOON_ZIG_LD_LIBRARY_PATH "${CTOON_ZIG_LIB_PATH_LIST}")

# ── CTest integration ────────────────────────────────────────────────────────
add_test(
    NAME ctoon_zig_tests
    COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CTOON_ZIG_LD_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH}"
            ${ZIG_EXECUTABLE} build ${CTOON_ZIG_SEARCH_PREFIX_ARGS} test
    WORKING_DIRECTORY ${CTOON_ZIG_PROJECT_ROOT}
)
set_tests_properties(ctoon_zig_tests PROPERTIES TIMEOUT 180)

# ── ctoon_test_zig custom target ─────────────────────────────────────────────
add_custom_target(ctoon_test_zig
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Zig tests..."
    COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CTOON_ZIG_LD_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH}"
            ${ZIG_EXECUTABLE} build ${CTOON_ZIG_SEARCH_PREFIX_ARGS} test
    WORKING_DIRECTORY ${CTOON_ZIG_PROJECT_ROOT}
    USES_TERMINAL
)

add_dependencies(ctoon_test ctoon_test_zig)

# ── ctoon_coverage_zig — not implemented ─────────────────────────────────────
# Deliberately absent for now: Zig's own coverage tooling (kcov-based, via
# `zig build test --test-coverage` on some setups) is still too immature/
# unstable across Zig versions to build a reliable CMake target around it —
# unlike Go and Rust, which each have one well-established, stable tool
# (`go tool cover`, `cargo-llvm-cov`). Revisit once Zig's native coverage
# support stabilizes. This is the same slot ctoon_coverage_go and
# ctoon_coverage_rust fill in their own tests/<lang>/CMakeLists.txt — see
# tests/CMakeLists.txt's merge/dashboard logic for how a future
# ctoon_coverage_zig target would plug in the same way.
