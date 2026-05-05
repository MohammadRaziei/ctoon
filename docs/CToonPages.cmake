# CToonPages.cmake
# CMake module for building Markdown-based documentation pages.
#
# Provides:
#   ctoon_add_page(
#       NAME     <name>           # unique identifier, used as target name and output subdir
#       SOURCE   <file.md>        # path to the Markdown source file
#       TITLE    <"Page Title">   # displayed in <title> and page header
#       [NAV_LABEL <"Label">]     # optional short label for topbar nav (defaults to TITLE)
#       [OUTPUT_DIR <dir>]        # optional override for output dir (default: ${CTOON_DOCS_OUT}/<name>)
#   )
#
# Each page is built as:
#   <OUTPUT_DIR>/<name>/index.html
#
# Prerequisites (must be set before including this file):
#   Python3_EXECUTABLE   — Python 3 interpreter (from find_package(Python3))
#   CTOON_DOCS_OUT       — root output directory for all docs
#   CTOON_DOCS_SRC       — source directory of the docs folder (contains page.html.in, ctoon-docs.css)
#   LOGO_SQ              — path to the SVG logo file (for embedding)
#   PROJECT_VERSION      — project version string

cmake_minimum_required(VERSION 3.19)

# ── Verify prerequisites ──────────────────────────────────────────────────────
foreach(_var Python3_EXECUTABLE CTOON_DOCS_OUT CTOON_DOCS_SRC PROJECT_VERSION)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "CtoonPages: ${_var} must be defined before including CtoonPages.cmake")
    endif()
endforeach()

# Check that markdown-it-py is available
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import markdown_it"
    OUTPUT_QUIET ERROR_QUIET
    RESULT_VARIABLE _MDIT_CHECK
)
if(NOT _MDIT_CHECK EQUAL 0)
    message(FATAL_ERROR
        "CtoonPages: 'markdown-it-py' Python package not found.\n"
        "Install it with:  pip install markdown-it-py"
    )
endif()

# ── Internal helper: path to the convert script ──────────────────────────────
set(_CTOON_CONVERT_SCRIPT "${CTOON_DOCS_SRC}/convert_page.py"
    CACHE INTERNAL "Path to the ctoon md→html conversion script")

set(_CTOON_PAGE_TEMPLATE "${CTOON_DOCS_SRC}/page.html.in"
    CACHE INTERNAL "Path to the ctoon page HTML template")

set(_CTOON_PAGE_CSS "${CTOON_DOCS_SRC}/ctoon-docs.css"
    CACHE INTERNAL "Path to ctoon-docs.css")

# ── ctoon_add_page ────────────────────────────────────────────────────────────
function(ctoon_add_page)
    cmake_parse_arguments(ARG "" "NAME;SOURCE;TITLE;NAV_LABEL;OUTPUT_DIR" "" ${ARGN})

    if(NOT ARG_NAME OR NOT ARG_SOURCE OR NOT ARG_TITLE)
        message(FATAL_ERROR "ctoon_add_page: NAME, SOURCE, and TITLE are required")
    endif()

    if(NOT ARG_NAV_LABEL)
        set(ARG_NAV_LABEL "${ARG_TITLE}")
    endif()

    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR "${CTOON_DOCS_OUT}/${ARG_NAME}")
    endif()

    set(_OUTPUT_HTML "${ARG_OUTPUT_DIR}/index.html")

    # Resolve absolute source path
    if(NOT IS_ABSOLUTE "${ARG_SOURCE}")
        set(ARG_SOURCE "${CTOON_DOCS_SRC}/${ARG_SOURCE}")
    endif()

    add_custom_command(
        OUTPUT "${_OUTPUT_HTML}"
        COMMAND ${Python3_EXECUTABLE} "${_CTOON_CONVERT_SCRIPT}"
            --input     "${ARG_SOURCE}"
            --output    "${_OUTPUT_HTML}"
            --title     "${ARG_TITLE}"
            --nav-label "${ARG_NAV_LABEL}"
            --template  "${_CTOON_PAGE_TEMPLATE}"
            --css       "${_CTOON_PAGE_CSS}"
            --logo      "${LOGO_SQ}"
            --version   "${PROJECT_VERSION}"
        DEPENDS
            "${ARG_SOURCE}"
            "${_CTOON_CONVERT_SCRIPT}"
            "${_CTOON_PAGE_TEMPLATE}"
            "${_CTOON_PAGE_CSS}"
        COMMENT "Building page: ${ARG_TITLE}"
        VERBATIM
    )

    add_custom_target("ctoon_docs_page_${ARG_NAME}" DEPENDS "${_OUTPUT_HTML}")

    # Register in the global list so ctoon_docs can depend on all pages
    set_property(GLOBAL APPEND PROPERTY CTOON_PAGE_TARGETS "ctoon_docs_page_${ARG_NAME}")

    message(STATUS "CtoonPages: registered page '${ARG_NAME}' → ${_OUTPUT_HTML}")
endfunction()

# ── ctoon_add_all_pages_dependency ───────────────────────────────────────────
# Call this after all ctoon_add_page() calls to wire pages into ctoon_docs.
macro(ctoon_finalize_pages TARGET)
    get_property(_page_targets GLOBAL PROPERTY CTOON_PAGE_TARGETS)
    if(_page_targets)
        add_dependencies(${TARGET} ${_page_targets})
    endif()
endmacro()