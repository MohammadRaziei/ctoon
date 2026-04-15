# CreateDocs.cmake — C++ documentation (Doxygen)
# Called via: cmake -P CreateDocs.cmake
# Required CMake variables (passed via -D):
#   PROJECT_SOURCE_DIR, PROJECT_VERSION, OUTPUT_DIR, DOXYGEN_EXECUTABLE,
#   FOOTER_IN, DOXYFILE_IN, LOGO_SRC

cmake_minimum_required(VERSION 3.19)

# ── Check Doxygen ─────────────────────────────────────────────
if(NOT DEFINED DOXYGEN_EXECUTABLE OR DOXYGEN_EXECUTABLE STREQUAL "DOXYGEN_EXECUTABLE-NOTFOUND")
    message(WARNING "Doxygen not found - C++ documentation skipped")
    return()
endif()

# ── Variables ─────────────────────────────────────────────────
set(CPP_OUT "${OUTPUT_DIR}")
set(CPP_HTML "${CPP_OUT}/html")
set(CACHE_DIR "${CPP_OUT}/.cache")

file(MAKE_DIRECTORY "${CPP_HTML}")
file(MAKE_DIRECTORY "${CACHE_DIR}")

# ── Fetch doxygen-awesome-css ────────────────────────────────
set(CSS_URL "https://github.com/jothepro/doxygen-awesome-css/archive/main.zip")
set(CSS_ARCHIVE "${CACHE_DIR}/doxygen-awesome-css.zip")
set(CSS_EXTRACT "${CACHE_DIR}/doxygen-awesome-css")

if(NOT EXISTS "${CSS_ARCHIVE}")
    message(STATUS "Downloading doxygen-awesome-css ...")
    file(DOWNLOAD "${CSS_URL}" "${CSS_ARCHIVE}" STATUS DL_STATUS SHOW_PROGRESS)
    list(GET DL_STATUS 0 DL_CODE)
    if(NOT DL_CODE EQUAL 0)
        list(GET DL_STATUS 1 DL_MSG)
        message(FATAL_ERROR "Failed to download CSS: ${DL_MSG}")
    endif()
endif()

if(NOT EXISTS "${CSS_EXTRACT}/doxygen-awesome.css")
    message(STATUS "Extracting doxygen-awesome-css ...")
    file(ARCHIVE_EXTRACT INPUT "${CSS_ARCHIVE}" DESTINATION "${CACHE_DIR}")
    # github zip extracts to dir-name-main/
    file(GLOB CSS_SRC "${CACHE_DIR}/doxygen-awesome-css-*")
    list(LENGTH CSS_SRC CSS_LEN)
    if(NOT CSS_LEN EQUAL 1)
        message(FATAL_ERROR "Expected one extracted dir, found: ${CSS_SRC}")
    endif()
    list(GET CSS_SRC 0 CSS_SRC_DIR)
    file(RENAME "${CSS_SRC_DIR}" "${CSS_EXTRACT}")
endif()

set(AWESOME_CSS_DIR "${CSS_EXTRACT}")

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
