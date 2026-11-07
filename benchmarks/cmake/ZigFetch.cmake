# `zig fetch --save` of a GitHub repo, with a fallback for when Zig's own HTTP
# client cannot reach GitHub:
#   cmake -P ZigFetch.cmake <repo-url> <ref|-> <zig> [ENV=VALUE...]
#
# 1. zig fetch --save git+<repo>#<ref>, up to 3 tries.
# 2. If that never works (typically "unable to discover remote git server
#    capabilities: HttpConnectionClosing", a Zig 0.15 bug behind proxies,
#    ziglang/zig#25996), git itself clones the repo (git handles proxies fine),
#    `git archive` makes a tarball and zig fetches that local file. The package
#    hash is the same as with the git+ URL; only the saved .url is the local
#    tarball, kept next to the Zig cache so it survives reruns.
math(EXPR _last "${CMAKE_ARGC} - 1")
set(_repo "${CMAKE_ARGV3}")
set(_ref "${CMAKE_ARGV4}")
set(_zig "${CMAKE_ARGV5}")
set(_env "")
foreach(_i RANGE 6 ${_last})
    list(APPEND _env "${CMAKE_ARGV${_i}}")
endforeach()
if(_ref STREQUAL "-")
    set(_ref "")
endif()

set(_url "git+${_repo}")
if(_ref)
    set(_url "${_url}#${_ref}")
endif()

foreach(_n RANGE 1 3)
    execute_process(COMMAND ${CMAKE_COMMAND} -E env ${_env} ${_zig} fetch --save "${_url}" RESULT_VARIABLE _rc)
    if(_rc EQUAL 0)
        return()
    endif()
    if(_n LESS 3)
        message(STATUS "[zig-fetch] attempt ${_n}/3 failed (exit ${_rc}), retrying in 5s")
        execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 5)
    endif()
endforeach()

message(STATUS "[zig-fetch] Zig could not fetch ${_url}; falling back to: git clone + git archive + zig fetch of the tarball")
set(_dir "${CMAKE_BINARY_DIR}/zig-git-fetch")
foreach(_e IN LISTS _env)
    if(_e MATCHES "^ZIG_GLOBAL_CACHE_DIR=(.*)/[^/]+$")
        set(_dir "${CMAKE_MATCH_1}/git-fetch")
    endif()
endforeach()
get_filename_component(_name "${_repo}" NAME_WE)
set(_src "${_dir}/${_name}-src")
set(_tar "${_dir}/${_name}.tar.gz")
file(REMOVE_RECURSE "${_src}")
file(MAKE_DIRECTORY "${_dir}")
set(_branch "")
if(_ref)
    set(_branch --branch "${_ref}")
endif()
message(STATUS "[zig-fetch] git clone --depth 1 ${_ref} ${_repo} -> ${_src}")
execute_process(COMMAND git clone --depth 1 ${_branch} "${_repo}" "${_src}" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "[zig-fetch] git clone of ${_repo} failed (exit ${_rc}): GitHub is not reachable")
endif()
execute_process(COMMAND git -C "${_src}" archive --format=tar.gz -o "${_tar}" HEAD RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "[zig-fetch] git archive failed (exit ${_rc})")
endif()
message(STATUS "[zig-fetch] zig fetch --save ${_tar}")
execute_process(COMMAND ${CMAKE_COMMAND} -E env ${_env} ${_zig} fetch --save "${_tar}" RESULT_VARIABLE _rc)
file(REMOVE_RECURSE "${_src}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "[zig-fetch] zig fetch of the local tarball failed (exit ${_rc})")
endif()
