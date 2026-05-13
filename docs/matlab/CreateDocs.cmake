# CreateDocs.cmake — MATLAB binding documentation
#
# Called via cmake -P or as part of the ctoon_docs_matlab_extract target.
#
# Required variables (passed via -D):
#   PYTHON_EXECUTABLE     Python 3 interpreter
#   MATLAB_BINDING_DIR    path to src/bindings/matlab  (.m source files)
#   SOURCE_DIR            path to docs/matlab/source   (hand-written .rst pages)
#   OUT_DIR               output directory (CMAKE_CURRENT_BINARY_DIR/matlab)
#   PROJECT_VERSION       version string
#
# Output (all in OUT_DIR/):
#   installation.rst      copied from SOURCE_DIR
#   usage.rst             copied from SOURCE_DIR
#   ctoon_encode.rst  \
#   ctoon_decode.rst   |  generated from .m docstrings
#   ctoon_read.rst     |
#   ctoon_write.rst   /
#   api.rst               top-level index that includes all function RSTs

cmake_minimum_required(VERSION 3.19)

foreach(_var PYTHON_EXECUTABLE MATLAB_BINDING_DIR SOURCE_DIR OUT_DIR PROJECT_VERSION)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "CreateDocs.cmake (matlab): ${_var} must be defined")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUT_DIR}")

# ---------------------------------------------------------------------------
# Step 1 — Copy hand-written .rst files into OUT_DIR
# ---------------------------------------------------------------------------
file(GLOB _hand_rst "${SOURCE_DIR}/*.rst")

foreach(_f IN LISTS _hand_rst)
    file(COPY "${_f}" DESTINATION "${OUT_DIR}")
    cmake_path(GET _f FILENAME _name)
    message(STATUS "  [matlab-docs] copied ${_name}")
endforeach()

# ---------------------------------------------------------------------------
# Step 2 — Extract .m docstrings → RST files into OUT_DIR
# ---------------------------------------------------------------------------
set(_extractor "${CMAKE_CURRENT_LIST_DIR}/extract_matlab_docs.py")

file(GLOB _m_sources "${MATLAB_BINDING_DIR}/ctoon_*.m")

if(NOT _m_sources)
    message(FATAL_ERROR
        "CreateDocs.cmake (matlab): no ctoon_*.m files found in ${MATLAB_BINDING_DIR}")
endif()

execute_process(
    COMMAND ${PYTHON_EXECUTABLE} "${_extractor}"
            --sources ${_m_sources}
            --out-dir "${OUT_DIR}"
            --version "${PROJECT_VERSION}"
    RESULT_VARIABLE _result
)

if(NOT _result EQUAL 0)
    message(FATAL_ERROR "extract_matlab_docs.py failed (exit ${_result})")
endif()

message(STATUS "MATLAB docs ready in: ${OUT_DIR}")
