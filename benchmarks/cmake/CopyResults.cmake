# CopyResults.cmake -- copy every top-level *.json from SRC into DST.
#
#   cmake -DSRC=<dir> -DDST=<dir> -P CopyResults.cmake
#
# Globs at run time (not configure time): the files only exist once the
# language benchmarks have run.
if(NOT SRC OR NOT DST)
  message(FATAL_ERROR "CopyResults.cmake: both -DSRC=... and -DDST=... are required")
endif()
file(MAKE_DIRECTORY "${DST}")
file(GLOB _jsons "${SRC}/*.json")
if(NOT _jsons)
  message(FATAL_ERROR "CopyResults.cmake: no *.json in ${SRC} -- run the language benchmarks first")
endif()
foreach(_json IN LISTS _jsons)
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy "${_json}" "${DST}" COMMAND_ERROR_IS_FATAL ANY)
endforeach()
