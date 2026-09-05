# All three dependencies (ctoon included) are fetched the same way: via
# `go get` from their real GitHub module paths, using GOPROXY=direct
# GOSUMDB=off so this works even where the module proxy/sum database
# isn't reachable (falls back to a direct git clone).
set(CTOON_BENCH_GO_ENV
    ${CMAKE_COMMAND} -E env GOPROXY=direct GOSUMDB=off GOTOOLCHAIN=local
)

add_custom_target(ctoon_benchmarks_go_deps
    COMMAND ${CTOON_BENCH_GO_ENV} ${GO_EXECUTABLE} get github.com/mohammadraziei/ctoon@master
    COMMAND ${CTOON_BENCH_GO_ENV} ${GO_EXECUTABLE} get github.com/alpkeskin/gotoon@main
    COMMAND ${CTOON_BENCH_GO_ENV} ${GO_EXECUTABLE} get github.com/toon-format/toon-go@main
    COMMAND ${CTOON_BENCH_GO_ENV} ${GO_EXECUTABLE} mod tidy
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Fetching Go benchmark dependencies from GitHub"
    USES_TERMINAL
)

add_custom_target(ctoon_benchmarks_go
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Go benchmark..."
    COMMAND ${CTOON_BENCH_GO_ENV} ${GO_EXECUTABLE} run .
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR}/go.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks_go ctoon_benchmarks_go_deps)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_go)
