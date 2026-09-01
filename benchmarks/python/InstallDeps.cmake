# InstallDeps.cmake — installs Python benchmark dependencies automatically.
# Called via: cmake -DPython3_EXECUTABLE=... -DREQUIREMENTS=... -DCTOON_ROOT=... -P InstallDeps.cmake

function(pip_install)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -m pip install -q ${ARGN}
        RESULT_VARIABLE _rc
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(NOT _rc EQUAL 0)
        # Likely PEP 668 "externally-managed-environment" — retry forcing it.
        execute_process(
            COMMAND ${Python3_EXECUTABLE} -m pip install -q --break-system-packages ${ARGN}
            RESULT_VARIABLE _rc
        )
    endif()
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "pip install failed for: ${ARGN}")
    endif()
endfunction()

pip_install(-r "${REQUIREMENTS}")
pip_install("git+https://github.com/toon-format/toon-python.git")
pip_install("${CTOON_ROOT}")
