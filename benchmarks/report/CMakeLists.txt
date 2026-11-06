# ------------------------------------------------------------ system info --
# A factual record of the machine these numbers ran on (CPU, cores, RAM,
# OS) -- same as https://github.com/MohammadRaziei/pygixml/blob/main/benchmarks/python/system_info.py.
# Plain stdlib (platform/os), run with the system interpreter -- no venv.

set(SYSTEM_INFO_JSON "${CTOON_BENCH_RESULTS_DIR}/system_info.json")

add_custom_command(
  OUTPUT "${SYSTEM_INFO_JSON}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/system_info.py" "${SYSTEM_INFO_JSON}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/system_info.py"
  COMMENT "ctoon benchmarks: recording system info (CPU/RAM/OS)"
  USES_TERMINAL
)
add_custom_target(ctoon_bench_system_info DEPENDS "${SYSTEM_INFO_JSON}")

# --------------------------------------------------------------- memory ----
# Peak RSS per (language, library), each in its own fresh process (see
# collect_memory.py's docstring for why) -- merged straight into the
# c.json/cpp.json that ctoon_benchmarks_c / _cpp
# already wrote, so it MUST run after those, and MUST finish before
# the results are copied out. Only languages whose toolchain
# was actually found (bench_c / bench_cpp targets exist) get a --*-exe flag; the others are
# silently left out of collect_memory.py's argv, same "only what's
# configured" convention as everywhere else in this suite.
#
# Off by default (CTOON_BENCH_MEMORY): it reruns every library several
# times over in fresh processes just to read peak RSS, which roughly
# doubles that language's benchmark wall time for information most
# people don't need on every run. Turn on with:
#   cmake -S . -B build-bench -DCTOON_BENCH_MEMORY=ON

option(CTOON_BENCH_MEMORY "Measure peak memory per (language, library) in isolated processes" OFF)

set(MEMORY_STAMP "")
if(CTOON_BENCH_MEMORY)
  set(MEMORY_STAMP "${CMAKE_CURRENT_BINARY_DIR}/_memory.stamp")
  set(MEMORY_ARGS "")
  set(MEMORY_DEPS "")

  if(TARGET bench_c)
    list(APPEND MEMORY_ARGS --c-exe "$<TARGET_FILE:bench_c>" --c-manifest "${CTOON_BENCH_MANIFEST}")
    list(APPEND MEMORY_DEPS ctoon_benchmarks_c)
  endif()
  if(TARGET bench_cpp)
    list(APPEND MEMORY_ARGS --cpp-exe "$<TARGET_FILE:bench_cpp>")
    list(APPEND MEMORY_DEPS ctoon_benchmarks_cpp)
  endif()

  add_custom_command(
    OUTPUT "${MEMORY_STAMP}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/collect_memory.py"
            "${CTOON_BENCH_RESULTS_DIR}" ${MEMORY_ARGS}
    COMMAND ${CMAKE_COMMAND} -E touch "${MEMORY_STAMP}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/collect_memory.py"
    COMMENT "ctoon benchmarks: measuring peak memory per library (isolated processes)"
    USES_TERMINAL
  )
  add_custom_target(ctoon_bench_memory DEPENDS "${MEMORY_STAMP}")
  if(MEMORY_DEPS)
    add_dependencies(ctoon_bench_memory ${MEMORY_DEPS})
  endif()
else()
  message(STATUS "ctoon benchmarks: memory measurement is OFF (CTOON_BENCH_MEMORY=OFF) - "
                  "peak_rss_mb will not appear in the results. Enable with -DCTOON_BENCH_MEMORY=ON.")
endif()

# ------------------------------------------------------------------ verify --
# "Success" means the output is CORRECT, not merely that nothing threw. Each
# language harness dumps what it produced (results/dump/...), and
# node/verify_outputs.mjs judges every one of those outputs against a single
# shared criterion -- json_to_toon is decoded with the OFFICIAL reference
# decoder, the JSON-producing operations are parsed back and compared to the
# original -- then rewrites success_rate / throughput_mb_s / docs_per_sec in
# each <language>.json so only correct conversions count (the harness's own
# numbers are kept under "raw"). See that script's header for the details.
#
# A plain custom target, not an OUTPUT-stamp rule: the language targets
# rewrite their JSON on every run, so this has to re-judge on every run too.
# Needs Node (it uses @toon-format/toon as the reference decoder); without it
# the JSON is still written, with every row marked unverified.
if(TARGET ctoon_benchmarks_node_deps AND NODE_EXECUTABLE)
  add_custom_target(ctoon_bench_verify
    COMMAND ${CMAKE_COMMAND} -E echo "-- Verifying that every benchmark output is correct..."
    COMMAND "${NODE_EXECUTABLE}" "${CTOON_BENCH_SOURCE_DIR}/node/verify_outputs.mjs"
            "${CTOON_BENCH_MANIFEST}" "${CTOON_BENCH_RESULTS_DIR}"
    COMMENT "ctoon benchmarks: verifying outputs against the reference decoder"
    USES_TERMINAL
  )
  add_dependencies(ctoon_bench_verify ctoon_benchmarks_node_deps ctoon_bench_langs)
  if(CTOON_BENCH_MEMORY)
    add_dependencies(ctoon_bench_verify ctoon_bench_memory)   # memory merges into the same JSON first
  endif()
else()
  message(WARNING "ctoon benchmarks: Node.js not found -- outputs will NOT be verified, so "
                  "'success' only means 'no error' (the report says so). Install Node.js to fix.")
endif()

# ------------------------------------------------------------------ report --

# The suite's only committed output is results/*.json: one raw JSON file
# per language (c.json, cpp.json, ...) plus system_info.json. The HTML
# page is NOT built here: the documentation build (docs/CMakeLists.txt)
# reads that directory and renders <site>/benchmarks/index.html with
# docs/benchmarks/render_benchmarks.py. Run the suite on your own machine,
# commit the JSON, and CI just publishes the page -- CI never runs the
# benchmarks. Everything else (venvs, fetched sources, dumps) stays in the
# disposable build dir.
#
# A plain custom target, not an OUTPUT rule: the language targets rewrite
# their JSON on every run, so this has to copy on every run too.
set(CTOON_BENCH_REPORT_DIR "${CTOON_BENCH_SOURCE_DIR}/results"
    CACHE PATH "Where the final per-language results/*.json are copied -- the one artifact meant to be committed")

add_custom_target(ctoon_bench_report
  COMMAND ${CMAKE_COMMAND}
          -DSRC=${CTOON_BENCH_RESULTS_DIR} -DDST=${CTOON_BENCH_REPORT_DIR}
          -P "${CTOON_BENCH_SOURCE_DIR}/cmake/CopyResults.cmake"
  COMMENT "ctoon benchmarks: copying results/*.json -> ${CTOON_BENCH_REPORT_DIR}"
  USES_TERMINAL
)
add_dependencies(ctoon_bench_report ctoon_bench_system_info)
if(CTOON_BENCH_MEMORY)
  add_dependencies(ctoon_bench_report ctoon_bench_memory)
endif()

# The copy target always runs *after* whichever language benchmarks
# were actually configured (see toolchain detection in the parent
# CMakeLists.txt) -- ctoon_bench_langs itself already collects exactly
# those, so just depend on that umbrella target instead of hardcoding
# ctoon_benchmarks_c / _cpp / _python / ... here. ctoon_benchmarks (the
# top-level target people actually run) then depends on both this
# target and ctoon_bench_langs, so the copy always runs last.
add_dependencies(ctoon_bench_report ctoon_bench_langs)
if(TARGET ctoon_bench_verify)
  add_dependencies(ctoon_bench_report ctoon_bench_verify)
endif()
