# Configure-time logging helpers shared by the benchmarks/*/CMakeLists.txt.

# ctoon_bench_fetch_note(<name> <url> <ref>)
# Says, right before FetchContent_MakeAvailable(), whether the dependency is
# cloned from scratch or an existing clone is updated, so a slow configure is
# never a silent one.
function(ctoon_bench_fetch_note NAME URL REF)
    string(TOLOWER "${NAME}" _n)
    if(FETCHCONTENT_BASE_DIR)
        set(_base "${FETCHCONTENT_BASE_DIR}")
    else()
        set(_base "${CMAKE_BINARY_DIR}/_deps")
    endif()
    if(EXISTS "${_base}/${_n}-src/.git")
        message(STATUS "[fetch] ${NAME}: updating existing clone ${_base}/${_n}-src (git fetch ${URL} @ ${REF})")
    else()
        message(STATUS "[fetch] ${NAME}: git clone ${URL} @ ${REF} -> ${_base}/${_n}-src")
    endif()
endfunction()
