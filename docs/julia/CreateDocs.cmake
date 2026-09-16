# CreateDocs.cmake — Julia documentation (Documenter.jl)
# Called via: cmake -P CreateDocs.cmake
# Required CMake variables (passed via -D):
#   JULIA_EXECUTABLE — Julia interpreter
#   DOCS_DIR         — this directory (contains make.jl, src/, Project.toml)
#   OUTPUT_DIR       — Output directory for generated HTML

cmake_minimum_required(VERSION 3.19)

# ── Validate ──────────────────────────────────────────────────────────────────
foreach(_var JULIA_EXECUTABLE DOCS_DIR OUTPUT_DIR)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "CreateDocs.cmake (julia): ${_var} must be defined")
    endif()
endforeach()

# ── Instantiate Documenter.jl ────────────────────────────────────────────────
# Rather than requiring a separate CI step to pre-instantiate this
# Pkg environment (unlike Python's pip install, which needs its own
# `pip install -r requirements-dev.txt` step), just instantiate it here
# if needed — cheap and idempotent when already up to date.
execute_process(
    COMMAND ${JULIA_EXECUTABLE} --project=${DOCS_DIR} -e "import Pkg; Pkg.instantiate()"
    RESULT_VARIABLE _DOCUMENTER_INSTANTIATE
)
if(NOT _DOCUMENTER_INSTANTIATE EQUAL 0)
    message(FATAL_ERROR "Failed to instantiate docs/julia's Pkg environment (Documenter.jl).")
endif()

# ── Run Documenter.jl ─────────────────────────────────────────────────────────
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set(ENV{CTOON_JULIA_DOCS_OUT} "${OUTPUT_DIR}")

execute_process(
    COMMAND ${JULIA_EXECUTABLE} --project=${DOCS_DIR} ${DOCS_DIR}/make.jl
    WORKING_DIRECTORY ${DOCS_DIR}
    RESULT_VARIABLE JULIA_DOCS_RESULT
)

if(NOT JULIA_DOCS_RESULT EQUAL 0)
    message(FATAL_ERROR "Documenter.jl build failed with exit code ${JULIA_DOCS_RESULT}")
endif()

message(STATUS "Julia docs generated at: ${OUTPUT_DIR}/index.html")
