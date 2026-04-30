set(CTOON_TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CTOON_TEST_BINARY_DIR ${CMAKE_BINARY_DIR}/tests)
set(CTOON_COVERAGE_BINARY_DIR ${CMAKE_BINARY_DIR}/coverage)

set(CTOON_COVERAGE_TOTAL_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/total)
set(CTOON_COVERAGE_C_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/c)
set(CTOON_COVERAGE_CPP_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/cpp)
set(CTOON_COVERAGE_PYTHON_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/python)
set(CTOON_COVERAGE_GO_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/go)

file(REMOVE_RECURSE ${CTOON_COVERAGE_BINARY_DIR})
file(MAKE_DIRECTORY ${CTOON_COVERAGE_BINARY_DIR})

add_custom_target(ctoon_test
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running All tests..."
)

add_custom_target(ctoon_coverage
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running coverage for all languages ..."
)


find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(LCOV_EXECUTABLE lcov)
find_program(GENHTML_EXECUTABLE genhtml)

add_subdirectory(${CTOON_TEST_SOURCE_DIR}/c)
add_subdirectory(${CTOON_TEST_SOURCE_DIR}/cpp)
add_subdirectory(${CTOON_TEST_SOURCE_DIR}/python)
add_subdirectory(${CTOON_TEST_SOURCE_DIR}/go)

if(TARGET ctoon_coverage_c AND TARGET ctoon_coverage_cpp AND TARGET ctoon_coverage_python)
    if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)

        # Build the list of --add-tracefile arguments dynamically so Go is
        # included when ctoon_coverage_go is available (and its lcov exists).
        set(LCOV_MERGE_ARGS
            --add-tracefile ${CTOON_COVERAGE_C_BINARY_DIR}/coverage.lcov
            --add-tracefile ${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
            --add-tracefile ${CTOON_COVERAGE_PYTHON_BINARY_DIR}/coverage.lcov
        )
        set(LCOV_MERGE_DEPS ctoon_coverage_c ctoon_coverage_cpp ctoon_coverage_python)
        set(LCOV_MERGE_ECHO "-- Merging C, C++ and Python coverage...")

        if(TARGET ctoon_coverage_go)
            list(APPEND LCOV_MERGE_ARGS
                --add-tracefile ${CTOON_COVERAGE_GO_BINARY_DIR}/coverage.lcov
            )
            list(APPEND LCOV_MERGE_DEPS ctoon_coverage_go)
            set(LCOV_MERGE_ECHO "-- Merging C, C++, Python and Go coverage...")
        endif()

        add_custom_target(ctoon_coverage_total
            # Clean previous coverage data
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CTOON_COVERAGE_TOTAL_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/html
            # Merge all coverage info files
            COMMAND ${CMAKE_COMMAND} -E echo ${LCOV_MERGE_ECHO}
            COMMAND ${LCOV_EXECUTABLE}
                    ${LCOV_MERGE_ARGS}
                    --output-file ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/coverage.lcov
            COMMAND ${CMAKE_COMMAND} -E echo "-- Total coverage info created at: ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/coverage.lcov"

            # Generate HTML report
            COMMAND ${CMAKE_COMMAND}
                -DCOV_FILE=${CTOON_COVERAGE_TOTAL_BINARY_DIR}/coverage.lcov
                -DPREFIX_PATH=${PROJECT_SOURCE_DIR}/
                -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake
            COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report..."
            COMMAND ${GENHTML_EXECUTABLE} --quiet
                    ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/coverage.lcov
                    --output-directory ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/html
                    --prefix ${PROJECT_SOURCE_DIR}
                    --title "CToon Total Coverage"
                    --css-file ${CTOON_TEST_SOURCE_DIR}/genhtml.css
                    --dark-mode
            COMMAND ${CMAKE_COMMAND}
                -DCOV_FILE=${CTOON_COVERAGE_TOTAL_BINARY_DIR}/coverage.lcov
                -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
                -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake
            COMMAND ${CMAKE_COMMAND} -E echo "-- HTML entry point created here: ${CTOON_COVERAGE_TOTAL_BINARY_DIR}/html/index.html"

            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            USES_TERMINAL
        )

        foreach(_dep IN LISTS LCOV_MERGE_DEPS)
            add_dependencies(ctoon_coverage_total ${_dep})
        endforeach()

        add_dependencies(ctoon_coverage ctoon_coverage_total)

        add_custom_command(TARGET ctoon_coverage_total POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DIFRAME_SRC="html/index.html"
                -DSECTION_TITLE="Total%20Coverage"
                -DLOGO_SVG="${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg"
                -DPROJECT_NAME="${PROJECT_NAME}"
                -DOUTPUT_DIR="${CTOON_COVERAGE_TOTAL_BINARY_DIR}"
                -P "${CTOON_TEST_SOURCE_DIR}/GenerateIframe.cmake"
            COMMENT "Generating iframe wrapper for total coverage report"
            WORKING_DIRECTORY ${CTOON_TEST_SOURCE_DIR}
        )
    endif()
endif()

# --- Generate Coverage Dashboard HTML ---
set(COVERAGE_DATA_JSON "[")

if(TARGET ctoon_coverage_total)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"total\",
            \"title\": \"Total Coverage\",
            \"badge\": \"Merged\",
            \"badgeClass\": \"total\",
            \"color\": \"#a371f7\",
            \"desc\": \"Combined coverage report for C, C++, Python and Go codebases. Useful for SonarQube analysis.\",
            \"htmlLink\": \"total/index.html\",
            \"lcovLink\": \"total/coverage.lcov\"
        },")
endif()

if(TARGET ctoon_coverage_c)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"c\",
            \"title\": \"C Core\",
            \"badge\": \"Native\",
            \"color\": \"#2ecc71\",
            \"desc\": \"Detailed coverage report for the core C library, including headers and implementation files generated by LCOV.\",
            \"htmlLink\": \"c/index.html\",
            \"lcovLink\": \"c/coverage.lcov\"
        },")
endif()

if(TARGET ctoon_coverage_cpp)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"cpp\",
            \"title\": \"C++ Core\",
            \"badge\": \"Native\",
            \"color\": \"#f0883e\",
            \"desc\": \"Detailed coverage report for the core C++ library, including headers and implementation files generated by LCOV.\",
            \"htmlLink\": \"cpp/index.html\",
            \"lcovLink\": \"cpp/coverage.lcov\"
        },")
endif()

if(TARGET ctoon_coverage_python)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"py\",
            \"title\": \"Python Bindings\",
            \"badge\": \"Interpreter\",
            \"color\": \"#3572A5\",
            \"desc\": \"Coverage analysis for Python bindings and high-level logic, generated by coverage.py.\",
            \"htmlLink\": \"python/index.html\",
            \"lcovLink\": \"python/coverage.lcov\"
        },")
endif()

if(TARGET ctoon_coverage_go)
    set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}
        {
            \"id\": \"go\",
            \"title\": \"Go Bindings\",
            \"badge\": \"Interpreter\",
            \"color\": \"#00ADD8\",
            \"desc\": \"Coverage analysis for Go bindings generated by go tool cover, converted to lcov for unified reporting.\",
            \"htmlLink\": \"go/html/index.html\",
            \"lcovLink\": \"go/coverage.lcov\"
        },")
endif()

set(COVERAGE_DATA_JSON "${COVERAGE_DATA_JSON}]")

set(CTOON_LOGO_SVG "${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg")
if(EXISTS "${CTOON_LOGO_SVG}")
    file(READ "${CTOON_LOGO_SVG}" LOGO_CONTENT)
else()
    set(LOGO_CONTENT "")
endif()

configure_file(
    ${CTOON_TEST_SOURCE_DIR}/coverage_dashboard.html.in
    ${CTOON_COVERAGE_BINARY_DIR}/index.html
    @ONLY
)