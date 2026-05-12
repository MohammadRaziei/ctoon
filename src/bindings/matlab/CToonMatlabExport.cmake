# =============================================================================
# CToonMatlabExport.cmake
#
# This file lives in src/bindings/matlab/ alongside the .m files it exports.
# CMAKE_CURRENT_LIST_DIR therefore points directly at the binding directory.
#
# Usage (cmake -P):
#   cmake \
#     -DEXPORT_DIR=<output_dir>           (required)
#     -DMEX_SOURCES_PATH=<dir_of_ctoon.c> (required)
#     -DMEX_INCLUDE_DIR=<dir_of_ctoon.h>  (required)
#     -P src/bindings/matlab/CToonMatlabExport.cmake
#
# Output layout:
#   EXPORT_DIR/
#     ctoon.c
#     ctoon.h
#     ctoon_mex.c
#     ctoon_build.m      ← default paths patched to point at EXPORT_DIR
#     ctoon_install.m
#     ctoon_encode.m
#     ctoon_decode.m
#     ctoon_read.m
#     ctoon_write.m
# =============================================================================

foreach(_var EXPORT_DIR MEX_SOURCES_PATH MEX_INCLUDE_DIR)
    if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
        message(FATAL_ERROR
            "CToonMatlabExport: ${_var} must be set.\n"
            "Usage: cmake -DEXPORT_DIR=... -DMEX_SOURCES_PATH=... "
            "-DMEX_INCLUDE_DIR=... -P src/bindings/matlab/CToonMatlabExport.cmake")
    endif()
endforeach()

file(MAKE_DIRECTORY "${EXPORT_DIR}")
message(STATUS "CToonMatlabExport → ${EXPORT_DIR}")

# ---------------------------------------------------------------------------
# Copy .m wrappers — glob all, exclude buildfile.m (requires R2022b+ Build
# Tool and project-specific globals; end users only need ctoon_build.m)
# ---------------------------------------------------------------------------
file(GLOB _all_m "${CMAKE_CURRENT_LIST_DIR}/*.m")

foreach(_f IN LISTS _all_m)
    cmake_path(GET _f FILENAME _name)
    if(NOT _name STREQUAL "buildfile.m")
        file(COPY "${_f}" DESTINATION "${EXPORT_DIR}")
        message(STATUS "  copied ${_name}")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Copy ctoon_mex.c
# ---------------------------------------------------------------------------
file(COPY "${CMAKE_CURRENT_LIST_DIR}/ctoon_mex.c" DESTINATION "${EXPORT_DIR}")
message(STATUS "  copied ctoon_mex.c")

# ---------------------------------------------------------------------------
# Copy ctoon.c and ctoon.h from caller-supplied paths
# ---------------------------------------------------------------------------
foreach(_src "${MEX_SOURCES_PATH}/ctoon.c" "${MEX_INCLUDE_DIR}/ctoon.h")
    if(NOT EXISTS "${_src}")
        message(FATAL_ERROR "CToonMatlabExport: file not found: ${_src}")
    endif()
    cmake_path(GET _src FILENAME _name)
    file(COPY "${_src}" DESTINATION "${EXPORT_DIR}")
    message(STATUS "  copied ${_name}")
endforeach()

# ---------------------------------------------------------------------------
# Patch ctoon_build.m — replace repo-relative defaults with 'here'
# In the export, ctoon.c and ctoon.h sit right next to ctoon_build.m
# ---------------------------------------------------------------------------
file(READ "${EXPORT_DIR}/ctoon_build.m" _content)

string(REPLACE
    "mexSourcesPath = fullfile(repoRoot, 'src');"
    "mexSourcesPath = here;"
    _content "${_content}")

string(REPLACE
    "mexIncludeDir = fullfile(repoRoot, 'include');"
    "mexIncludeDir = here;"
    _content "${_content}")

file(WRITE "${EXPORT_DIR}/ctoon_build.m" "${_content}")
message(STATUS "  patched ctoon_build.m (default paths → export dir)")

# ---------------------------------------------------------------------------
message(STATUS "CToonMatlabExport complete.")
message(STATUS "  Build:   cd ${EXPORT_DIR} && matlab -batch \"ctoon_build\"")
message(STATUS "  Install: cd ${EXPORT_DIR} && matlab -batch \"ctoon_install\"")