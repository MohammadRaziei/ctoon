find_package(Python COMPONENTS Interpreter Development REQUIRED)

if(DEFINED SKBUILD)

    # Find Python and nanobind
    find_package(nanobind CONFIG REQUIRED)

    # Define the source file for the binding
    set(BINDING_SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/binding.cpp)

    # Create the Python extension module
    # Note: The module name here ('ctoon_py') should match the NB_MODULE name in binding.cpp
    nanobind_add_module(ctoon_py
        ${BINDING_SOURCE_FILE}
    )

    # Link against the ctoon library
    set_property(TARGET ctoonpp PROPERTY CXX_STANDARD 17)
    target_link_libraries(ctoon_py PRIVATE ctoon::ctoonpp)
    set_property(TARGET ctoon_py PROPERTY CXX_STANDARD 17)

    # Generate Python stubs
    add_custom_command(TARGET ctoon_py POST_BUILD
        COMMAND ${Python_EXECUTABLE} -m nanobind.stubgen
                -m ctoon_py
                -o ${CMAKE_CURRENT_BINARY_DIR}/ctoon_py.pyi
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating Python stubs (.pyi) for ctoon_py..."
        BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/ctoon_py.pyi
    )

    # Install rules for the Python package
    # This installs the compiled module (.so or .pyd) and the python source files

    # 1. Install the compiled extension module into the 'ctoon' package directory
    install(TARGETS ctoon_py
        LIBRARY DESTINATION ctoon
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ctoon_py.pyi
        DESTINATION ctoon
    )

    # 2. Install the Python source files (.py) into the package directory
    # We use DIRECTORY to copy the contents of the 'ctoon' folder
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ctoon/
        DESTINATION ctoon
        FILES_MATCHING PATTERN "*.py"
    )

    # Installation
    install(TARGETS ctoon_cli
        RUNTIME DESTINATION ctoon/bin
    )

    return()
endif()


# Include the ExternalProject module
include(ExternalProject)

# --- Check for nanobind dependency ---
# We need to find the nanobind cmake directory using the python module.
execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import nanobind; print(nanobind.cmake_dir())"
    OUTPUT_VARIABLE NANOBIND_CMAKE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE NANOBIND_CHECK_RESULT
)

if(NOT NANOBIND_CHECK_RESULT EQUAL 0)
    message(WARNING "nanobind python module is not installed or not found.")
    message(WARNING "Please install it using: pip install nanobind")
    # We set a dummy variable to prevent further errors if find_package is called conditionally later,
    # but ideally we should stop configuration if it's strictly required.
    # For now, we let it fail naturally in ExternalProject if not found, or handle it there.
endif()

# Setup the external project
# We use the current source directory but build it in a separate binary directory
# to avoid conflicts with the main build.

set(CTOON_PYTHON_INSTALL_DIR ${CMAKE_BINARY_DIR}/python/install)

# Get nanobind cmake dir for the external project
execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import nanobind; print(nanobind.cmake_dir())"
    OUTPUT_VARIABLE NANOBIND_CMAKE_DIR_EXTERNAL
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

ExternalProject_Add(ctoon_build_python
    SOURCE_DIR ${CMAKE_SOURCE_DIR}
    BINARY_DIR ${CMAKE_BINARY_DIR}/python/build
    INSTALL_DIR ${CTOON_PYTHON_INSTALL_DIR}

    # Pass configuration arguments to CMake
    CMAKE_ARGS
        --no-warn-unused-cli
        -DCTOON_BUILD_PYTHON="ON"
        -DSKBUILD=2
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/python/install
        -Dnanobind_DIR=${NANOBIND_CMAKE_DIR_EXTERNAL}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_AR=${CMAKE_AR}
        -DCMAKE_RANLIB=${CMAKE_RANLIB}

    # Ensure the build step runs the install command
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --parallel 8

    # Build this target always, useful for testing workflows
    BUILD_ALWAYS 1
)
add_dependencies(ctoon_build_python ctoon)
