# Find all test source files matching test_*.cpp pattern
file(GLOB CTOON_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp
)

add_executable(test_ctoon_cpp ${CTOON_TEST_SOURCES})

# Default: test against normal headers (source tree)
set(CTOON_TEST_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/includes")

target_include_directories(test_ctoon_cpp PRIVATE "${CTOON_TEST_INCLUDE_DIR}")

# Link against ctoon library
target_link_libraries(test_ctoon_cpp PRIVATE ctoon)

add_test(NAME test_ctoon_cpp COMMAND $<TARGET_FILE:test_ctoon_cpp>)

add_custom_target(ctoon_test_cpp
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running c++ tests..."
    COMMAND $<TARGET_FILE:test_ctoon_cpp>
    WORKING_DIRECTORY ${CTOON_TEST_BINARY_DIR}
)
add_dependencies(ctoon_test_cpp test_ctoon_cpp)
add_dependencies(ctoon_test ctoon_test_cpp)


if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
    # Enable coverage flags for the test executable
    target_compile_options(test_ctoon_cpp PRIVATE -O0 -g --coverage)
    target_link_options(test_ctoon_cpp PRIVATE --coverage)

    add_custom_target(ctoon_coverage_cpp
        # Clean previous coverage data (only html, keep index.html wrapper)
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CTOON_COVERAGE_CPP_BINARY_DIR}/html
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CTOON_COVERAGE_CPP_BINARY_DIR}/html

        # Capture coverage data
        COMMAND ${CMAKE_COMMAND} -E echo "-- Capturing coverage..."
        COMMAND ${LCOV_EXECUTABLE}
                --capture
                --directory ${CTOON_TEST_BINARY_DIR}
                --base-directory ${PROJECT_SOURCE_DIR}
                --no-external
                --rc function_coverage=1
                --output-file ${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage.lcov

        # Filter for ctoon files only
        COMMAND ${CMAKE_COMMAND} -E echo "Filtering ctoon files..."
        COMMAND ${LCOV_EXECUTABLE}
                --extract
                ${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
                "*/ctoon/*"
                --output-file ${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov
        COMMAND ${CMAKE_COMMAND} -E remove ${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
        COMMAND ${LCOV_EXECUTABLE}
            --remove
            ${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov
            "*/tests/*"
            --output-file ${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
        COMMAND ${CMAKE_COMMAND} -E remove ${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov

        # Generate HTML report
        COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report..."
        COMMAND ${GENHTML_EXECUTABLE} --quiet
                ${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
                --output-directory ${CTOON_COVERAGE_CPP_BINARY_DIR}/html
                --prefix ${PROJECT_SOURCE_DIR}
                --title "CToon C++ Coverage"
                --css-file ${CTOON_TEST_SOURCE_DIR}/genhtml.css
                --dark-mode
        COMMAND ${CMAKE_COMMAND} -E echo "-- HTML entry point created here: ${CTOON_COVERAGE_CPP_BINARY_DIR}/html/index.html"

        # Fix paths to be relative (for SonarQube, etc.)
        COMMAND ${CMAKE_COMMAND} -E echo "-- Fixing paths to relative..."
        COMMAND ${CMAKE_COMMAND}
                -DCOV_FILE=${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
                -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
                -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake
        # Summary
        COMMAND ${CMAKE_COMMAND} -E echo "-- Coverage summary:"
        COMMAND ${LCOV_EXECUTABLE}
                --list
                ${CTOON_COVERAGE_CPP_BINARY_DIR}/coverage.lcov

        WORKING_DIRECTORY ${CTOON_TEST_BINARY_DIR}
        USES_TERMINAL
    )

    add_dependencies(ctoon_coverage_cpp ctoon_test_cpp)
    add_dependencies(ctoon_coverage ctoon_coverage_cpp)

    # Generate iframe wrapper
    set(CTOON_IFRAME_SRC "html/index.html")
    set(CTOON_SECTION_TITLE "C++ Core")
    file(MAKE_DIRECTORY ${CTOON_COVERAGE_CPP_BINARY_DIR})
    configure_file(
        ${CTOON_TEST_SOURCE_DIR}/coverage_iframe.html.in
        ${CTOON_COVERAGE_CPP_BINARY_DIR}/index.html
        @ONLY
    )
else()
    message(WARNING "LCOV or GENHTML executables not found.")
    message(WARNING "C++ coverage targets will be skipped.")
    message(WARNING "Please install them to enable coverage reports (e.g., 'sudo apt install lcov' on Linux).")
endif()
