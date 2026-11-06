# ctoon_bench_zig_toolchain(<name> <ziglang version>)
#
# Zig is never taken from the system or from PATH: each benchmark that needs
# Zig gets its OWN venv holding exactly `ziglang==<version>` from PyPI. Two
# Zig versions are needed at once (zig/ctoon/ needs 0.16.x, zig/zig-toon/
# needs 0.15.2) and pip cannot install two versions of one
# package into the same environment, so there is one venv per version:
#
#     <build>/venv_zig_<version>/        the compiler (python-zig)
#     <build>/zig_cache_<version>/       ZIG_GLOBAL_CACHE_DIR + ZIG_LOCAL_CACHE_DIR
#
# These are separate from the Python benchmark's venv (python/venv) and from
# each other. The separate cache dirs keep the two compilers from sharing
# ~/.cache/zig (fetched packages, compiled std) and keep .zig-cache out of
# the source tree.
#
# Creates the target ctoon_zig_toolchain_<name> and sets, in the caller's scope:
#     CTOON_ZIG_<name>_EXE      the python-zig executable
#     CTOON_ZIG_<name>_PYTHON   that venv's python (handy for helper scripts)
#     CTOON_ZIG_<name>_ENV      VAR=value list for `cmake -E env`
# Needs UV_EXECUTABLE, CTOON_BENCH_PYTHON_VERSION, CTOON_UV_INDEX_ARGS and
# CTOON_BENCH_BINARY_DIR (all set by benchmarks/CMakeLists.txt).
function(ctoon_bench_zig_toolchain NAME VERSION)
    set(_venv "${CTOON_BENCH_BINARY_DIR}/venv_zig_${VERSION}")
    set(_cache "${CTOON_BENCH_BINARY_DIR}/zig_cache_${VERSION}")
    if(WIN32)
        set(_py  "${_venv}/Scripts/python.exe")
        set(_zig "${_venv}/Scripts/python-zig.exe")
    else()
        set(_py  "${_venv}/bin/python")
        set(_zig "${_venv}/bin/python-zig")
    endif()

    add_custom_target(ctoon_zig_toolchain_${NAME}
        COMMAND ${UV_EXECUTABLE} venv --clear --python ${CTOON_BENCH_PYTHON_VERSION} ${_venv}
        COMMAND ${UV_EXECUTABLE} pip install -q --python ${_py} ${CTOON_UV_INDEX_ARGS}
                ziglang==${VERSION}
        COMMAND ${_zig} version
        COMMENT "Zig ${VERSION}: installing ziglang==${VERSION} into ${_venv}"
        USES_TERMINAL
    )

    set(CTOON_ZIG_${NAME}_EXE "${_zig}" PARENT_SCOPE)
    set(CTOON_ZIG_${NAME}_PYTHON "${_py}" PARENT_SCOPE)
    set(CTOON_ZIG_${NAME}_ENV
        "ZIG_GLOBAL_CACHE_DIR=${_cache}/global"
        "ZIG_LOCAL_CACHE_DIR=${_cache}/local"
        PARENT_SCOPE)
endfunction()
