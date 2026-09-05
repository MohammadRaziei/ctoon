set(VENV_DIR ${CMAKE_CURRENT_BINARY_DIR}/venv)
if(WIN32)
    set(VENV_PYTHON ${VENV_DIR}/Scripts/python.exe)
    set(VENV_PIP ${VENV_DIR}/Scripts/pip.exe)
else()
    set(VENV_PYTHON ${VENV_DIR}/bin/python)
    set(VENV_PIP ${VENV_DIR}/bin/pip)
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

add_custom_target(ctoon_benchmarks_python
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Python benchmark..."
    COMMAND ${VENV_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/bench.py
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR}/python.json
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks_python ctoon_benchmarks_python_venv)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_python)
