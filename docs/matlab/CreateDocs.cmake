# CreateDocs.cmake — MATLAB binding documentation
#
# Required variables (passed via -D):
#   PYTHON_EXECUTABLE     Python 3 interpreter
#   MATLAB_BINDING_DIR    path to src/bindings/matlab
#   SOURCE_DIR            path to docs/matlab/source   (hand-written .rst)
#   OUT_DIR               output directory
#   PROJECT_VERSION       version string
#
# Output (all in OUT_DIR/):
#   installation.rst      copied from SOURCE_DIR
#   usage.rst             copied from SOURCE_DIR
#   api.rst               generated — package functions + top-level scripts

cmake_minimum_required(VERSION 3.19)

foreach(_var PYTHON_EXECUTABLE MATLAB_BINDING_DIR SOURCE_DIR OUT_DIR PROJECT_VERSION)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "CreateDocs.cmake (matlab): ${_var} must be defined")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")

# ---------------------------------------------------------------------------
# Step 1 — Copy hand-written .rst files
# ---------------------------------------------------------------------------
file(GLOB _hand_rst "${SOURCE_DIR}/*.rst")
foreach(_f IN LISTS _hand_rst)
    file(COPY "${_f}" DESTINATION "${OUT_DIR}")
    cmake_path(GET _f FILENAME _name)
    message(STATUS "  [matlab-docs] copied ${_name}")
endforeach()

# ---------------------------------------------------------------------------
# Step 2 — Collect sources
#   group 1: ctoon/ package functions  (encode, decode, load, dump, ...)
#   group 2: top-level helper scripts  (ctoon_build, ctoon_install, ctoon_clean)
# ---------------------------------------------------------------------------
file(GLOB _pkg_sources    "${MATLAB_BINDING_DIR}/ctoon/*.m")
file(GLOB _helper_sources "${MATLAB_BINDING_DIR}/ctoon_*.m")

if(NOT _pkg_sources AND NOT _helper_sources)
    message(FATAL_ERROR
        "CreateDocs.cmake (matlab): no .m files found in ${MATLAB_BINDING_DIR}")
endif()

set(_extractor "${CMAKE_CURRENT_LIST_DIR}/extract_matlab_docs.py")

execute_process(
    COMMAND ${PYTHON_EXECUTABLE} "${_extractor}"
            --pkg-sources     ${_pkg_sources}
            --helper-sources  ${_helper_sources}
            --out-dir         "${OUT_DIR}"
            --version         "${PROJECT_VERSION}"
    RESULT_VARIABLE _result
)

if(NOT _result EQUAL 0)
    message(FATAL_ERROR "extract_matlab_docs.py failed (exit ${_result})")
endif()

message(STATUS "MATLAB docs ready in: ${OUT_DIR}")