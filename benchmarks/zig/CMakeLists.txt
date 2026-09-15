# ctoon is fetched by `zig fetch --save`, the same way `go get` fetches
# ctoon for the Go benchmark — run before every build so build.zig.zon's
# dependency hash always matches ctoon's current master, instead of
# committing a fixed version.
add_custom_target(ctoon_benchmarks_zig_deps
    COMMAND ${ZIG_EXECUTABLE} fetch --save git+https://github.com/mohammadraziei/ctoon.git
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Fetching Zig benchmark dependency (ctoon) from GitHub"
    USES_TERMINAL
)

if(CTOON_BENCH_LOG_DEBUG)
    set(CTOON_ZIG_LOG_ARG ${CTOON_BENCH_LOGS_DIR}/zig.log)
else()
    set(CTOON_ZIG_LOG_ARG "")
endif()

# On some Linux setups (notably Debian/Ubuntu multiarch layouts, and
# some WSL/conda environments), Zig's bundled lld linker looks for libc
# pieces (libm.so.6, etc.) under /lib64 by default and doesn't find them
# if the system instead keeps them under a triplet directory like
# /usr/lib/x86_64-linux-gnu. Rather than assume any particular layout —
# or ask anyone to add a system-wide /lib64 symlink — ask the C compiler
# CMake already found where IT looks for libraries by default
# (CMAKE_C_IMPLICIT_LINK_DIRECTORIES, populated by CMake's own compiler
# detection) and hand each of those to zig via --search-prefix. No-op on
# setups that don't need it (e.g. /lib64 already resolves correctly).
set(CTOON_ZIG_SEARCH_PREFIX_ARGS "")
set(CTOON_ZIG_LIB_PATH_LIST "")
foreach(_dir IN LISTS CMAKE_C_IMPLICIT_LINK_DIRECTORIES)
    # zig's --search-prefix requires every path given to actually exist
    # as a directory — GCC's own implicit-link-directory list routinely
    # includes paths that don't exist on disk (this is normal for GCC;
    # it's an optional search list, not a guarantee), so filter those
    # out before handing the list to zig.
    if(IS_DIRECTORY "${_dir}")
        list(APPEND CTOON_ZIG_SEARCH_PREFIX_ARGS --search-prefix "${_dir}")
        list(APPEND CTOON_ZIG_LIB_PATH_LIST "${_dir}")
    endif()
endforeach()
# Belt-and-suspenders: also set LD_LIBRARY_PATH to the same list, in case
# --search-prefix and LD_LIBRARY_PATH aren't both honored the same way
# in every zig/lld version — whichever one actually gets picked up is
# enough. Harmless to set when unneeded.
string(REPLACE ";" ":" CTOON_ZIG_LD_LIBRARY_PATH "${CTOON_ZIG_LIB_PATH_LIST}")

# --search-prefix alone isn't enough on setups where `zig libc`'s own
# native-libc autodetection is the thing pointing at the wrong sysroot
# (e.g. conda/anaconda environments): that autodetection shells out to
# whatever `cc` resolves to on PATH, completely independent of
# CMAKE_C_COMPILER, so a stray conda `cc` ahead of the system one on
# PATH silently pulls in conda's (often older) glibc regardless of what
# compiler this CMake build was configured with. Generate an explicit
# zig libc.txt from CMake's own compiler-detection results instead,
# and pass it via --libc so zig never has to guess.
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-file-name=crt1.o
    OUTPUT_VARIABLE CTOON_ZIG_CRT1_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
get_filename_component(CTOON_ZIG_CRT_DIR "${CTOON_ZIG_CRT1_PATH}" DIRECTORY)

set(CTOON_ZIG_LIBC_FILE ${CMAKE_CURRENT_BINARY_DIR}/zig-libc.txt)
if(CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES AND CTOON_ZIG_CRT_DIR)
    list(GET CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES 0 CTOON_ZIG_INCLUDE_DIR)
    file(WRITE ${CTOON_ZIG_LIBC_FILE}
"include_dir=${CTOON_ZIG_INCLUDE_DIR}
sys_include_dir=${CTOON_ZIG_INCLUDE_DIR}
crt_dir=${CTOON_ZIG_CRT_DIR}
msvc_lib_dir=
kernel32_lib_dir=
gcc_dir=
")
    set(CTOON_ZIG_LIBC_ARGS --libc ${CTOON_ZIG_LIBC_FILE})
else()
    # Couldn't resolve crt1.o / implicit include dirs (unusual compiler
    # setup) — fall back to search-prefix-only behavior rather than
    # writing a broken libc.txt that would hard-fail the build.
    set(CTOON_ZIG_LIBC_ARGS "")
endif()

add_custom_target(ctoon_benchmarks_zig
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running Zig benchmark..."
    COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CTOON_ZIG_LD_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH}"
            ${ZIG_EXECUTABLE} build ${CTOON_ZIG_LIBC_ARGS} ${CTOON_ZIG_SEARCH_PREFIX_ARGS} --release=fast run --
            ${CTOON_BENCH_MANIFEST} ${CTOON_BENCH_RESULTS_DIR}/zig.json ${CTOON_ZIG_LOG_ARG}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(ctoon_benchmarks_zig ctoon_benchmarks_zig_deps)
add_dependencies(ctoon_benchmarks ctoon_benchmarks_zig)
