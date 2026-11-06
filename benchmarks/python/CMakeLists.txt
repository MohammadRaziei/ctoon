cmake_path(SET VENV_DIR "${CMAKE_CURRENT_BINARY_DIR}/venv")
cmake_path(ABSOLUTE_PATH VENV_DIR NORMALIZE)

if(WIN32)
    cmake_path(APPEND VENV_DIR "Scripts" "python.exe" OUTPUT_VARIABLE VENV_PYTHON)
else()
    cmake_path(APPEND VENV_DIR "bin" "python" OUTPUT_VARIABLE VENV_PYTHON)
endif()

# Visible from report/CMakeLists.txt (a sibling directory) so the memory
# harness can run this exact venv's python in isolation per library,
# without re-deriving the venv path or re-installing anything.
set(CTOON_PY_VENV_PYTHON "${VENV_PYTHON}" CACHE INTERNAL "ctoon python benchmark venv interpreter")

# A real venv (created with uv, Python ${CTOON_BENCH_PYTHON_VERSION}; separate from
# the Zig toolchain venvs). Competitors
# (toon_format, toons, ...) come from PyPI via requirements.txt; only
# ctoon is installed straight from its GitHub repo. Nothing comes from
# a local checkout.
add_custom_target(ctoon_benchmarks_python_venv
    COMMAND ${UV_EXECUTABLE} venv --clear --python ${CTOON_BENCH_PYTHON_VERSION} ${VENV_DIR}
    COMMAND ${UV_EXECUTABLE} pip install -q --python ${VENV_PYTHON} ${CTOON_UV_INDEX_ARGS}
            -r ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt
    COMMAND ${UV_EXECUTABLE} pip install -q --python ${VENV_PYTHON} ${CTOON_UV_INDEX_ARGS}
            --reinstall --no-deps "git+${CTOON_GITHUB_URL}"
    COMMENT "Setting up Python venv and installing benchmark dependencies"
    USES_TERMINAL
)

if(CTOON_BENCH_LOG_DEBUG)
    set(CTOON_PY_LOG_ARG ${CTOON_BENCH_LOGS_DIR}/python.log)
else()
    set(CTOON_PY_LOG_ARG "")
endif()

add_custom_target(ctoon_benchmarks_python
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Python benchmark..."
    COMMAND ${VENV_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/bench.py
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR}/python.json ${CTOON_PY_LOG_ARG}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks_python ctoon_benchmarks_python_venv)
add_dependencies(ctoon_bench_langs ctoon_benchmarks_python)
