cmake_minimum_required(VERSION 3.19)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

project(CToon LANGUAGES CXX C VERSION 0.1.0)
message(STATUS "Project: ${PROJECT_NAME}@v${PROJECT_VERSION}")
include(Startup)

# ---- Language standards -------------------------------------------------
set(CMAKE_C_STANDARD   11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD   17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(Optimize)

# ---- Options ------------------------------------------------------------
option(CTOON_BUILD_EXAMPLES "Build examples"       ${PROJECT_IS_TOP_LEVEL})
option(CTOON_BUILD_TESTS    "Build tests"           ${PROJECT_IS_TOP_LEVEL})
option(CTOON_BUILD_PYTHON   "Build Python bindings" OFF)
option(CTOON_BUILD_DOCS     "Build documentation"  OFF)

set(CTOON_SOURCE_DIR ${PROJECT_SOURCE_DIR})

# ---- Core C library -----------------------------------------------------
file(GLOB CTOON_SOURCES_C   CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/sources/*.c)
file(GLOB CTOON_SOURCES_CPP CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/sources/*.cpp)

add_library(ctoon ${CTOON_SOURCES_C} ${CTOON_SOURCES_CPP})
add_library(ctoon::ctoon ALIAS ctoon)

set_target_properties(ctoon PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(ctoon
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${PROJECT_SOURCE_DIR}/src/sources
)

target_compile_options(ctoon PRIVATE
    $<$<COMPILE_LANGUAGE:C>:  -Wall -Wextra -Wpedantic>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc>
)

apply_optimization_flags(ctoon)

# ---- C++ header-only binding wrapper ------------------------------------
add_library(ctoonpp INTERFACE)
add_library(ctoon::ctoonpp ALIAS ctoonpp)

target_include_directories(ctoonpp INTERFACE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/bindings
)

target_link_libraries(ctoonpp INTERFACE ctoon::ctoon)

# ---- CLI ----------------------------------------------------------------
add_executable(ctoon_cli ${PROJECT_SOURCE_DIR}/src/cli/cli.cpp
                         ${PROJECT_SOURCE_DIR}/src/cli/json/yyjson.c)

target_include_directories(ctoon_cli PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/bindings
    ${PROJECT_SOURCE_DIR}/src/cli
    ${PROJECT_SOURCE_DIR}/src/cli/json
)

target_link_libraries(ctoon_cli PRIVATE ctoon::ctoonpp)

set_target_properties(ctoon_cli PROPERTIES OUTPUT_NAME ctoon)

target_compile_options(ctoon_cli PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic>
    $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra -Wpedantic>
)

apply_optimization_flags(ctoon_cli)

# ---- Python (optional) --------------------------------------------------
if(CTOON_BUILD_PYTHON OR DEFINED SKBUILD)
    add_subdirectory(${PROJECT_SOURCE_DIR}/src/bindings/python)
    if(DEFINED SKBUILD)
        return()
    endif()
endif()

# ---- Installation -------------------------------------------------------
install(TARGETS ctoon_cli RUNTIME DESTINATION bin)
install(DIRECTORY include DESTINATION .)

# ---- Tests --------------------------------------------------------------
if(CTOON_BUILD_TESTS)
    enable_testing()
    set(CMAKE_CTEST_ARGUMENTS "--output-on-failure" "-V")
    add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
endif()

# ---- Docs ---------------------------------------------------------------
if(CTOON_BUILD_DOCS)
    add_subdirectory(${PROJECT_SOURCE_DIR}/docs)
endif()

print_optimization_summary()
