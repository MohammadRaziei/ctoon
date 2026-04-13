# Find Doxygen
find_package(Doxygen)

if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE}")
    cmake_policy(SET CMP0135 NEW)

    include(FetchContent)
    FetchContent_Declare(
        doxygen-awesome-css
        URL https://github.com/jothepro/doxygen-awesome-css/archive/main.zip
        # DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        # URL_HASH SHA256=286f8008c08bb80f88910b3333af763a20724e90c2fd70dd9ee24071e1856965
    )
    FetchContent_MakeAvailable(doxygen-awesome-css)

    # Save the location the files were cloned into
    # This allows us to get the path to doxygen-awesome.css
    FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)

    set(DOXYGEN_FOOTER_IN ${CMAKE_CURRENT_SOURCE_DIR}/footer.html.in)
    set(DOXYGEN_FOOTER_OUT ${CMAKE_CURRENT_BINARY_DIR}/footer.html)
    configure_file(${DOXYGEN_FOOTER_IN} ${DOXYGEN_FOOTER_OUT} @ONLY)

    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(ctoon_docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        COMMAND ${CMAKE_COMMAND} -E echo "-- Documentation generated at: ${CMAKE_CURRENT_BINARY_DIR}/docs/html/index.html"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )

    # # Print documentation location after build
    # add_custom_command(TARGET docs POST_BUILD
    #     VERBATIM
    # )
else()
    message(WARNING "Doxygen not found - documentation will not be generated")
endif()
