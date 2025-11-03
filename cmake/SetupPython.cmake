find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(DEFINED SKBUILD)
    set(PYTHON_PROJECT_NAME "${SKBUILD_PROJECT_NAME}")
elseif(BUILD_PYTHON)
    set(PYTHON_PROJECT_NAME "${CMAKE_BINARY_DIR}")

    if(NOT PYTHON_REQUIREMENT_INSTALLED)
        execute_process(
                COMMAND "${Python3_EXECUTABLE}" -m pip install
                nanobind ninja pytest-xdist pip-tools # build requirements
                OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY
        )
        execute_process(
                COMMAND "${Python3_EXECUTABLE}" -m piptools compile --output-file ${CMAKE_CURRENT_BINARY_DIR}/requirements.txt pyproject.toml
                    --no-emit-options --quiet --no-strip-extras --extra test
                OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY
        )
        execute_process(
                COMMAND "${Python3_EXECUTABLE}" -m pip install -r ${CMAKE_CURRENT_BINARY_DIR}/requirements.txt
                OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY
        )
        set(PYTHON_REQUIREMENT_INSTALLED TRUE CACHE INTERNAL "Python requirements installed")
    endif()

    execute_process(
            COMMAND "${Python3_EXECUTABLE}" -m nanobind --cmake_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE NB_DIR)

    message(STATUS "Found NanoBind at ${NB_DIR}")
    list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
endif()


if(DEFINED PYTHON_PROJECT_NAME)
    # Try to import all Python components potentially needed by nanobind
    #set(Python3_FIND_STRATEGY LOCATION)
    find_package(Python 3.8
            REQUIRED COMPONENTS Interpreter Development.Module
            OPTIONAL_COMPONENTS Development.SABIModule)
    # Import nanobind through CMake's find_package mechanism
    find_package(nanobind CONFIG REQUIRED)



    set(NB_MODULE _${PROJECT_NAME})

    nanobind_add_module(
            # Name of the extension
            ${NB_MODULE}

            # Target the stable ABI for Python 3.12+, which reduces
            # the number of binary wheels that must be built. This
            # does nothing on older Python versions
            STABLE_ABI

            # Build libnanobind statically and merge it into the
            # extension (which itself remains a shared library)
            #
            # If your project builds multiple extensions, you can
            # replace this flag by NB_SHARED to conserve space by
            # reusing a shared libnanobind across libraries
            NB_STATIC

            # Source code goes here
            ${SERIN_SOURCES} ${PROJECT_SOURCE_DIR}/src/binding/main.cpp
    )

    target_compile_definitions(${NB_MODULE} PRIVATE
            VERSION_INFO=${PROJECT_VERSION}
            NB_MODULE_NAME=${NB_MODULE})

endif()

if(DEFINED SKBUILD)
    RETURN()
endif()
