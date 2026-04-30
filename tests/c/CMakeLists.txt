# Find all test source files matching test_*.c pattern
file(GLOB CTOON_TEST_SOURCES_C CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.c
)

add_executable(test_ctoon_c ${CTOON_TEST_SOURCES_C} ${CTOON_TEST_SOURCE_DIR}/utest/utest_main.cxx)

target_include_directories(test_ctoon_c PRIVATE 
    "${CTOON_TEST_SOURCE_DIR}"
)

# Link against ctoon library
target_link_libraries(test_ctoon_c PRIVATE ctoon::ctoon)

add_test(NAME test_ctoon_c COMMAND $<TARGET_FILE:test_ctoon_c>)

add_custom_target(ctoon_test_c
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running C tests..."
    COMMAND $<TARGET_FILE:test_ctoon_c>
    WORKING_DIRECTORY ${CTOON_TEST_BINARY_DIR}
)
add_dependencies(ctoon_test_c test_ctoon_c)
add_dependencies(ctoon_test ctoon_test_c)


if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
    # Enable coverage flags for the test executable
    target_compile_options(test_ctoon_c PRIVATE -O0 -g --coverage)
    target_link_options(test_ctoon_c PRIVATE --coverage)
    
    # Also add coverage to ctoon library for this test
    set_target_properties(test_ctoon_c PROPERTIES
        COMPILE_OPTIONS "--coverage"
        LINK_OPTIONS "--coverage"
    )

    # Create coverage binary directory for C tests
    set(CTOON_COVERAGE_C_BINARY_DIR ${CTOON_COVERAGE_BINARY_DIR}/c)
    
    add_custom_target(ctoon_coverage_c
        # Clean previous coverage data (only html, keep index.html wrapper)
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CTOON_COVERAGE_C_BINARY_DIR}/html
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CTOON_COVERAGE_C_BINARY_DIR}/html

        # Capture coverage data from ctoon library only (not test files)
        COMMAND ${CMAKE_COMMAND} -E echo "-- Capturing C coverage..."
        # First capture all coverage, excluding test files to avoid mismatch errors
        COMMAND ${LCOV_EXECUTABLE}
                --capture
                --base-directory ${PROJECT_SOURCE_DIR}
                --directory ${CMAKE_CURRENT_BINARY_DIR}
                --exclude "*/tests/*"
                --no-external
                --rc function_coverage=1
                --output-file ${CTOON_COVERAGE_C_BINARY_DIR}/coverage_temp.lcov
        # Extract only ctoon source files
        COMMAND ${CMAKE_COMMAND} -E echo "Filtering ctoon source files..."
        COMMAND ${LCOV_EXECUTABLE}
                --extract
                ${CTOON_COVERAGE_C_BINARY_DIR}/coverage_temp.lcov
                "*/ctoon.*"
                --output-file ${CTOON_COVERAGE_C_BINARY_DIR}/coverage.lcov
        COMMAND ${CMAKE_COMMAND} -E remove ${CTOON_COVERAGE_C_BINARY_DIR}/coverage_temp.lcov

        # Generate HTML report
        COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report..."
        COMMAND ${GENHTML_EXECUTABLE} --quiet
                ${CTOON_COVERAGE_C_BINARY_DIR}/coverage.lcov
                --output-directory ${CTOON_COVERAGE_C_BINARY_DIR}/html
                --prefix ${PROJECT_SOURCE_DIR}
                --title "CToon C Coverage"
                --css-file ${CTOON_TEST_SOURCE_DIR}/genhtml.css
                --dark-mode
        COMMAND ${CMAKE_COMMAND} -E echo "-- HTML entry point created here: ${CTOON_COVERAGE_C_BINARY_DIR}/html/index.html"

        # Fix paths to be relative (for SonarQube, etc.)
        COMMAND ${CMAKE_COMMAND} -E echo "-- Fixing paths to relative..."
        COMMAND ${CMAKE_COMMAND}
                -DCOV_FILE=${CTOON_COVERAGE_C_BINARY_DIR}/coverage.lcov
                -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
                -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake
        # Summary
        COMMAND ${CMAKE_COMMAND} -E echo "-- Coverage summary:"
        COMMAND ${LCOV_EXECUTABLE}
                --list
                ${CTOON_COVERAGE_C_BINARY_DIR}/coverage.lcov

        WORKING_DIRECTORY ${CTOON_TEST_BINARY_DIR}
        USES_TERMINAL
    )

    add_dependencies(ctoon_coverage_c ctoon_test_c)
    add_dependencies(ctoon_coverage ctoon_coverage_c)

    # Generate iframe wrapper using GenerateIframe.cmake
    file(MAKE_DIRECTORY ${CTOON_COVERAGE_C_BINARY_DIR})
    
    # Add custom command to run GenerateIframe.cmake after coverage target
    add_custom_command(TARGET ctoon_coverage_c POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DIFRAME_SRC="html/index.html"
                -DSECTION_TITLE="C%20Core"
                -DLOGO_SVG="${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg"
                -DPROJECT_NAME="${PROJECT_NAME}"
                -DOUTPUT_DIR="${CTOON_COVERAGE_C_BINARY_DIR}"
                -P "${PROJECT_SOURCE_DIR}/cmake/GenerateIframe.cmake"
        COMMENT "Generating iframe wrapper for C coverage report"
        WORKING_DIRECTORY ${CTOON_TEST_SOURCE_DIR}
    )
else()
    message(WARNING "LCOV or GENHTML executables not found.")
    message(WARNING "C coverage targets will be skipped.")
    message(WARNING "Please install them to enable coverage reports (e.g., 'sudo apt install lcov' on Linux).")
endif()