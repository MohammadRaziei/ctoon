# CToon Documentation Build
# Builds C++ docs (Doxygen) and Python docs (Sphinx) separately,
# then creates a route index.html that links to both.

cmake_minimum_required(VERSION 3.19)

set(CTOON_DOCS_OUT ${CMAKE_CURRENT_BINARY_DIR})

set(CTOON_DOCS_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
set(CTOON_DOCS_CPP_OUT ${CTOON_DOCS_OUT}/cpp)

set(CTOON_DOCS_PYTHON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/python)
set(CTOON_DOCS_PYTHON_OUT ${CTOON_DOCS_OUT}/python)

file(REMOVE_RECURSE ${CTOON_DOCS_OUT})
file(MAKE_DIRECTORY ${CTOON_DOCS_OUT})

# ── C++ Documentation (Doxygen) ─────────────────────────────────

find_package(Doxygen)

if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE}")

    file(REMOVE_RECURSE ${CTOON_DOCS_OUT})


    include(FetchContent)
    FetchContent_Declare(
        doxygen-awesome-css
        URL https://github.com/jothepro/doxygen-awesome-css/archive/main.zip
    )
    FetchContent_MakeAvailable(doxygen-awesome-css)
    FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)

    set(DOXYGEN_FOOTER_IN ${CTOON_DOCS_CPP_DIR}/footer.html.in)
    set(DOXYGEN_FOOTER_OUT ${CTOON_DOCS_CPP_OUT}/footer.html)
    configure_file(${DOXYGEN_FOOTER_IN} ${DOXYGEN_FOOTER_OUT} @ONLY)

    set(DOXYGEN_IN ${CTOON_DOCS_CPP_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CTOON_DOCS_CPP_OUT}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(ctoon_docs_cpp
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CTOON_DOCS_CPP_OUT}
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/docs/images/ctoon-theme.svg ${CTOON_DOCS_CPP_OUT}/html/ctoon.svg
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        COMMAND ${CMAKE_COMMAND} -E echo "-- C++ docs generated at: ${CTOON_DOCS_CPP_OUT}/html/index.html"
        WORKING_DIRECTORY ${CTOON_DOCS_CPP_OUT}
        COMMENT "Generating C++ API documentation with Doxygen"
        VERBATIM
        DEPENDS ${DOXYGEN_OUT} ${DOXYGEN_FOOTER_OUT}
    )
else()
    message(WARNING "Doxygen not found - C++ documentation will not be generated")
endif()


# ── Python Documentation (Sphinx) ───────────────────────────────

find_package(Python3 COMPONENTS Interpreter REQUIRED)

execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import sphinx"
    OUTPUT_QUIET
    ERROR_QUIET
    RESULT_VARIABLE SPHINX_CHECK
)

if(SPHINX_CHECK EQUAL 0)
    add_custom_target(ctoon_docs_python
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CTOON_DOCS_PYTHON_OUT}/html
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CTOON_DOCS_PYTHON_OUT}/doctrees
        COMMAND ${Python3_EXECUTABLE} -m sphinx
            -b html
            -d ${CTOON_DOCS_PYTHON_OUT}/doctrees
            ${CTOON_DOCS_PYTHON_DIR}/source
            ${CTOON_DOCS_PYTHON_OUT}/html
        COMMAND ${CMAKE_COMMAND} -E echo "-- Python docs generated at: ${CTOON_DOCS_PYTHON_OUT}/html/index.html"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Building Python API documentation with Sphinx"
        VERBATIM
    )
else()
    message(WARNING "Sphinx not found - Python documentation will not be generated")
    message(WARNING "Install with: pip install sphinx furo")
endif()


# ── Route Index Page ────────────────────────────────────────────
set(CTOON_INDEX_IN  ${CMAKE_CURRENT_SOURCE_DIR}/index.html.in)
set(CTOON_INDEX_OUT ${CMAKE_CURRENT_BINARY_DIR}/index.html)

# Copy logo to build root for index.html
add_custom_command(
    OUTPUT ${CTOON_INDEX_OUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/docs/images/ctoon-theme.svg ${CMAKE_CURRENT_BINARY_DIR}/ctoon.svg
    COMMAND sed "s/@PROJECT_VERSION@/${PROJECT_VERSION}/g" ${CTOON_INDEX_IN} > ${CTOON_INDEX_OUT}
    COMMENT "Creating documentation route index"
)

add_custom_target(ctoon_docs_index
    DEPENDS ${CTOON_INDEX_OUT}
)


# ── Master target ──────────────────────────────────────────────
add_custom_target(ctoon_docs
    COMMAND ${CMAKE_COMMAND} -E echo "-- All documentation generated"
    COMMENT "Building all documentation"
)

if(DOXYGEN_FOUND)
    add_dependencies(ctoon_docs ctoon_docs_cpp ctoon_docs_index)
endif()

if(SPHINX_CHECK EQUAL 0)
    add_dependencies(ctoon_docs ctoon_docs_python)
endif()
