# Auto-installs its own dependencies (requirements.txt + the local ctoon
# checkout) every time it's invoked, rather than warning and skipping —
# Python3 is already confirmed present (see the parent CMakeLists.txt), so
# there's nothing to skip here, only packages to install.
add_custom_target(ctoon_benchmark_python_deps
    COMMAND ${CMAKE_COMMAND}
        -DPython3_EXECUTABLE=${Python3_EXECUTABLE}
        -DREQUIREMENTS=${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt
        -DCTOON_ROOT=${CTOON_ROOT_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/InstallDeps.cmake
    COMMENT "Installing Python benchmark dependencies"
    USES_TERMINAL
)

add_custom_target(ctoon_benchmark_python
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Python benchmark..."
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/bench_ctoon.py ${CTOON_BENCH_MANIFEST}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmark_python ctoon_benchmark_python_deps)
add_dependencies(ctoon_benchmark ctoon_benchmark_python)
