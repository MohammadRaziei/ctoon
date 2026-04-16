# generate_iframe.cmake
# Called via: cmake -P generate_iframe.cmake
# Required CMake variables (passed via -D):
#   CTOON_IFRAME_SRC, CTOON_SECTION_TITLE, CTOON_LOGO_SVG,
#   PROJECT_NAME, INDEX_IN, OUTPUT_DIR

cmake_minimum_required(VERSION 3.19)

# ── Read SVG logo ─────────────────────────────────────────────
if(DEFINED CTOON_LOGO_SVG AND EXISTS "${CTOON_LOGO_SVG}")
    file(READ "${CTOON_LOGO_SVG}" CTOON_LOGO_CONTENT)
else()
    set(CTOON_LOGO_CONTENT "")
endif()

# ── Ensure output dir exists ──────────────────────────────────
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

# ── Generate iframe wrapper ──────────────────────────────────
configure_file(
    "${INDEX_IN}"
    "${OUTPUT_DIR}/index.html"
    @ONLY
)
