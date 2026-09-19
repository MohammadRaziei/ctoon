# Fetches ctoon's own Julia binding straight from GitHub — the same way
# any real Julia user would install it, via a git URL + subdir (Pkg's
# equivalent of `go get`/`pip install git+...`; CToon.jl doesn't need to
# be registered in Julia's General registry for this to work). Building
# the fetched package's native shared lib (deps/build.jl) happens
# automatically as part of `Pkg.add` — same as it does when developing
# the binding directly out of the main repo checkout.
#
# deps/build.jl fetches the core C source (src/ctoon.c, include/ctoon.h)
# from GitHub at build time too (see its own comment for why); by
# default that means whatever the latest tagged *release* is. A
# benchmark run should measure this repo's own current master instead
# — otherwise "run the benchmark" and "what commit does it measure"
# quietly drift apart the moment a new release is tagged — so
# CTOON_JULIA_CORE_REF=master is set for this one build.
add_custom_target(ctoon_benchmarks_julia_deps
    COMMAND ${CMAKE_COMMAND} -E env CTOON_JULIA_CORE_REF=master
            ${JULIA_EXECUTABLE} --project=${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/fetch_deps.jl ${CTOON_GITHUB_URL}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Fetching Julia benchmark dependency (ctoon, pinned to master) from GitHub"
    USES_TERMINAL
)

if(CTOON_BENCH_LOG_DEBUG)
    set(CTOON_JULIA_LOG_ARG ${CTOON_BENCH_LOGS_DIR}/julia.log)
else()
    set(CTOON_JULIA_LOG_ARG "")
endif()

add_custom_target(ctoon_benchmarks_julia
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Julia benchmark..."
    COMMAND ${JULIA_EXECUTABLE} --project=${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/bench.jl
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR}/julia.json ${CTOON_JULIA_LOG_ARG}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks_julia ctoon_benchmarks_julia_deps)
add_dependencies(ctoon_bench_langs ctoon_benchmarks_julia)
