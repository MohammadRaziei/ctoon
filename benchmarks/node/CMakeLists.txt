# @toon-format/toon (fetched via npm, in package.json) is the only
# implementation here — no ctoon Node.js binding exists yet (see bench.mjs).
add_custom_target(ctoon_benchmarks_node_deps
    COMMAND ${NPM_EXECUTABLE} install --silent --no-audit --no-fund
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Installing Node.js benchmark dependencies from npm"
    USES_TERMINAL
)

if(CTOON_BENCH_LOG_DEBUG)
    set(CTOON_NODE_LOG_ARG ${CTOON_BENCH_LOGS_DIR}/node.log)
else()
    set(CTOON_NODE_LOG_ARG "")
endif()

add_custom_target(ctoon_benchmarks_node
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Node.js benchmark..."
    COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/bench.mjs
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR}/node.json ${CTOON_NODE_LOG_ARG}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks_node ctoon_benchmarks_node_deps)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_node)
