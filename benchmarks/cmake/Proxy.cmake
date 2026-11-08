# ctoon_bench_proxy_env(<out-var> <proxy-url> [<extra env var names>...])
#
# Turns a proxy URL into a VAR=value list for `cmake -E env`. Empty URL ->
# empty list, so a tool whose *_PROXY option is not set runs exactly as before.
# Covers the variables curl, git, uv/reqwest, Go, Julia and Zig read; the
# optional extra names are for tools with their own (CARGO_HTTP_PROXY, npm_config_*).
function(ctoon_bench_proxy_env OUT PROXY)
    set(_env "")
    if(PROXY)
        foreach(_v HTTP_PROXY HTTPS_PROXY ALL_PROXY http_proxy https_proxy all_proxy ${ARGN})
            list(APPEND _env "${_v}=${PROXY}")
        endforeach()
    endif()
    set(${OUT} "${_env}" PARENT_SCOPE)
endfunction()
