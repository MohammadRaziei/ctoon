# =============================================================================
# CToon MATLAB MEX Binding
# =============================================================================
#
# Sole responsibility: compile ctoon_mex.c into a MEX shared library and
# install it together with the public .m wrappers.
#
# Building from MATLAB (recommended):
#   cd src/bindings/matlab
#   buildtool mex          % uses buildfile.m → mexTask()
#
# Building via CMake (for CI / system builds without interactive MATLAB):
#   cmake -B build -DMatlab_ROOT_DIR=/path/to/matlab
#   cmake --build build --target ctoon_mex
#
# Test and coverage targets are in tests/matlab/CMakeLists.txt and also
# delegate to buildtool (buildtool test / buildtool coverage).
# =============================================================================

cmake_minimum_required(VERSION 3.19)

# ---------------------------------------------------------------------------
# Locate MATLAB
# ---------------------------------------------------------------------------
find_package(Matlab COMPONENTS MX_LIBRARY MEX_COMPILER QUIET)

if(NOT Matlab_FOUND)
    message(WARNING
        "MATLAB not found — skipping MEX binding.\n"
        "Set Matlab_ROOT_DIR or the MATLAB_DIR environment variable.")
    return()
endif()

message(STATUS "MATLAB found: ${Matlab_ROOT_DIR}  (${Matlab_VERSION_STRING})")

# ---------------------------------------------------------------------------
# MEX gateway  (ctoon_mex)
# ---------------------------------------------------------------------------
matlab_add_mex(
    NAME        ctoon_mex
    SRC         ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_mex.c
    LINK_TO     ctoon::ctoon
    OUTPUT_NAME ctoon_mex
)

target_include_directories(ctoon_mex PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(NOT CTOON_WITHOUT_JSON)
    target_compile_definitions(ctoon_mex PRIVATE CTOON_ENABLE_JSON=1)
endif()

target_compile_options(ctoon_mex PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W3>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
set(CTOON_MATLAB_WRAPPERS
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_encode.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_decode.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_read.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_write.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_encode_json.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_decode_json.m
)

install(TARGETS ctoon_mex
    LIBRARY DESTINATION matlab
    RUNTIME DESTINATION matlab
)
install(FILES ${CTOON_MATLAB_WRAPPERS}
              ${CMAKE_CURRENT_SOURCE_DIR}/buildfile.m
    DESTINATION matlab
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_mex.c
    DESTINATION matlab/src
)
