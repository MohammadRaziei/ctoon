# CreateDocs.cmake — Python documentation (Sphinx)
# Called via: cmake -P CreateDocs.cmake
# Required CMake variables (passed via -D):
#   PYTHON_EXECUTABLE, SOURCE_DIR, OUTPUT_DIR

cmake_minimum_required(VERSION 3.19)

set(PY_OUT "${OUTPUT_DIR}")
set(PY_HTML "${PY_OUT}/html")
set(PY_DOCTREES "${PY_OUT}/doctrees")

file(MAKE_DIRECTORY "${PY_HTML}")
file(MAKE_DIRECTORY "${PY_DOCTREES}")

# ── Run Sphinx ──────────────────────────────────────────────
execute_process(
  COMMAND "${PYTHON_EXECUTABLE}" -m sphinx
          -b html
          -d "${PY_DOCTREES}"
          "${SOURCE_DIR}"
          "${PY_HTML}"
  RESULT_VARIABLE SPHINX_RESULT
)

if(NOT SPHINX_RESULT EQUAL 0)
  message(FATAL_ERROR "Sphinx failed with exit code ${SPHINX_RESULT}")
endif()

message(STATUS "-- Python docs generated at: ${PY_HTML}/index.html")
