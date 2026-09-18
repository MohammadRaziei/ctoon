# Download.cmake -- generic file(DOWNLOAD) wrapper for use in script
# mode (cmake -P), so a single add_custom_command can invoke it
# without duplicating the same DOWNLOAD/STATUS-check boilerplate.
#
# Usage:
#   cmake -DURL=<url> -DDEST=<path> -P Download.cmake

if(NOT URL OR NOT DEST)
  message(FATAL_ERROR "Download.cmake: both -DURL=... and -DDEST=... are required")
endif()

if(EXISTS "${DEST}")
  return()
endif()

get_filename_component(_dest_dir "${DEST}" DIRECTORY)
file(MAKE_DIRECTORY "${_dest_dir}")

message(STATUS "Downloading ${URL} -> ${DEST}")
file(DOWNLOAD "${URL}" "${DEST}" STATUS _status TIMEOUT 60)
list(GET _status 0 _code)
if(NOT _code EQUAL 0)
  list(GET _status 1 _msg)
  file(REMOVE "${DEST}")
  message(FATAL_ERROR "Download failed (${_msg}): ${URL}")
endif()
