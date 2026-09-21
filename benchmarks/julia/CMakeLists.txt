# A local, throwaway Julia depot (registries, packages, compiled
# cache, artifacts -- everything Julia would otherwise put in
# ~/.julia) instead of the user's real one: the same idea as a Python
# venv, just at the depot level rather than per-package. Nothing this
# benchmark does ever touches ~/.julia, and `rm -rf build-bench` wipes
# every trace of it, including the General registry download -- no
# separate cleanup step, no risk to whatever the user has installed
# globally for their own Julia work.
set(CTOON_JULIA_DEPOT "${CMAKE_CURRENT_BINARY_DIR}/depot")
file(MAKE_DIRECTORY "${CTOON_JULIA_DEPOT}")

# Fetches ctoon's own Julia binding straight from GitHub — the same way
# any real Julia user would install it, declared in this environment's
# own Project.toml (see its [sources] entry) rather than an imperative
# `Pkg.add(url=..., subdir=...)` call: modern Pkg resolves a [sources]
# entry the same way for an unregistered git+subdir dependency, and a
# declared entry is one less script to maintain. Pinned to rev="master"
# right in Project.toml, since this benchmark should always measure
# ctoon's current master, not whatever the last tagged release happened
# to be — every checkout of this repo gets that same pin, so there's no
# separate "make sure this build measures master" step to remember.
# Building the fetched package's native shared lib (deps/build.jl)
# happens automatically as part of resolving it -- and "resolving"
# means BOTH `Pkg.instantiate()` (for a fresh depot with no
# Manifest.toml yet) AND `Pkg.update("CToon")` (to re-resolve against
# current master on every later run); see update_deps.jl's own comment
# for why a rev="master" pin specifically needs both, in that order.
#
# That's the Julia *binding* (CToon.jl itself). deps/build.jl separately
# fetches the core C source (src/ctoon.c, include/ctoon.h) it compiles
# against — a different pin, for a different piece — and defaults to
# whatever this package's own Project.toml version says is the latest
# *release*. Same reasoning applies there: a benchmark run should
# measure current master, not a possibly-older tagged release, so
# CTOON_JULIA_CORE_REF=master is set for this one build too.
add_custom_target(ctoon_benchmarks_julia_deps
    COMMAND ${CMAKE_COMMAND} -E env
            CTOON_JULIA_CORE_REF=master
            JULIA_DEPOT_PATH=${CTOON_JULIA_DEPOT}
            ${JULIA_EXECUTABLE} --project=${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/update_deps.jl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Fetching Julia benchmark dependency (ctoon, pinned to master) from GitHub, into a local depot"
    USES_TERMINAL
)

if(CTOON_BENCH_LOG_DEBUG)
    set(CTOON_JULIA_LOG_ARG ${CTOON_BENCH_LOGS_DIR}/julia.log)
else()
    set(CTOON_JULIA_LOG_ARG "")
endif()

add_custom_target(ctoon_benchmarks_julia
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Julia benchmark..."
    COMMAND ${CMAKE_COMMAND} -E env
            JULIA_DEPOT_PATH=${CTOON_JULIA_DEPOT}
            ${JULIA_EXECUTABLE} --project=${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/bench.jl
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR}/julia.json ${CTOON_JULIA_LOG_ARG}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks_julia ctoon_benchmarks_julia_deps)
add_dependencies(ctoon_bench_langs ctoon_benchmarks_julia)
