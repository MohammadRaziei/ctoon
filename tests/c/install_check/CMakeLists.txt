# Standalone consumer of an INSTALLED ctoon (not part of the main build, not
# add_subdirectory()'d): run by .github/workflows/cpp.yml on the zipped
# `cmake --install` tree, or by hand:
#   cmake -S tests/c/install_check -B /tmp/c -DCMAKE_PREFIX_PATH=<install-prefix>
cmake_minimum_required(VERSION 3.19)
project(ctoon_install_check_c LANGUAGES C)

find_package(ctoon CONFIG REQUIRED)

# CI passes -DCTOON_EXPECT_C_ONLY=ON for the C package: it must not ship the C++ header
option(CTOON_EXPECT_C_ONLY "fail if ctoon::ctoonpp exists in the installed package" OFF)
if(CTOON_EXPECT_C_ONLY AND TARGET ctoon::ctoonpp)
    message(FATAL_ERROR "ctoon::ctoonpp must not exist in the C package")
endif()

add_executable(check_c check.c)
target_link_libraries(check_c PRIVATE ctoon::ctoon)

enable_testing()
add_test(NAME check_c COMMAND check_c)
