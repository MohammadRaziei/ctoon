# =============================================================================
# CToon MATLAB MEX Binding
# =============================================================================
#
# Responsibilities:
#   - Compile ctoon_mex via MATLAB Build Tool (buildtool mex)
#   - Install MEX binary + .m wrappers
#   - Provide ctoon_matlab_export target (cmake/CToonMatlabExport.cmake)
#
# Test and coverage targets live in tests/matlab/CMakeLists.txt.
# =============================================================================

cmake_minimum_required(VERSION 3.19)

find_package(Matlab COMPONENTS MX_LIBRARY MEX_COMPILER MAIN_PROGRAM QUIET)

if(NOT Matlab_FOUND)
    message(WARNING
        "MATLAB not found - skipping MEX binding.\n"
        "Set Matlab_ROOT_DIR or the MATLAB_DIR environment variable.")
    return()
endif()

message(STATUS "MATLAB found: ${Matlab_ROOT_DIR}  (${Matlab_VERSION_STRING})")
set(Matlab_EXECUTABLE ${Matlab_MAIN_PROGRAM} -nosplash -noFigureWindows -nodesktop -singleCompThread -nojvm)

# ---------------------------------------------------------------------------
# MEX target - compiled by MATLAB Build Tool via buildtool
# ---------------------------------------------------------------------------
# We declare a custom target (not matlab_add_mex) because the compiler is
# driven by buildfile.m / ctoon_build.m, not by CMake's mex invocation.
# This lets CI use the exact same build path as interactive MATLAB users.


add_custom_target(ctoon_build_mex ALL
    COMMAND ${CMAKE_COMMAND} -E echo "-- MATLAB buildtool mex..."
    COMMAND ${Matlab_EXECUTABLE}
             -sd "${CMAKE_CURRENT_SOURCE_DIR}"
             -batch
             "global BUILD_DIR MEX_SOURCES_PATH MEX_INCLUDE_DIR TEST_DIR; \
              BUILD_DIR='${CMAKE_BINARY_DIR}/matlab/build'; \
              MEX_SOURCES_PATH='${PROJECT_SOURCE_DIR}/src'; \
              MEX_INCLUDE_DIR='${PROJECT_SOURCE_DIR}/include'; \
              TEST_DIR='${PROJECT_SOURCE_DIR}/tests/matlab'; \
              buildtool mex"
    BYPRODUCTS
        ${CMAKE_CURRENT_BINARY_DIR}/ctoon_mex.${Matlab_MEX_EXTENSION}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling ctoon_build_mex via MATLAB Build Tool"
    USES_TERMINAL
    VERBATIM
)   

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
# file(GLOB CTOON_MATLAB_WRAPPERS "${CMAKE_CURRENT_SOURCE_DIR}/*.m")
 
# install(FILES
#         ${CMAKE_CURRENT_BINARY_DIR}/ctoon_mex.${Matlab_MEX_EXTENSION}
#     DESTINATION matlab
# )
# install(FILES ${CTOON_MATLAB_WRAPPERS} DESTINATION matlab)
# install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_mex.c DESTINATION matlab/src)
 
# ---------------------------------------------------------------------------
# Export target  (cmake -P src/bindings/matlab/CToonMatlabExport.cmake)
# ---------------------------------------------------------------------------
add_custom_target(ctoon_matlab_export
    COMMAND ${CMAKE_COMMAND}
            -DEXPORT_DIR=${CMAKE_BINARY_DIR}/matlab/export
            -DMEX_SOURCES_PATH=${PROJECT_SOURCE_DIR}/src
            -DMEX_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/include
            -P ${CMAKE_CURRENT_SOURCE_DIR}/CToonMatlabExport.cmake
    COMMENT "Exporting self-contained MATLAB binding → ${CMAKE_BINARY_DIR}/matlab"
    USES_TERMINAL
)