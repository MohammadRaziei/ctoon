# Standalone consumer of an INSTALLED ctoon (not part of the main build, not
# add_subdirectory()'d): run by .github/workflows/cpp.yml on the zipped
# `cmake --install` tree, or by hand:
#   cmake -S tests/c/install_check -B /tmp/c -DCMAKE_PREFIX_PATH=<install-prefix>
cmake_minimum_required(VERSION 3.19)
project(ctoon_install_check_c LANGUAGES C)

find_package(ctoon CONFIG REQUIRED)

add_executable(check_c check.c)
target_link_libraries(check_c PRIVATE ctoon::ctoon)

enable_testing()
add_test(NAME check_c COMMAND check_c)
