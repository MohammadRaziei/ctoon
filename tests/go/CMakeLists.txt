# ---------------------------------------------------------------------------
# Go binding tests
# ---------------------------------------------------------------------------
# Mirrors the pattern used by tests/python/CMakeLists.txt.
# Requires Go >= 1.21 on PATH.
# ---------------------------------------------------------------------------

set(CTOON_GO_TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CTOON_TEST_DATA_DIR      ${CTOON_TEST_SOURCE_DIR}/data)

# ── Locate Go toolchain ──────────────────────────────────────────────────────
find_program(GO_EXECUTABLE go)

if(NOT GO_EXECUTABLE)
    message(WARNING "Go toolchain not found. Go tests will be skipped.")
    message(WARNING "Install Go >= 1.21 and make sure 'go' is on PATH.")
    return()
endif()

execute_process(
    COMMAND ${GO_EXECUTABLE} version
    OUTPUT_VARIABLE GO_VERSION_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Found Go: ${GO_VERSION_OUTPUT}")

# ── CTest integration ────────────────────────────────────────────────────────
add_test(
    NAME ctoon_go_tests
    COMMAND ${GO_EXECUTABLE} test
            -v
            -count=1
            ${CTOON_GO_TEST_SOURCE_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/src/bindings/go
)
set_tests_properties(ctoon_go_tests PROPERTIES
    ENVIRONMENT "CTOON_TEST_DATA_DIR=${CTOON_TEST_DATA_DIR}"
    TIMEOUT 120
)

# ── ctoon_test_go custom target ──────────────────────────────────────────────
add_custom_target(ctoon_test_go
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Go tests..."
    COMMAND ${CMAKE_COMMAND} -E env
                "CTOON_TEST_DATA_DIR=${CTOON_TEST_DATA_DIR}"
            ${GO_EXECUTABLE} test
                -v
                -count=1
                ${CTOON_GO_TEST_SOURCE_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/src/bindings/go
    USES_TERMINAL
)

# Hook into the top-level ctoon_test target
add_dependencies(ctoon_test ctoon_test_go)

# ── ctoon_coverage_go custom target ─────────────────────────────────────────
set(CTOON_COVERAGE_GO_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/go)

execute_process(
    COMMAND ${GO_EXECUTABLE} tool cover -V
    OUTPUT_QUIET
    ERROR_QUIET
    RESULT_VARIABLE GO_COVER_CHECK
)

if(GO_COVER_CHECK EQUAL 0)
    file(MAKE_DIRECTORY ${CTOON_COVERAGE_GO_BINARY_DIR})

    add_custom_target(ctoon_coverage_go
        # Clean previous artefacts
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CTOON_COVERAGE_GO_BINARY_DIR}/html
        COMMAND ${CMAKE_COMMAND} -E make_directory   ${CTOON_COVERAGE_GO_BINARY_DIR}/html

        # Run tests with -coverprofile
        COMMAND ${CMAKE_COMMAND} -E echo "-- Capturing Go coverage..."
        COMMAND ${CMAKE_COMMAND} -E env
                    "CTOON_TEST_DATA_DIR=${CTOON_TEST_DATA_DIR}"
                ${GO_EXECUTABLE} test
                    -v
                    -count=1
                    -coverprofile=${CTOON_COVERAGE_GO_BINARY_DIR}/coverage.out
                    -coverpkg=github.com/mohammadraziei/ctoon/...
                    ${CTOON_GO_TEST_SOURCE_DIR}

        # Print function-level summary
        COMMAND ${CMAKE_COMMAND} -E echo "-- Coverage summary:"
        COMMAND ${GO_EXECUTABLE} tool cover
                    -func=${CTOON_COVERAGE_GO_BINARY_DIR}/coverage.out

        # Generate HTML report
        COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report..."
        COMMAND ${GO_EXECUTABLE} tool cover
                    -html=${CTOON_COVERAGE_GO_BINARY_DIR}/coverage.out
                    -o ${CTOON_COVERAGE_GO_BINARY_DIR}/html/index.html

        COMMAND ${CMAKE_COMMAND} -E echo
                "-- HTML entry point created here: ${CTOON_COVERAGE_GO_BINARY_DIR}/html/index.html"

        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/src/bindings/go
        USES_TERMINAL
    )

    add_dependencies(ctoon_coverage_go ctoon_test_go)
    add_dependencies(ctoon_coverage ctoon_coverage_go)

    # Iframe wrapper (same pattern as C / C++ / Python)
    add_custom_command(TARGET ctoon_coverage_go POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DIFRAME_SRC="html/index.html"
            -DSECTION_TITLE="Go-Bindings"
            -DLOGO_SVG="${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg"
            -DPROJECT_NAME="${PROJECT_NAME}"
            -DOUTPUT_DIR="${CTOON_COVERAGE_GO_BINARY_DIR}"
            -P "${CTOON_TEST_SOURCE_DIR}/GenerateIframe.cmake"
        COMMENT "Generating iframe wrapper for Go coverage report"
        WORKING_DIRECTORY ${CTOON_TEST_SOURCE_DIR}
    )
else()
    message(WARNING "go tool cover not available; Go coverage target will be skipped.")
endif()