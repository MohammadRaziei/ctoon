# No separate go.mod here on purpose: this file lives inside the same
# module as the repo root (github.com/mohammadraziei/ctoon), exactly like
# tests/go does, so plain `go run` already resolves the "github.com/
# mohammadraziei/ctoon" import against the local checkout — no CMake
# add_subdirectory() linkage required for Go to find it.
add_custom_target(ctoon_benchmark_go
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Go benchmark..."
    COMMAND ${GO_EXECUTABLE} run ${CMAKE_CURRENT_SOURCE_DIR}/bench_ctoon.go ${CTOON_BENCH_MANIFEST}
    WORKING_DIRECTORY ${CTOON_ROOT_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmark ctoon_benchmark_go)

# Both competitor comparisons below fetch their dependency directly from
# GitHub via git (not the Go module proxy / sum database, which aren't on
# this project's network allowlist), and live in their own nested Go
# module with a `replace` back to the repo root for the ctoon import — so
# a competitor's own Go-version floor never propagates to the main
# module's go.mod (used by the real ctoon package and tests/go).
set(CTOON_BENCH_GO_ENV
    ${CMAKE_COMMAND} -E env GOPROXY=direct GOSUMDB=off GOTOOLCHAIN=local
)

# ── vs gotoon (community, encode-only) — no Go-version floor to speak of ──
add_custom_target(ctoon_benchmark_go_vs_gotoon
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Go benchmark vs gotoon..."
    COMMAND ${CTOON_BENCH_GO_ENV} ${GO_EXECUTABLE} mod tidy
    COMMAND ${CTOON_BENCH_GO_ENV} ${GO_EXECUTABLE} run . ${CTOON_BENCH_MANIFEST}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vs_gotoon
    USES_TERMINAL
)
add_dependencies(ctoon_benchmark ctoon_benchmark_go_vs_gotoon)

# ── vs toon-go (official) — requires Go >= 1.23 ──
execute_process(
    COMMAND ${GO_EXECUTABLE} version
    OUTPUT_VARIABLE CTOON_GO_VERSION_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX MATCH "go([0-9]+)\\.([0-9]+)" _unused "${CTOON_GO_VERSION_OUTPUT}")
set(CTOON_GO_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")

if(CTOON_GO_VERSION VERSION_GREATER_EQUAL "1.23")
    add_custom_target(ctoon_benchmark_go_vs_toongo
        COMMAND ${CMAKE_COMMAND} -E echo "-- Running Go benchmark vs toon-go..."
        COMMAND ${CTOON_BENCH_GO_ENV} ${GO_EXECUTABLE} mod tidy
        COMMAND ${CTOON_BENCH_GO_ENV} ${GO_EXECUTABLE} run . ${CTOON_BENCH_MANIFEST}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vs_toongo
        USES_TERMINAL
    )
    add_dependencies(ctoon_benchmark ctoon_benchmark_go_vs_toongo)
else()
    message(WARNING "Go ${CTOON_GO_VERSION} found, but toon-go requires >= 1.23 "
                     "— skipping the vs-toon-go benchmark.")
endif()
