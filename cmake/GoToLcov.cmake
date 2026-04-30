# GoToLcov.cmake
# Converts a Go coverage profile (coverage.out) to lcov format (coverage.lcov).
#
# Usage (from tests/go/CMakeLists.txt):
#   cmake -DCOVER_OUT=<path/coverage.out>
#         -DLCOV_OUT=<path/coverage.lcov>
#         -DMODULE_ROOT=<absolute path to directory containing go.mod>
#         -P <project>/cmake/GoToLcov.cmake

cmake_minimum_required(VERSION 3.19)

if(NOT COVER_OUT OR NOT LCOV_OUT OR NOT MODULE_ROOT)
    message(FATAL_ERROR
        "GoToLcov: required variables missing.\n"
        "  -DCOVER_OUT=<coverage.out>\n"
        "  -DLCOV_OUT=<coverage.lcov>\n"
        "  -DMODULE_ROOT=<dir with go.mod>\n"
    )
endif()

# Read the module path from go.mod (first line: "module <path>")
file(READ "${MODULE_ROOT}/go.mod" GOMOD_CONTENT)
string(REGEX MATCH "module[ \t]+([^\r\n]+)" _ "${GOMOD_CONTENT}")
set(MODULE_PATH "${CMAKE_MATCH_1}")
string(STRIP "${MODULE_PATH}" MODULE_PATH)

if(NOT MODULE_PATH)
    message(FATAL_ERROR "GoToLcov: could not parse module path from ${MODULE_ROOT}/go.mod")
endif()

message(STATUS "GoToLcov: module = ${MODULE_PATH}")

# Read and normalise line endings
file(READ "${COVER_OUT}" RAW)
string(REPLACE "\r\n" "\n" RAW "${RAW}")
string(REPLACE "\r"   "\n" RAW "${RAW}")
string(REPLACE "\n"   ";"  LINES "${RAW}")

set(LCOV "")
set(CURRENT_FILE "")
set(CURRENT_DA "")

foreach(LINE IN LISTS LINES)
    # skip blank lines and the "mode:" header
    if(LINE STREQUAL "" OR LINE MATCHES "^mode:")
        continue()
    endif()

    # format: <pkg/file>:<sline>.<scol>,<eline>.<ecol> <stmts> <count>
    if(NOT LINE MATCHES "^([^:]+):([0-9]+)\\.([0-9]+),([0-9]+)\\.([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)$")
        continue()
    endif()

    set(FILE  "${CMAKE_MATCH_1}")
    set(SLINE "${CMAKE_MATCH_2}")
    set(ELINE "${CMAKE_MATCH_4}")
    set(COUNT "${CMAKE_MATCH_7}")

    # Strip the module path prefix to get the repo-relative file path
    string(REPLACE "${MODULE_PATH}/" "" REL_PATH "${FILE}")
    set(ABS_PATH "${MODULE_ROOT}/${REL_PATH}")

    if(NOT FILE STREQUAL CURRENT_FILE)
        # close the previous SF block
        if(CURRENT_FILE)
            string(APPEND LCOV "${CURRENT_DA}end_of_record\n")
        endif()
        set(CURRENT_FILE "${FILE}")
        set(CURRENT_DA "SF:${ABS_PATH}\n")
    endif()

    # emit one DA line per source line covered by this block
    foreach(L RANGE ${SLINE} ${ELINE})
        string(APPEND CURRENT_DA "DA:${L},${COUNT}\n")
    endforeach()
endforeach()

# close the last SF block
if(CURRENT_FILE)
    string(APPEND LCOV "${CURRENT_DA}end_of_record\n")
endif()

file(WRITE "${LCOV_OUT}" "${LCOV}")
message(STATUS "GoToLcov: wrote ${LCOV_OUT}")