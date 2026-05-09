# CToon Documentation Build
# Builds C++ docs (Doxygen) and Python docs (Sphinx) via CreateDocs.cmake scripts

cmake_minimum_required(VERSION 3.19)

set(CTOON_DOCS_OUT ${CMAKE_CURRENT_BINARY_DIR}/out)

set(CTOON_DOCS_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
set(CTOON_DOCS_CPP_OUT ${CTOON_DOCS_OUT}/cpp)

set(CTOON_DOCS_PYTHON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/python)
set(CTOON_DOCS_PYTHON_OUT ${CTOON_DOCS_OUT}/python)


# ── C++ Documentation (Doxygen) ───────────────────────────────

find_package(Doxygen)

if(DOXYGEN_FOUND)
    set(CTOON_DOCS_CPP_HTML ${CTOON_DOCS_CPP_OUT}/html/index.html)

    add_custom_command(
        OUTPUT ${CTOON_DOCS_CPP_HTML}
        COMMAND ${CMAKE_COMMAND}
            -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            -DPROJECT_VERSION=${PROJECT_VERSION}
            -DOUTPUT_DIR=${CTOON_DOCS_CPP_OUT}
            -DDOXYGEN_EXECUTABLE=${DOXYGEN_EXECUTABLE}
            -DFOOTER_IN=${CTOON_DOCS_CPP_DIR}/footer.html.in
            -DDOXYFILE_IN=${CTOON_DOCS_CPP_DIR}/Doxyfile.in
            -DLOGO_SRC=${PROJECT_SOURCE_DIR}/docs/images/ctoon-theme.svg
            -P ${CTOON_DOCS_CPP_DIR}/CreateDocs.cmake
        DEPENDS
            ${CTOON_DOCS_CPP_DIR}/CreateDocs.cmake
            ${CTOON_DOCS_CPP_DIR}/footer.html.in
            ${CTOON_DOCS_CPP_DIR}/Doxyfile.in
        COMMENT "Generating C++ API documentation with Doxygen"
        VERBATIM
    )

    add_custom_target(ctoon_docs_cpp DEPENDS ${CTOON_DOCS_CPP_HTML})
else()
    message(WARNING "Doxygen not found - C++ documentation will not be generated")
    add_custom_target(ctoon_docs_cpp
        COMMAND ${CMAKE_COMMAND} -E echo "Skipping C++ docs: Doxygen not found"
    )
endif()


# ── Python Documentation (Sphinx) ─────────────────────────────

find_package(Python3 COMPONENTS Interpreter REQUIRED)

execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import sphinx"
    OUTPUT_QUIET
    ERROR_QUIET
    RESULT_VARIABLE SPHINX_CHECK
)

if(SPHINX_CHECK EQUAL 0)
    set(CTOON_DOCS_PYTHON_HTML ${CTOON_DOCS_PYTHON_OUT}/html/index.html)

    add_custom_command(
        OUTPUT ${CTOON_DOCS_PYTHON_HTML}
        COMMAND ${CMAKE_COMMAND}
            -DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}
            -DSOURCE_DIR=${CTOON_DOCS_PYTHON_DIR}/source
            -DOUTPUT_DIR=${CTOON_DOCS_PYTHON_OUT}
            -DCTOON_PYTHON_INSTALL_DIR=${CMAKE_BINARY_DIR}/python/install
            -P ${CTOON_DOCS_PYTHON_DIR}/CreateDocs.cmake
        DEPENDS
            ${CTOON_DOCS_PYTHON_DIR}/CreateDocs.cmake
            ${CTOON_DOCS_PYTHON_DIR}/source/conf.py
            ${CTOON_DOCS_PYTHON_DIR}/source/index.rst
        COMMENT "Building Python API documentation with Sphinx"
        VERBATIM
    )

    add_custom_target(ctoon_docs_python DEPENDS ${CTOON_DOCS_PYTHON_HTML})

    # Must have ctoon_build_python run first so the module is installed
    if(TARGET ctoon_build_python)
        add_dependencies(ctoon_docs_python ctoon_build_python)
    endif()
else()
    message(WARNING "Sphinx not found - Python documentation will not be generated")
    add_custom_target(ctoon_docs_python
        COMMAND ${CMAKE_COMMAND} -E echo "Skipping Python docs: Sphinx not found"
    )
endif()


# ── Route Index Page ──────────────────────────────────────────

set(CTOON_INDEX_OUT ${CTOON_DOCS_OUT}/index.html)

add_custom_command(
    OUTPUT ${CTOON_INDEX_OUT}
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_DIR=${CTOON_DOCS_OUT}
        -DINDEX_IN=${CMAKE_CURRENT_SOURCE_DIR}/index.html.in
        -DCSS_FILE=${CMAKE_CURRENT_SOURCE_DIR}/ctoon-docs.css
        -DPROJECT_VERSION=${PROJECT_VERSION}
        -DLOGO_SQ=${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CreateDocs.cmake
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/CreateDocs.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/index.html.in
        ${CMAKE_CURRENT_SOURCE_DIR}/ctoon-docs.css
        ${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg
    COMMENT "Creating documentation route index"
    VERBATIM
)

add_custom_target(ctoon_docs_index DEPENDS ${CTOON_INDEX_OUT})


# ── Markdown pages (CToonPages) ───────────────────────────────

set(CTOON_DOCS_SRC ${CMAKE_CURRENT_SOURCE_DIR})

set(LOGO_SQ ${PROJECT_SOURCE_DIR}/docs/images/ctoon-sq.svg)

include(${CMAKE_CURRENT_SOURCE_DIR}/CToonPages.cmake)

# ctoon_add_page(NAME cli        SOURCE cli.md        TITLE "CLI Reference")
# ctoon_add_page(NAME benchmark  SOURCE benchmark.md  TITLE "Benchmarks")
# ctoon_add_page(NAME changelog  SOURCE changelog.md  TITLE "Changelog")

ctoon_add_section(
    NAME  cli
    TITLE "CLI Reference"
    SOURCES
        "cli/index.md:Overview"
        "cli/installation.md:Installation"
        "cli/options.md:Options"
        "cli/examples.md:Examples"
)



# ── Master target ─────────────────────────────────────────────

add_custom_target(ctoon_docs
    COMMAND ${CMAKE_COMMAND} -E echo "-- All documentation generated"
    COMMENT "Building all documentation"
)

add_dependencies(ctoon_docs ctoon_docs_cpp ctoon_docs_python ctoon_docs_index)
ctoon_finalize_pages(ctoon_docs)


# ── Post-process: inject HTML comment into all output HTML files ──

add_custom_command(TARGET ctoon_docs POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_DIR=${CTOON_DOCS_OUT}
        -DCOMMENT_IN=${CMAKE_CURRENT_SOURCE_DIR}/initial_comment.in
        -DASCII_ART_FILE=${CMAKE_CURRENT_SOURCE_DIR}/images/ctoon-sq.txt
        -DPROJECT_VERSION=${PROJECT_VERSION}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/InjectComment.cmake
    COMMENT "Injecting HTML comments into documentation pages"
    VERBATIM
)