# Standalone consumer of an INSTALLED ctoon (not part of the main build, not
# add_subdirectory()'d): run by .github/workflows/cpp.yml on the zipped
# `cmake --install` tree, or by hand:
#   cmake -S tests/cpp/install_check -B /tmp/c -DCMAKE_PREFIX_PATH=<install-prefix>
cmake_minimum_required(VERSION 3.19)
project(ctoon_install_check_cpp LANGUAGES C CXX)

find_package(ctoon CONFIG REQUIRED)

add_executable(check_cpp check.cpp)
target_link_libraries(check_cpp PRIVATE ctoon::ctoonpp)
set_target_properties(check_cpp PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

enable_testing()
add_test(NAME check_cpp COMMAND check_cpp)
