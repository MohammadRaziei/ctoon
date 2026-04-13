# FixLCovPaths.cmake
# Convert absolute paths in LCOV files to relative paths
# This is useful for SonarQube and other tools that expect relative paths

if(NOT DEFINED COV_FILE)
    message(FATAL_ERROR "COV_FILE is not defined")
endif()

if(NOT EXISTS "${COV_FILE}")
    message(WARNING "Coverage file does not exist: ${COV_FILE}")
    return()
endif()

# Read the coverage file
file(READ "${COV_FILE}" COVERAGE_CONTENT)

# If SOURCE_PATH is defined, replace absolute paths with relative paths
if(DEFINED SOURCE_PATH)
    # Remove trailing slash if present
    string(REGEX REPLACE "/$" "" SOURCE_PATH "${SOURCE_PATH}")
    
    # Replace absolute paths with relative paths
    string(REPLACE "${SOURCE_PATH}/" "" COVERAGE_CONTENT "${COVERAGE_CONTENT}")
elseif(DEFINED PREFIX_PATH)
    # Remove trailing slash if present
    string(REGEX REPLACE "/$" "" PREFIX_PATH "${PREFIX_PATH}")
    
    # Replace absolute paths with prefix-relative paths
    string(REPLACE "${PREFIX_PATH}/" "" COVERAGE_CONTENT "${COVERAGE_CONTENT}")
endif()

# Write the modified content back
file(WRITE "${COV_FILE}" "${COVERAGE_CONTENT}")

message(STATUS "Fixed LCOV paths in: ${COV_FILE}")
