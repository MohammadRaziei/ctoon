# Run a command up to <tries> times (GitHub drops connections now and then):
#   cmake -P Retry.cmake <tries> <command> [args...]
# Output goes straight to the terminal; stops with the last exit code's message
# when every try failed.
math(EXPR _last "${CMAKE_ARGC} - 1")
set(_cmd "")
foreach(_i RANGE 4 ${_last})
    list(APPEND _cmd "${CMAKE_ARGV${_i}}")
endforeach()
set(_tries ${CMAKE_ARGV3})
foreach(_n RANGE 1 ${_tries})
    execute_process(COMMAND ${_cmd} RESULT_VARIABLE _rc)
    if(_rc EQUAL 0)
        return()
    endif()
    if(_n LESS _tries)
        message(STATUS "[retry] attempt ${_n}/${_tries} failed (exit ${_rc}), retrying in 5s")
        execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 5)
    endif()
endforeach()
message(FATAL_ERROR "[retry] failed after ${_tries} attempts: ${_cmd}")
