# =============================================================================
# CToonMatlabExport.cmake
#
# Exports the MATLAB MEX binding into a self-contained directory so it can
# be used without a full CMake/build-system install — useful for distributing
# to MATLAB users who just want to run ctoon_build or ctoon_install.
#
# Usage (cmake -P):
#   cmake \
#     -DEXPORT_DIR=<output_dir>           (required)
#     -DMEX_SOURCES_PATH=<dir_of_ctoon.c> (required)
#     -DMEX_INCLUDE_DIR=<dir_of_ctoon.h>  (required)
#     -P cmake/CToonMatlabExport.cmake
#
# What it does:
#   1. Copies all .m files from src/bindings/matlab/ into EXPORT_DIR
#   2. Copies ctoon_mex.c into EXPORT_DIR
#   3. Copies ctoon.c  into EXPORT_DIR  (so the export is self-contained)
#   4. Copies ctoon.h  into EXPORT_DIR
#   5. In the EXPORT_DIR copy of ctoon_build.m, replaces the placeholder
#      values of MEX_SOURCES_PATH and MEX_INCLUDE_DIR with the paths
#      supplied so the exported ctoon_build works out-of-the-box without
#      any variables needing to be set in the workspace.
#
# The exported directory contains everything needed to compile and use the
# binding on any MATLAB installation (R2014b+, no CMake required):
#
#   EXPORT_DIR/
#     ctoon.c
#     ctoon.h
#     ctoon_mex.c
#     ctoon_build.m      ← MEX_SOURCES_PATH / MEX_INCLUDE_DIR pre-filled
#     ctoon_install.m
#     ctoon_encode.m
#     ctoon_decode.m
#     ctoon_read.m
#     ctoon_write.m
#     buildfile.m
# =============================================================================

# ---------------------------------------------------------------------------
# Validate required inputs
# ---------------------------------------------------------------------------
foreach(_var EXPORT_DIR MEX_SOURCES_PATH MEX_INCLUDE_DIR)
    if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
        message(FATAL_ERROR
            "CToonMatlabExport: ${_var} must be set.\n"
            "Usage: cmake -DEXPORT_DIR=... -DMEX_SOURCES_PATH=... "
            "-DMEX_INCLUDE_DIR=... -P cmake/CToonMatlabExport.cmake")
    endif()
endforeach()

# Resolve script location (this file lives in <repo>/cmake/)
get_filename_component(_cmake_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(_repo_root "${_cmake_dir}/.." ABSOLUTE)

set(_binding_dir "${_repo_root}/src/bindings/matlab")

# ---------------------------------------------------------------------------
# Create output directory
# ---------------------------------------------------------------------------
file(MAKE_DIRECTORY "${EXPORT_DIR}")
message(STATUS "CToonMatlabExport → ${EXPORT_DIR}")

# ---------------------------------------------------------------------------
# 1-4. Copy source files
# ---------------------------------------------------------------------------
set(_m_files
    ctoon_build.m
    ctoon_install.m
    ctoon_encode.m
    ctoon_decode.m
    ctoon_read.m
    ctoon_write.m
    buildfile.m
)

foreach(_f IN LISTS _m_files)
    file(COPY "${_binding_dir}/${_f}" DESTINATION "${EXPORT_DIR}")
    message(STATUS "  copied ${_f}")
endforeach()

file(COPY "${_binding_dir}/ctoon_mex.c" DESTINATION "${EXPORT_DIR}")
message(STATUS "  copied ctoon_mex.c")

# ctoon.c and ctoon.h — locate from MEX_SOURCES_PATH / MEX_INCLUDE_DIR
set(_ctoon_c "${MEX_SOURCES_PATH}/ctoon.c")
set(_ctoon_h "${MEX_INCLUDE_DIR}/ctoon.h")

foreach(_src "${_ctoon_c}" "${_ctoon_h}")
    if(NOT EXISTS "${_src}")
        message(FATAL_ERROR "CToonMatlabExport: file not found: ${_src}")
    endif()
    get_filename_component(_fname "${_src}" NAME)
    file(COPY "${_src}" DESTINATION "${EXPORT_DIR}")
    message(STATUS "  copied ${_fname}")
endforeach()

# ---------------------------------------------------------------------------
# 5. Patch ctoon_build.m — replace default fallback paths with real paths
#
# The two lines we target in ctoon_build.m are:
#   mexSourcesPath = ws_get('MEX_SOURCES_PATH', here);
#   mexIncludeDir  = ws_get('MEX_INCLUDE_DIR',  here);
#
# In the exported copy the sources/headers live right next to ctoon_build.m,
# so we replace 'here' with '.' (the export dir itself).  This means the
# exported ctoon_build.m works with just:
#   cd <EXPORT_DIR>; ctoon_build
# without setting any workspace variables.
# ---------------------------------------------------------------------------
set(_build_m "${EXPORT_DIR}/ctoon_build.m")

file(READ "${_build_m}" _content)

string(REPLACE
    "mexSourcesPath = ws_get('MEX_SOURCES_PATH', here);"
    "mexSourcesPath = ws_get('MEX_SOURCES_PATH', here); % default: export dir"
    _content "${_content}")

# Replace the default fallback for MEX_SOURCES_PATH
string(REPLACE
    "ws_get('MEX_SOURCES_PATH', here)"
    "ws_get('MEX_SOURCES_PATH', fileparts(mfilename('fullpath')))"
    _content "${_content}")

# Replace the default fallback for MEX_INCLUDE_DIR
string(REPLACE
    "ws_get('MEX_INCLUDE_DIR',  here)"
    "ws_get('MEX_INCLUDE_DIR',  fileparts(mfilename('fullpath')))"
    _content "${_content}")

file(WRITE "${_build_m}" "${_content}")
message(STATUS "  patched ctoon_build.m (MEX_SOURCES_PATH / MEX_INCLUDE_DIR → export dir)")

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
message(STATUS "CToonMatlabExport complete.")
message(STATUS "  To build MEX:    cd ${EXPORT_DIR} && matlab -batch \"ctoon_build\"")
message(STATUS "  To install:      cd ${EXPORT_DIR} && matlab -batch \"ctoon_install\"")
