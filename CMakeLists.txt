cmake_minimum_required(VERSION 3.15)

option(BUILD_PYTHON "export python module" off)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ version selection")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(PyProject)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()


set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)



project(${PyProject_NAME} LANGUAGES CXX C VERSION ${PyProject_VERSION})



file(GLOB CTOON_SOURCES_C ${PROJECT_SOURCE_DIR}/src/sources/*.c)
file(GLOB CTOON_SOURCES_CPP ${PROJECT_SOURCE_DIR}/src/sources/*.cpp)
set(CTOON_SOURCES ${CTOON_SOURCES_CPP} ${CTOON_SOURCES_C})

# Create library
add_library(ctoon ${CTOON_SOURCES})

# Include directories
target_include_directories(ctoon
    PUBLIC ${PROJECT_SOURCE_DIR}/includes
    PRIVATE ${PROJECT_SOURCE_DIR}/src/sources
)

target_compile_definitions(ctoon PUBLIC
    CTOON_VERSION=${PROJECT_VERSION}
)

# Compiler options
target_compile_options(ctoon PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)


# Build examples
option(BUILD_EXAMPLES "Build example programs" ON)


include(SetupPython)

if (BUILD_EXAMPLES)
    message(STATUS "Building examples")

    # Build all examples using glob pattern
    file(GLOB EXAMPLE_SOURCES ${PROJECT_SOURCE_DIR}/examples/*.cpp)
    foreach(EXAMPLE_SOURCE IN LISTS EXAMPLE_SOURCES)
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
        add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
        target_link_libraries(${EXAMPLE_NAME} PUBLIC ctoon)
    endforeach()
endif()

# Build CLI tool
add_executable(ctoon-cli src/sources/cli.cpp)
target_link_libraries(ctoon-cli PUBLIC ctoon)
set_target_properties(ctoon-cli PROPERTIES OUTPUT_NAME ctoon)

option(CTOON_BUILD_TESTS "Build C++ tests" ON)

if(CTOON_BUILD_TESTS)
    enable_testing()
    add_executable(ctoon_tests tests/cpp/test_ctoon.cpp)
    target_include_directories(ctoon_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/cpp)
    target_link_libraries(ctoon_tests PRIVATE ctoon)
    add_test(NAME ctoon_tests COMMAND ctoon_tests)
    set_tests_properties(ctoon_tests PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
