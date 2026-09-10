cmake_path(SET VENV_DIR "${CMAKE_CURRENT_BINARY_DIR}/venv")
cmake_path(ABSOLUTE_PATH VENV_DIR NORMALIZE)

if(WIN32)
    cmake_path(APPEND VENV_DIR "Scripts" "python.exe" OUTPUT_VARIABLE VENV_PYTHON)
    cmake_path(APPEND VENV_DIR "Scripts" "pip.exe" OUTPUT_VARIABLE VENV_PIP)
else()
    cmake_path(APPEND VENV_DIR "bin" "python" OUTPUT_VARIABLE VENV_PYTHON)
    cmake_path(APPEND VENV_DIR "bin" "pip" OUTPUT_VARIABLE VENV_PIP)
endif()

# A real venv (not the system Python) — installs ctoon and toon_format
# straight from their GitHub repos, exactly like every other language
# fetches its dependencies here. Nothing comes from a local checkout.
add_custom_target(ctoon_benchmarks_python_venv
    COMMAND ${Python3_EXECUTABLE} -m venv ${VENV_DIR}
    COMMAND ${VENV_PIP} install -q --upgrade pip
    COMMAND ${VENV_PIP} install -q -r ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt
    COMMAND ${VENV_PIP} install -q --force-reinstall --no-deps
            "git+${CTOON_GITHUB_URL}"
    COMMAND ${VENV_PIP} install -q --force-reinstall --no-deps
            "git+https://github.com/toon-format/toon-python.git"
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
add_dependencies(ctoon_benchmarks ctoon_benchmarks_python)
