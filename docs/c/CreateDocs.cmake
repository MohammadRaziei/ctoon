# CreateDocs.cmake — C documentation (Doxygen)
# Called via: cmake -P CreateDocs.cmake
# Required CMake variables (passed via -D):
#   PROJECT_SOURCE_DIR, PROJECT_VERSION, OUTPUT_DIR, DOXYGEN_EXECUTABLE,
#   FOOTER_IN, DOXYFILE_IN, LOGO_SRC

cmake_minimum_required(VERSION 3.19)

# ── Check Doxygen ─────────────────────────────────────────────
if(NOT DEFINED DOXYGEN_EXECUTABLE OR DOXYGEN_EXECUTABLE STREQUAL "DOXYGEN_EXECUTABLE-NOTFOUND")
    message(WARNING "Doxygen not found - C documentation skipped")
    return()
endif()

# ── Find doxygen-awesome-css from pip package ─────────────────
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} -m doxygen_awesome_css --path
    OUTPUT_VARIABLE AWESOME_CSS_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT AWESOME_CSS_DIR OR NOT EXISTS "${AWESOME_CSS_DIR}/doxygen-awesome.css")
    message(WARNING "doxygen-awesome-css not found - C docs will use default theme")
    set(AWESOME_HTML_STYLESHEET "")
    set(AWESOME_HTML_EXTRA_STYLESHEET "")
else()
    set(AWESOME_HTML_STYLESHEET "${AWESOME_CSS_DIR}/doxygen-awesome.css")
    set(AWESOME_HTML_EXTRA_STYLESHEET "${AWESOME_CSS_DIR}/doxygen-awesome-sidebar-only.css")
endif()

# ── Variables ─────────────────────────────────────────────────
set(C_OUT "${OUTPUT_DIR}")
set(C_HTML "${C_OUT}/html")

file(MAKE_DIRECTORY "${C_HTML}")

# ── Configure footer ──────────────────────────────────────────
set(DOXYGEN_FOOTER_OUT "${C_OUT}/footer.html")
configure_file("${FOOTER_IN}" "${DOXYGEN_FOOTER_OUT}" @ONLY)

# ── Configure Doxyfile ───────────────────────────────────────
set(DOXYGEN_OUT "${C_OUT}/Doxyfile")
configure_file("${DOXYFILE_IN}" "${DOXYGEN_OUT}" @ONLY)

# ── Copy theme logo ─────────────────────────────────────────
if(DEFINED LOGO_SRC)
  file(COPY_FILE "${LOGO_SRC}" "${C_HTML}/ctoon-sq.svg")
endif()

# ── Run Doxygen ─────────────────────────────────────────────
execute_process(
  COMMAND "${DOXYGEN_EXECUTABLE}" "${DOXYGEN_OUT}"
  WORKING_DIRECTORY "${C_OUT}"
  RESULT_VARIABLE DOXYGEN_RESULT
)

if(NOT DOXYGEN_RESULT EQUAL 0)
  message(FATAL_ERROR "Doxygen failed with exit code ${DOXYGEN_RESULT}")
endif()

message(STATUS "-- C docs generated at: ${C_HTML}/index.html")
