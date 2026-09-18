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
    "${CMAKE_CURRENT_SOURCE_DIR}/generate_report.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/template.html.jinja2"
  COMMENT "ctoon benchmarks: rendering the standalone HTML report"
  USES_TERMINAL
)
add_custom_target(ctoon_bench_report DEPENDS "${REPORT_HTML}")
add_dependencies(ctoon_bench_report ctoon_bench_report_venv ctoon_bench_chartjs)

# The report target always runs *after* whichever language benchmarks
# were actually configured (see toolchain detection in the parent
# CMakeLists.txt) -- ctoon_bench_langs itself already collects exactly
# those, so just depend on that umbrella target instead of hardcoding
# ctoon_benchmarks_c / _cpp / _python / ... here. ctoon_benchmarks (the
# top-level target people actually run) then depends on both this
# target and ctoon_bench_langs, so the report always renders last.
add_dependencies(ctoon_bench_report ctoon_bench_langs)
