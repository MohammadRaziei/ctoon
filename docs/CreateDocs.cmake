# CreateDocs.cmake — Route Index Page
# Called via: cmake -P CreateDocs.cmake
# Required CMake variables (passed via -D):
#   OUTPUT_DIR, INDEX_IN, PROJECT_VERSION, LOGO_SQ, LOGO_SQ_CTOON,
#   LANGS_FILE (docs/supported_langs.txt), LANGS_DIR (docs/, holding each
#   <folder>/index-{docs,install,example}.txt fragment — see
#   supported_langs.txt's own header comment for the format)

cmake_minimum_required(VERSION 3.19)

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

if(DEFINED CSS_FILE)
  file(COPY_FILE "${CSS_FILE}" "${OUTPUT_DIR}/ctoon-docs.css")
endif()

# ── Read SVG logo for embedding ───────────────────────────────
if(DEFINED LOGO_SQ AND EXISTS "${LOGO_SQ}")
  file(READ "${LOGO_SQ}" LOGO_CONTENT)
endif()

# ── Copy logos ────────────────────────────────────────────────
if(DEFINED LOGO_SQ)
  file(COPY_FILE "${LOGO_SQ}" "${OUTPUT_DIR}/ctoon-sq.svg")
endif()
if(DEFINED LOGO_SQ_CTOON)
  file(COPY_FILE "${LOGO_SQ_CTOON}" "${OUTPUT_DIR}/ctoon-sq-ctoon.svg")
endif()

# ── Build the per-language tab bars / tab panes ─────────────────
# One language section (CLI, C, C++, ...) drives three tabbed panels —
# docs / install / example — from supported_langs.txt plus each
# language's three fragment files. This is the same "build a big string,
# then string(REPLACE) it into a @PLACEHOLDER@" template technique
# tests/CMakeLists.txt uses for @COVERAGE_DATA_JSON@, just emitting HTML
# instead of JSON.

# Named SVG glyphs for languages without a suitable Font Awesome icon
# (see supported_langs.txt's "icon kind" column).
set(ICON_SVG_layers "<svg class=\"tab-svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5\"/></svg>")

set(PANELS docs install example)
foreach(PANEL ${PANELS})
  set(${PANEL}_TABS "")
  set(${PANEL}_CONTENT "")
endforeach()

file(STRINGS "${LANGS_FILE}" LANG_LINES)
set(FIRST_LANG TRUE)
foreach(LANG_LINE ${LANG_LINES})
  string(STRIP "${LANG_LINE}" LANG_LINE)
  if(LANG_LINE STREQUAL "" OR LANG_LINE MATCHES "^#")
    continue()
  endif()

  string(REPLACE "|" ";" LANG_FIELDS "${LANG_LINE}")
  list(LENGTH LANG_FIELDS LANG_FIELD_COUNT)
  if(NOT LANG_FIELD_COUNT EQUAL 4)
    message(FATAL_ERROR "${LANGS_FILE}: malformed line (need 4 '|'-separated fields): ${LANG_LINE}")
  endif()
  list(GET LANG_FIELDS 0 LANG_FOLDER)
  list(GET LANG_FIELDS 1 LANG_NAME)
  list(GET LANG_FIELDS 2 LANG_ICON_KIND)
  list(GET LANG_FIELDS 3 LANG_ICON_VALUE)
  string(STRIP "${LANG_FOLDER}"     LANG_FOLDER)
  string(STRIP "${LANG_NAME}"       LANG_NAME)
  string(STRIP "${LANG_ICON_KIND}"  LANG_ICON_KIND)
  string(STRIP "${LANG_ICON_VALUE}" LANG_ICON_VALUE)

  if(LANG_ICON_KIND STREQUAL "fa")
    set(LANG_ICON_HTML "<i class=\"${LANG_ICON_VALUE}\"></i>")
  elseif(LANG_ICON_KIND STREQUAL "svg")
    if(NOT DEFINED ICON_SVG_${LANG_ICON_VALUE})
      message(FATAL_ERROR "${LANGS_FILE}: unknown svg icon '${LANG_ICON_VALUE}' for '${LANG_FOLDER}'")
    endif()
    set(LANG_ICON_HTML "${ICON_SVG_${LANG_ICON_VALUE}}")
  else()
    message(FATAL_ERROR "${LANGS_FILE}: unknown icon kind '${LANG_ICON_KIND}' for '${LANG_FOLDER}' (expected 'fa' or 'svg')")
  endif()

  if(FIRST_LANG)
    set(LANG_ACTIVE_BTN  " active")
    set(LANG_ACTIVE_PANE " active")
    set(FIRST_LANG FALSE)
  else()
    set(LANG_ACTIVE_BTN  "")
    set(LANG_ACTIVE_PANE "")
  endif()

  foreach(PANEL ${PANELS})
    set(FRAG_FILE "${LANGS_DIR}/${LANG_FOLDER}/index-${PANEL}.txt")
    if(NOT EXISTS "${FRAG_FILE}")
      message(FATAL_ERROR "Missing fragment for '${LANG_FOLDER}': ${FRAG_FILE}")
    endif()
    file(READ "${FRAG_FILE}" FRAG_CONTENT)

    string(APPEND ${PANEL}_TABS
      "            <button class=\"tab-btn${LANG_ACTIVE_BTN}\" data-tab=\"${PANEL}-${LANG_FOLDER}\" onclick=\"switchTab('${PANEL}','${LANG_FOLDER}')\">\n"
      "                ${LANG_ICON_HTML} ${LANG_NAME}\n"
      "            </button>\n"
    )
    string(APPEND ${PANEL}_CONTENT
      "        <div class=\"tab-content${LANG_ACTIVE_PANE}\" id=\"${PANEL}-${LANG_FOLDER}\">\n"
      "${FRAG_CONTENT}"
      "        </div>\n\n"
    )
  endforeach()
endforeach()

# ── Generate index.html ───────────────────────────────────────
# Panel placeholders go first: some fragments (e.g. cli/index-docs.txt's
# "v@PROJECT_VERSION@" badge) contain their own @PROJECT_VERSION@ etc.
# placeholders, which only exist in INDEX_CONTENT once their fragment is
# spliced in, so the general substitutions below must run after.
file(READ "${INDEX_IN}" INDEX_CONTENT)
string(REPLACE "@DOCS_TABS@"       "${docs_TABS}"       INDEX_CONTENT "${INDEX_CONTENT}")
string(REPLACE "@DOCS_CONTENT@"    "${docs_CONTENT}"    INDEX_CONTENT "${INDEX_CONTENT}")
string(REPLACE "@INSTALL_TABS@"    "${install_TABS}"    INDEX_CONTENT "${INDEX_CONTENT}")
string(REPLACE "@INSTALL_CONTENT@" "${install_CONTENT}" INDEX_CONTENT "${INDEX_CONTENT}")
string(REPLACE "@EXAMPLE_TABS@"    "${example_TABS}"    INDEX_CONTENT "${INDEX_CONTENT}")
string(REPLACE "@EXAMPLE_CONTENT@" "${example_CONTENT}" INDEX_CONTENT "${INDEX_CONTENT}")
string(REPLACE "@PROJECT_VERSION@" "${PROJECT_VERSION}" INDEX_CONTENT "${INDEX_CONTENT}")
string(REPLACE "@LOGO_CONTENT@"    "${LOGO_CONTENT}"    INDEX_CONTENT "${INDEX_CONTENT}")
string(REPLACE "@FAVICON@"         "ctoon-sq.svg"       INDEX_CONTENT "${INDEX_CONTENT}")
file(WRITE "${OUTPUT_DIR}/index.html" "${INDEX_CONTENT}")

message(STATUS "-- Route index created at: ${OUTPUT_DIR}/index.html")