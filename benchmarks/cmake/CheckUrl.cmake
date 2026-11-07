# Fail-fast reachability probe, run as a build step:
#   cmake -DURL=<url> -DLABEL=<text> -DTMP_FILE=<path> [-DHINT=<text>] [-DTIMEOUT=20] [-DINSECURE=ON] -P CheckUrl.cmake
# Prints what it is checking and how long it took; on failure stops the build
# with the reason (instead of uv retrying for minutes and printing "timed out").
foreach(_v URL LABEL TMP_FILE)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "CheckUrl.cmake: -D${_v}=... is required")
    endif()
endforeach()
if(NOT DEFINED TIMEOUT)
    set(TIMEOUT 20)
endif()

message(STATUS "[net] checking ${LABEL}: ${URL} (timeout ${TIMEOUT}s)")
string(TIMESTAMP _t0 "%s")
set(_tls "")
if(INSECURE)
    set(_tls TLS_VERIFY OFF)  # the mirror host is trusted, same as uv's --allow-insecure-host
endif()
file(DOWNLOAD "${URL}" "${TMP_FILE}" TIMEOUT ${TIMEOUT} ${_tls} STATUS _st)
string(TIMESTAMP _t1 "%s")
math(EXPR _dt "${_t1} - ${_t0}")
file(REMOVE "${TMP_FILE}")

list(GET _st 0 _code)
list(GET _st 1 _msg)
if(_code EQUAL 0)
    message(STATUS "[net] OK: ${LABEL} reachable (${_dt}s)")
else()
    message(FATAL_ERROR "[net] FAILED: ${LABEL} not reachable after ${_dt}s -> ${_msg} (code ${_code})\n${HINT}")
endif()
