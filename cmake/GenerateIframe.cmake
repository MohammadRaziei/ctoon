# GenerateIframe.cmake
# Called via: cmake -P GenerateIframe.cmake
# Required CMake variables (passed via -D):
#   IFRAME_SRC, SECTION_TITLE, LOGO_SVG,
#   PROJECT_NAME, OUTPUT_DIR

cmake_minimum_required(VERSION 3.19)

# ── Read SVG logo ─────────────────────────────────────────────
if(DEFINED LOGO_SVG AND EXISTS "${LOGO_SVG}")
    file(READ "${LOGO_SVG}" LOGO_CONTENT)
endif()

# ── Ensure output dir exists ──────────────────────────────────
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
string(REPLACE "%20" " " SECTION_TITLE "${SECTION_TITLE}")

# ── Generate iframe wrapper ──────────────────────────────────
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/coverage_iframe.html.in"
    "${OUTPUT_DIR}/index.html"
    @ONLY
)
