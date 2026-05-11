# =============================================================================
# CToon MATLAB MEX Binding
# =============================================================================
#
# Builds ctoon_mex (the internal MEX gateway) and installs the public
# .m wrapper functions alongside it.
#
# Responsibilities of this file:
#   - Locate MATLAB
#   - Compile ctoon_mex.c → ctoon_mex.<mexext>
#   - Install the MEX binary and .m wrappers
#   - Copy .m wrappers next to the built MEX (so the build tree is usable
#     without installing)
#
# Test and coverage targets live in tests/matlab/CMakeLists.txt.
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
# MEX gateway
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

# Expose the MEX build directory to the parent scope so tests/matlab can use it
set(CTOON_MEX_OUTPUT_DIR $<TARGET_FILE_DIR:ctoon_mex> PARENT_SCOPE)

# ---------------------------------------------------------------------------
# Public .m wrappers
# ---------------------------------------------------------------------------
set(CTOON_MATLAB_WRAPPERS
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_encode.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_decode.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_read.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_write.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_encode_json.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_decode_json.m
)

# Copy wrappers next to the built MEX so one addpath() is enough
foreach(_w IN LISTS CTOON_MATLAB_WRAPPERS)
    get_filename_component(_name "${_w}" NAME)
    add_custom_command(TARGET ctoon_mex POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_w}" "$<TARGET_FILE_DIR:ctoon_mex>/${_name}"
        COMMENT "Copying ${_name} → MEX output dir"
    )
endforeach()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS ctoon_mex
    LIBRARY DESTINATION matlab
    RUNTIME DESTINATION matlab
)
install(FILES ${CTOON_MATLAB_WRAPPERS} DESTINATION matlab)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_mex.c DESTINATION matlab/src)
