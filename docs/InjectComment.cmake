# InjectComment.cmake
# Post-process script: injects a formatted HTML comment at the top of every
# .html file found under OUTPUT_DIR.
#
# Called with -P (script mode). Required variables:
#   OUTPUT_DIR       — root directory to scan for .html files (recursive)
#   COMMENT_IN       — path to initial_comment.in template
#   ASCII_ART_FILE   — path to the ASCII art .txt file
#   PROJECT_VERSION  — project version string

cmake_policy(SET CMP0053 NEW)

# ── Validate inputs ───────────────────────────────────────────────────────────
foreach(_var OUTPUT_DIR COMMENT_IN ASCII_ART_FILE PROJECT_VERSION)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "InjectComment.cmake: ${_var} must be defined")
    endif()
endforeach()

if(NOT EXISTS "${OUTPUT_DIR}")
    message(FATAL_ERROR "InjectComment.cmake: OUTPUT_DIR does not exist: ${OUTPUT_DIR}")
endif()

if(NOT EXISTS "${COMMENT_IN}")
    message(FATAL_ERROR "InjectComment.cmake: COMMENT_IN not found: ${COMMENT_IN}")
endif()

# ── Read and prepare the ASCII art ───────────────────────────────────────────
set(_ASCII_FORMATTED "")
if(EXISTS "${ASCII_ART_FILE}")
    file(READ "${ASCII_ART_FILE}" _ASCII_RAW)

    # Normalise line endings
    string(REPLACE "\r\n" "\n" _ASCII_RAW "${_ASCII_RAW}")
    string(REPLACE "\r"   "\n" _ASCII_RAW "${_ASCII_RAW}")

    # Split into lines
    string(REPLACE "\n" ";" _ASCII_LINES "${_ASCII_RAW}")

    foreach(_line IN LISTS _ASCII_LINES)
        string(APPEND _ASCII_FORMATTED "  ${_line}\n")
    endforeach()

    # Remove trailing newline
    string(REGEX REPLACE "\n$" "" _ASCII_FORMATTED "${_ASCII_FORMATTED}")
else()
    message(WARNING "InjectComment.cmake: ASCII art file not found: ${ASCII_ART_FILE}")
    set(_ASCII_FORMATTED "   CToon")
endif()

# ── Read template and substitute ─────────────────────────────────────────────
file(READ "${COMMENT_IN}" _COMMENT_TEMPLATE)
string(REPLACE "CTOON_ASCII_ART"   "${_ASCII_FORMATTED}" _COMMENT "${_COMMENT_TEMPLATE}")
string(REPLACE "CTOON_VERSION"     "${PROJECT_VERSION}"  _COMMENT "${_COMMENT}")

# ── Collect all HTML files under OUTPUT_DIR ───────────────────────────────────
file(GLOB_RECURSE _HTML_FILES "${OUTPUT_DIR}/*.html")

list(LENGTH _HTML_FILES _COUNT)
message(STATUS "InjectComment: processing ${_COUNT} HTML file(s) in ${OUTPUT_DIR}")

# ── Inject comment at the top of each file ────────────────────────────────────
foreach(_html IN LISTS _HTML_FILES)
    file(READ "${_html}" _CONTENT)

    # Idempotent — skip if already injected
    string(FIND "${_CONTENT}" "CToon — C implementation" _ALREADY)
    if(NOT _ALREADY EQUAL -1)
        continue()
    endif()

    # Inject comment right after <!DOCTYPE html>
    if(_CONTENT MATCHES "<!DOCTYPE html>")
        string(REPLACE "<!DOCTYPE html>" "<!DOCTYPE html>\n${_COMMENT}" _CONTENT "${_CONTENT}")
    elseif(_CONTENT MATCHES "<html")
        string(REPLACE "<html" "${_COMMENT}<html" _CONTENT "${_CONTENT}")
    else()
        set(_CONTENT "${_COMMENT}${_CONTENT}")
    endif()

    file(WRITE "${_html}" "${_CONTENT}")
    message(STATUS "InjectComment: injected → ${_html}")
endforeach()

message(STATUS "InjectComment: done.")