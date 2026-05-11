# =============================================================================
# CToon MATLAB MEX Binding
# =============================================================================
#
# Builds the internal MEX gateway (ctoon_mex) and installs the public
# MATLAB wrapper functions that shadow it with clean names and help text:
#
#   ctoon_encode(value)         encode MATLAB value → TOON string
#   ctoon_decode(str)           decode TOON string  → MATLAB value
#   ctoon_read(filepath)        read   .toon file   → MATLAB value
#   ctoon_write(value,filepath) write  .toon file
#   ctoon_encode_json(value)    encode MATLAB value → JSON string
#   ctoon_decode_json(str)      decode JSON string  → MATLAB value
#
# Prerequisites:
#   MATLAB R2018a+  (find_package(Matlab) must succeed)
#   Set Matlab_ROOT_DIR or MATLAB_DIR env var when MATLAB is not on PATH.
#
# Targets:
#   ctoon_mex               MEX shared library (internal gateway)
#   ctoon_test_matlab       run MATLAB unit tests via -batch
#   ctoon_coverage_matlab   lcov/genhtml coverage report for ctoon_mex.c
# =============================================================================

cmake_minimum_required(VERSION 3.19)

# ---------------------------------------------------------------------------
# Locate MATLAB
# ---------------------------------------------------------------------------
find_package(Matlab COMPONENTS MX_LIBRARY MEX_COMPILER QUIET)

if(NOT Matlab_FOUND)
    message(WARNING
        "MATLAB not found — skipping MEX binding.\n"
        "Set Matlab_ROOT_DIR (cmake -DMatlab_ROOT_DIR=/path/to/matlab ...) "
        "or the MATLAB_DIR environment variable to enable it.")
    return()
endif()

message(STATUS "MATLAB found: ${Matlab_ROOT_DIR}")
message(STATUS "MATLAB version: ${Matlab_VERSION_STRING}")

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

# JSON support on by default; disable with -DCTOON_WITHOUT_JSON=ON
if(NOT CTOON_WITHOUT_JSON)
    target_compile_definitions(ctoon_mex PRIVATE CTOON_ENABLE_JSON=1)
endif()

target_compile_options(ctoon_mex PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W3>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

# ---------------------------------------------------------------------------
# Public wrapper .m files
# ---------------------------------------------------------------------------
set(CTOON_MATLAB_WRAPPERS
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_encode.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_decode.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_read.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_write.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_encode_json.m
    ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_decode_json.m
)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS ctoon_mex
    LIBRARY DESTINATION matlab
    RUNTIME DESTINATION matlab   # Windows DLL
)

install(FILES ${CTOON_MATLAB_WRAPPERS}
    DESTINATION matlab
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ctoon_mex.c
    DESTINATION matlab/src
)

# ---------------------------------------------------------------------------
# Convenience: copy wrappers next to the built MEX so the build tree is
# usable directly without installing (addpath('<build>/src/bindings/matlab')).
# ---------------------------------------------------------------------------
foreach(_wrapper IN LISTS CTOON_MATLAB_WRAPPERS)
    get_filename_component(_wname "${_wrapper}" NAME)
    add_custom_command(TARGET ctoon_mex POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wrapper}"
                "$<TARGET_FILE_DIR:ctoon_mex>/${_wname}"
        COMMENT "Copying ${_wname} to MEX output directory"
    )
endforeach()

# ---------------------------------------------------------------------------
# Tests  — ctoon_test_matlab
# ---------------------------------------------------------------------------
set(CTOON_MATLAB_TEST_DIR  ${PROJECT_SOURCE_DIR}/tests/matlab)
set(CTOON_MATLAB_TEST_FILE ${CTOON_MATLAB_TEST_DIR}/test_ctoon_mex.m)
set(CTOON_MEX_BUILD_DIR    $<TARGET_FILE_DIR:ctoon_mex>)

if(EXISTS "${CTOON_MATLAB_TEST_FILE}")

    # Custom target (cmake --build . --target ctoon_test_matlab)
    add_custom_target(ctoon_test_matlab
        COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB tests..."
        COMMAND ${Matlab_MAIN_PROGRAM} -batch
                "addpath('${CMAKE_CURRENT_SOURCE_DIR}'); \
                 addpath('${CTOON_MEX_BUILD_DIR}'); \
                 addpath('${CTOON_MATLAB_TEST_DIR}'); \
                 test_ctoon_mex"
        WORKING_DIRECTORY ${CTOON_MATLAB_TEST_DIR}
        USES_TERMINAL
    )
    add_dependencies(ctoon_test_matlab ctoon_mex)

    # Hook into global test target
    if(TARGET ctoon_test)
        add_dependencies(ctoon_test ctoon_test_matlab)
    endif()

    # CTest registration (picked up by ctest / cmake --build . --target test)
    add_test(NAME test_ctoon_matlab
        COMMAND ${Matlab_MAIN_PROGRAM} -batch
                "addpath('${CMAKE_CURRENT_SOURCE_DIR}'); \
                 addpath('${CTOON_MEX_BUILD_DIR}'); \
                 addpath('${CTOON_MATLAB_TEST_DIR}'); \
                 test_ctoon_mex"
        WORKING_DIRECTORY ${CTOON_MATLAB_TEST_DIR}
    )

else()
    message(STATUS
        "MATLAB test file not found at ${CTOON_MATLAB_TEST_FILE} — "
        "ctoon_test_matlab target skipped.")
endif()

# ---------------------------------------------------------------------------
# Coverage — ctoon_coverage_matlab
# ---------------------------------------------------------------------------
# Instruments ctoon_mex.c with gcov, runs the MATLAB tests, then produces
# an lcov tracefile and an HTML report with genhtml.
# Mirrors the pattern used by the C and C++ coverage targets.

if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)

    set(CTOON_COVERAGE_MATLAB_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/matlab)

    # Recompile MEX with gcov instrumentation
    target_compile_options(ctoon_mex PRIVATE -O0 -g --coverage)
    target_link_options(ctoon_mex    PRIVATE --coverage)

    add_custom_target(ctoon_coverage_matlab
        # ---- reset ----
        COMMAND ${CMAKE_COMMAND} -E remove_directory
                ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/html
        COMMAND ${CMAKE_COMMAND} -E make_directory
                ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/html

        # ---- exercise ----
        COMMAND ${CMAKE_COMMAND} -E echo "-- Running MATLAB tests for coverage..."
        COMMAND ${Matlab_MAIN_PROGRAM} -batch
                "addpath('${CMAKE_CURRENT_SOURCE_DIR}'); \
                 addpath('${CTOON_MEX_BUILD_DIR}'); \
                 addpath('${CTOON_MATLAB_TEST_DIR}'); \
                 test_ctoon_mex"

        # ---- capture ----
        COMMAND ${CMAKE_COMMAND} -E echo "-- Capturing MEX coverage data..."
        COMMAND ${LCOV_EXECUTABLE}
                --capture
                --base-directory ${PROJECT_SOURCE_DIR}
                --directory ${CMAKE_CURRENT_BINARY_DIR}
                --exclude "*/tests/*"
                --no-external
                --rc function_coverage=1
                --output-file ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage_temp.lcov

        # ---- filter: keep only ctoon sources ----
        COMMAND ${CMAKE_COMMAND} -E echo "-- Filtering to ctoon sources..."
        COMMAND ${LCOV_EXECUTABLE}
                --extract
                ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage_temp.lcov
                "*/ctoon*"
                --output-file ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage.lcov
        COMMAND ${CMAKE_COMMAND} -E remove
                ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage_temp.lcov

        # ---- HTML report ----
        COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report..."
        COMMAND ${GENHTML_EXECUTABLE} --quiet
                ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage.lcov
                --output-directory ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/html
                --prefix           ${PROJECT_SOURCE_DIR}
                --title            "CToon MATLAB MEX Coverage"
                --css-file         ${PROJECT_SOURCE_DIR}/tests/genhtml.css
                --dark-mode
        COMMAND ${CMAKE_COMMAND} -E echo
                "-- Report: ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/html/index.html"

        # ---- normalise paths for SonarQube / CI ----
        COMMAND ${CMAKE_COMMAND} -E echo "-- Normalising source paths..."
        COMMAND ${CMAKE_COMMAND}
                -DCOV_FILE=${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage.lcov
                -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
                -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake

        # ---- summary ----
        COMMAND ${CMAKE_COMMAND} -E echo "-- Coverage summary:"
        COMMAND ${LCOV_EXECUTABLE}
                --list ${CTOON_COVERAGE_MATLAB_BINARY_DIR}/coverage.lcov

        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
    )

    add_dependencies(ctoon_coverage_matlab ctoon_mex)
    if(TARGET ctoon_test_matlab)
        add_dependencies(ctoon_coverage_matlab ctoon_test_matlab)
    endif()

    # Hook into the global merged coverage target
    if(TARGET ctoon_coverage)
        add_dependencies(ctoon_coverage ctoon_coverage_matlab)
    endif()

    # iframe wrapper (matches pattern used by other language coverage targets)
    file(MAKE_DIRECTORY ${CTOON_COVERAGE_MATLAB_BINARY_DIR})
    add_custom_command(TARGET ctoon_coverage_matlab POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DIFRAME_SRC="html/index.html"
                -DSECTION_TITLE="MATLAB%20MEX"
                -DLOGO_SVG="${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg"
                -DPROJECT_NAME="${PROJECT_NAME}"
                -DOUTPUT_DIR="${CTOON_COVERAGE_MATLAB_BINARY_DIR}"
                -P "${PROJECT_SOURCE_DIR}/cmake/GenerateIframe.cmake"
        COMMENT "Generating iframe wrapper for MATLAB MEX coverage report"
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/tests
    )

else()
    message(STATUS
        "lcov/genhtml not found — ctoon_coverage_matlab target skipped.")
endif()
