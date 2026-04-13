# --- Testing and Coverage Section ---
set(CTOON_PYTHON_INSTALL_DIR ${CMAKE_BINARY_DIR}/python/install)

set(USE_GENHTML_FOR_PYTHON OFF)

if(TARGET ctoon_build_python)
    # Check for pytest
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -m pytest --version
        OUTPUT_QUIET
        ERROR_QUIET
        RESULT_VARIABLE PYTEST_CHECK_RESULT
    )

    if(NOT PYTEST_CHECK_RESULT EQUAL 0)
        message(WARNING "pytest is not installed. Python tests will be skipped.")
        message(WARNING "Please install it using: pip install pytest")
    else()
        # Define the test executable target (conceptually)
        add_test(NAME ctoon_python_tests
                 COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}
                 WORKING_DIRECTORY ${CTOON_PYTHON_INSTALL_DIR}
        )

        # In reality, we run pytest via the Python interpreter found above.
        add_custom_target(ctoon_test_python
            COMMAND ${CMAKE_COMMAND} -E echo "-- Running python tests..."
            COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}
            WORKING_DIRECTORY ${CTOON_PYTHON_INSTALL_DIR}
            USES_TERMINAL
        )

        # Ensure tests are run after the custom build is complete
        add_dependencies(ctoon_test_python ctoon_build_python)
        add_dependencies(ctoon_test ctoon_test_python)
    endif()

    # Check for pytest-cov (coverage tool)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -m coverage --version
        OUTPUT_QUIET
        ERROR_QUIET
        RESULT_VARIABLE COVERAGE_CHECK_RESULT
    )

    if(COVERAGE_CHECK_RESULT EQUAL 0)
        # Enable coverage for the python test target
        # We define a custom target to generate the coverage report.
        add_custom_target(ctoon_coverage_python
            # Clean previous coverage data
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CTOON_COVERAGE_PYTHON_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CTOON_COVERAGE_PYTHON_BINARY_DIR}/html

            # Run tests with coverage enabled
            # We run from the install directory so Python can find the installed package.
            COMMAND ${CMAKE_COMMAND} -E echo "-- Capturing coverage..."
            COMMAND ${Python3_EXECUTABLE} -m coverage run
                    --source=${CTOON_PYTHON_INSTALL_DIR}
                    --omit="*/tests/*"
                    -m pytest ${CMAKE_CURRENT_SOURCE_DIR}

            # Generate lcov file
            COMMAND ${CMAKE_COMMAND} -E echo "-- Generating coverage.lcov file..."
            COMMAND ${Python3_EXECUTABLE} -m coverage lcov -o ${CTOON_COVERAGE_PYTHON_BINARY_DIR}/coverage.lcov

            COMMAND ${CMAKE_COMMAND}
                    -DCOV_FILE=${CTOON_COVERAGE_PYTHON_BINARY_DIR}/coverage.lcov
                    -DPREFIX_PATH=src/bindings/python/
                    -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake

            # Generate HTML report
            COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report..."
            COMMAND ${Python3_EXECUTABLE} -m coverage html -d ${CTOON_COVERAGE_PYTHON_BINARY_DIR}/html

            # Summary
            COMMAND ${CMAKE_COMMAND} -E echo "-- HTML entry point created here: ${CTOON_COVERAGE_PYTHON_BINARY_DIR}/html/index.html"
            COMMAND ${CMAKE_COMMAND} -E echo "-- Coverage summary:"
            COMMAND ${Python3_EXECUTABLE} -m coverage report

            # Changed working directory to the install directory
            WORKING_DIRECTORY ${CTOON_PYTHON_INSTALL_DIR}
            USES_TERMINAL
        )
        if(GENHTML_EXECUTABLE AND USE_GENHTML_FOR_PYTHON)
            add_custom_command(TARGET ctoon_coverage_python POST_BUILD
                # Generate HTML report
                COMMAND ${CMAKE_COMMAND} -E echo "-- Generating HTML report with GENHTML..."
                COMMAND ${CMAKE_COMMAND} -E remove_directory ${CTOON_COVERAGE_PYTHON_BINARY_DIR}/html
                COMMAND ${GENHTML_EXECUTABLE} --quiet
                        ${CTOON_COVERAGE_PYTHON_BINARY_DIR}/coverage.lcov
                        --output-directory ${CTOON_COVERAGE_PYTHON_BINARY_DIR}/html
                        --title "CToon Python Coverage"
                        --css-file ${CTOON_TEST_SOURCE_DIR}/genhtml.css
                        --dark-mode
                COMMAND ${CMAKE_COMMAND} -E echo "-- HTML entry point created here: ${CTOON_COVERAGE_PYTHON_BINARY_DIR}/html/index.html"
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            )
        endif()
        # Ensure coverage runs after the build is complete
        add_dependencies(ctoon_coverage_python ctoon_build_python)

        # Add this python coverage target to the main 'ctoon_coverage' target
        add_dependencies(ctoon_coverage ctoon_coverage_python)
    else()
        message(WARNING "coverage module is not installed. Python coverage target will be skipped.")
        message(WARNING "Please install it using: pip install coverage")
    endif()
endif()
