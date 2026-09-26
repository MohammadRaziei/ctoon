# CreateDocs.cmake — Route Index Page
# Called via: cmake -P CreateDocs.cmake
# Required CMake variables (passed via -D):
#   OUTPUT_DIR, INDEX_IN, PROJECT_VERSION, LOGO_SQ, LOGO_SQ_CTOON,
#   LANGS_FILE (docs/supported_langs.json), LANGS_DIR (docs/, holding each
#   <folder>/index-docs.json + index-install.txt + index-example.txt — see
#   supported_langs.json's own "_comment" field for the format),
#   GENERATE_SCRIPT (docs/generate_index.py)
#
# The tab bars / tab panes themselves are built by GENERATE_SCRIPT, a small
# Python helper — reading JSON and templating HTML fought CMake's own
# string()/file(STRINGS) more than it needed to (an em-dash in a comment
# once desynced file(STRINGS)'s line splitting), and this project already
# requires Python3 for its Sphinx docs, so there's no new dependency.

cmake_minimum_required(VERSION 3.19)

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

if(DEFINED CSS_FILE)
  file(COPY_FILE "${CSS_FILE}" "${OUTPUT_DIR}/ctoon-docs.css")
endif()

# ── Copy logos ────────────────────────────────────────────────
if(DEFINED LOGO_SQ)
  file(COPY_FILE "${LOGO_SQ}" "${OUTPUT_DIR}/ctoon-sq.svg")
endif()
if(DEFINED LOGO_SQ_CTOON)
  file(COPY_FILE "${LOGO_SQ_CTOON}" "${OUTPUT_DIR}/ctoon-sq-ctoon.svg")
endif()

# ── Generate index.html (tab bars / tab panes + placeholder fill) ──
find_package(Python3 COMPONENTS Interpreter REQUIRED)

execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${GENERATE_SCRIPT}"
          --index-in        "${INDEX_IN}"
          --output          "${OUTPUT_DIR}/index.html"
          --langs-file      "${LANGS_FILE}"
          --langs-dir       "${LANGS_DIR}"
          --project-version "${PROJECT_VERSION}"
          --logo-svg        "${LOGO_SQ}"
          --favicon         "ctoon-sq.svg"
  RESULT_VARIABLE GENERATE_RESULT
)
if(NOT GENERATE_RESULT EQUAL 0)
  message(FATAL_ERROR "generate_index.py failed (exit ${GENERATE_RESULT})")
endif()