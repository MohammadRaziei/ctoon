# ------------------------------------------------------------- Chart.js --
# Fetched fresh via plain file(DOWNLOAD) from the npm registry (a tarball,
# same as `npm pack` would give you) -- nothing vendored in the source
# tree, same convention as
# https://github.com/MohammadRaziei/pygixml/blob/main/benchmarks/report/CMakeLists.txt

set(CHARTJS_VERSION "4.5.1" CACHE STRING "Chart.js version to embed in the report")
set(CHARTJS_DL_DIR "${CMAKE_CURRENT_BINARY_DIR}/_chartjs")
set(CHARTJS_ARCHIVE "${CHARTJS_DL_DIR}/chart.js-${CHARTJS_VERSION}.tgz")
set(CHARTJS_EXTRACTED "${CHARTJS_DL_DIR}/extracted")
set(CHARTJS_UMD_MIN "${CHARTJS_EXTRACTED}/package/dist/chart.umd.min.js")

file(MAKE_DIRECTORY "${CHARTJS_DL_DIR}")
file(MAKE_DIRECTORY "${CHARTJS_EXTRACTED}")

add_custom_command(
  OUTPUT "${CHARTJS_UMD_MIN}"
  COMMAND ${CMAKE_COMMAND} -DURL=https://registry.npmjs.org/chart.js/-/chart.js-${CHARTJS_VERSION}.tgz
                            -DDEST=${CHARTJS_ARCHIVE}
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Download.cmake"
  COMMAND ${CMAKE_COMMAND} -E tar xzf "${CHARTJS_ARCHIVE}"
          WORKING_DIRECTORY "${CHARTJS_EXTRACTED}"
  COMMENT "ctoon benchmarks: fetching Chart.js ${CHARTJS_VERSION} (for the standalone report)"
  USES_TERMINAL
)
add_custom_target(ctoon_bench_chartjs DEPENDS "${CHARTJS_UMD_MIN}")

# ------------------------------------------------------- python venv ------
# generate_report.py only needs jinja2 -- reuse a tiny throwaway venv
# rather than requiring it on the system interpreter.

set(REPORT_VENV_DIR "${CMAKE_CURRENT_BINARY_DIR}/_venv")
if(WIN32)
  set(REPORT_VENV_PYTHON "${REPORT_VENV_DIR}/Scripts/python.exe")
else()
  set(REPORT_VENV_PYTHON "${REPORT_VENV_DIR}/bin/python")
endif()
set(REPORT_VENV_STAMP "${CMAKE_CURRENT_BINARY_DIR}/_venv.stamp")

add_custom_command(
  OUTPUT "${REPORT_VENV_STAMP}"
  COMMAND ${Python3_EXECUTABLE} -m venv "${REPORT_VENV_DIR}"
  COMMAND "${REPORT_VENV_PYTHON}" -m pip install --quiet --upgrade pip jinja2
  COMMAND ${CMAKE_COMMAND} -E touch "${REPORT_VENV_STAMP}"
  COMMENT "ctoon benchmarks: creating report venv (jinja2)"
  USES_TERMINAL
)
add_custom_target(ctoon_bench_report_venv DEPENDS "${REPORT_VENV_STAMP}")

# ------------------------------------------------------------ system info --
# A factual record of the machine these numbers ran on (CPU, cores, RAM,
# OS) -- same as https://github.com/MohammadRaziei/pygixml/blob/main/benchmarks/python/system_info.py.
# Uses the report's own venv (plain stdlib -- platform/os -- no extra deps),
# so it doesn't depend on whether the python *language* benchmark subdir
# was configured.

set(SYSTEM_INFO_JSON "${CTOON_BENCH_RESULTS_DIR}/system_info.json")

add_custom_command(
  OUTPUT "${SYSTEM_INFO_JSON}"
  COMMAND "${REPORT_VENV_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/system_info.py" "${SYSTEM_INFO_JSON}"
  DEPENDS "${REPORT_VENV_STAMP}" "${CMAKE_CURRENT_SOURCE_DIR}/system_info.py"
  COMMENT "ctoon benchmarks: recording system info (CPU/RAM/OS)"
  USES_TERMINAL
)
add_custom_target(ctoon_bench_system_info DEPENDS "${SYSTEM_INFO_JSON}")
add_dependencies(ctoon_bench_system_info ctoon_bench_report_venv)

# --------------------------------------------------------------- memory ----
# Peak RSS per (language, library), each in its own fresh process (see
# collect_memory.py's docstring for why) -- merged straight into the
# c.json/cpp.json/python.json that ctoon_benchmarks_c / _cpp / _python
# already wrote, so it MUST run after those, and MUST finish before
# generate_report.py reads those files. Only languages whose toolchain
# was actually found (bench_c / bench_cpp targets exist, or the python
# venv was configured) get a --*-exe/--py-python flag; the others are
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
  if(DEFINED CTOON_PY_VENV_PYTHON AND TARGET ctoon_benchmarks_python)
    list(APPEND MEMORY_ARGS --py-python "${CTOON_PY_VENV_PYTHON}"
                            --py-script "${CMAKE_CURRENT_SOURCE_DIR}/../python/bench.py"
                            --py-manifest "${CTOON_BENCH_MANIFEST}")
    list(APPEND MEMORY_DEPS ctoon_benchmarks_python)
  endif()

  add_custom_command(
    OUTPUT "${MEMORY_STAMP}"
    COMMAND "${REPORT_VENV_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/collect_memory.py"
            "${CTOON_BENCH_RESULTS_DIR}" ${MEMORY_ARGS}
    COMMAND ${CMAKE_COMMAND} -E touch "${MEMORY_STAMP}"
    DEPENDS "${REPORT_VENV_STAMP}" "${CMAKE_CURRENT_SOURCE_DIR}/collect_memory.py"
    COMMENT "ctoon benchmarks: measuring peak memory per library (isolated processes)"
    USES_TERMINAL
  )
  add_custom_target(ctoon_bench_memory DEPENDS "${MEMORY_STAMP}")
  add_dependencies(ctoon_bench_memory ctoon_bench_report_venv)
  if(MEMORY_DEPS)
    add_dependencies(ctoon_bench_memory ${MEMORY_DEPS})
  endif()
else()
  message(STATUS "ctoon benchmarks: memory measurement is OFF (CTOON_BENCH_MEMORY=OFF) — "
                  "peak_rss_mb will not appear in the report. Enable with -DCTOON_BENCH_MEMORY=ON.")
endif()

# ------------------------------------------------------------------ report --

set(REPORT_HTML "${CTOON_BENCH_RESULTS_DIR}/report.html")

add_custom_command(
  OUTPUT "${REPORT_HTML}"
  COMMAND "${REPORT_VENV_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_report.py"
          "${CTOON_BENCH_RESULTS_DIR}" "${REPORT_HTML}"
          --chartjs-path "${CHARTJS_UMD_MIN}"
  DEPENDS
    "${REPORT_VENV_STAMP}"
    "${CHARTJS_UMD_MIN}"
    "${SYSTEM_INFO_JSON}"
    "${MEMORY_STAMP}"
    "${CMAKE_CURRENT_SOURCE_DIR}/generate_report.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/template.html.jinja2"
  COMMENT "ctoon benchmarks: rendering the standalone HTML report"
  USES_TERMINAL
)
add_custom_target(ctoon_bench_report DEPENDS "${REPORT_HTML}")
add_dependencies(ctoon_bench_report ctoon_bench_report_venv ctoon_bench_chartjs ctoon_bench_system_info)
if(CTOON_BENCH_MEMORY)
  add_dependencies(ctoon_bench_report ctoon_bench_memory)
endif()

# The report target always runs *after* whichever language benchmarks
# were actually configured (see toolchain detection in the parent
# CMakeLists.txt) -- ctoon_bench_langs itself already collects exactly
# those, so just depend on that umbrella target instead of hardcoding
# ctoon_benchmarks_c / _cpp / _python / ... here. ctoon_benchmarks (the
# top-level target people actually run) then depends on both this
# target and ctoon_bench_langs, so the report always renders last.
add_dependencies(ctoon_bench_report ctoon_bench_langs)
