# CreateDocs.cmake — C++ documentation (Doxygen)
# Called via: cmake -P CreateDocs.cmake
# Required CMake variables (passed via -D):
#   PROJECT_SOURCE_DIR, PROJECT_VERSION, OUTPUT_DIR, DOXYGEN_EXECUTABLE,
#   FOOTER_IN, DOXYFILE_IN, LOGO_SRC, AWESOME_CSS_DIR

cmake_minimum_required(VERSION 3.19)

# ── Check Doxygen ─────────────────────────────────────────────
if(NOT DEFINED DOXYGEN_EXECUTABLE OR DOXYGEN_EXECUTABLE STREQUAL "DOXYGEN_EXECUTABLE-NOTFOUND")
    message(WARNING "Doxygen not found - C++ documentation skipped")
    return()
endif()

# ── Variables ─────────────────────────────────────────────────
set(CPP_OUT "${OUTPUT_DIR}")
set(CPP_HTML "${CPP_OUT}/html")

file(MAKE_DIRECTORY "${CPP_HTML}")

# ── Configure footer ──────────────────────────────────────────
set(DOXYGEN_FOOTER_OUT "${CPP_OUT}/footer.html")
configure_file("${FOOTER_IN}" "${DOXYGEN_FOOTER_OUT}" @ONLY)

# ── Configure Doxyfile ───────────────────────────────────────
set(DOXYGEN_OUT "${CPP_OUT}/Doxyfile")
configure_file("${DOXYFILE_IN}" "${DOXYGEN_OUT}" @ONLY)

# ── Copy theme logo ─────────────────────────────────────────
if(DEFINED LOGO_SRC)
  file(COPY_FILE "${LOGO_SRC}" "${CPP_HTML}/ctoon-sq.svg")
endif()

# ── Run Doxygen ─────────────────────────────────────────────
execute_process(
  COMMAND "${DOXYGEN_EXECUTABLE}" "${DOXYGEN_OUT}"
  WORKING_DIRECTORY "${CPP_OUT}"
  RESULT_VARIABLE DOXYGEN_RESULT
)

if(NOT DOXYGEN_RESULT EQUAL 0)
  message(FATAL_ERROR "Doxygen failed with exit code ${DOXYGEN_RESULT}")
endif()

message(STATUS "-- C++ docs generated at: ${CPP_HTML}/index.html")
