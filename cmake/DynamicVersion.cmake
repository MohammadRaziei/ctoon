# DynamicVersion.cmake
#
# Extract semantic version components from a C/C++ header that defines:
#   #define <PREFIX>VERSION_MAJOR <N>
#   #define <PREFIX>VERSION_MINOR <N>
#   #define <PREFIX>VERSION_PATCH <N>
#
# Where <PREFIX> is optional (e.g. "CTOON_").
# Values may optionally be wrapped in parentheses: 1 or (1).
#
# Public API:
#   extract_version_components(
#       HEADER_FILE <path>
#       [VERSION_PREFIX <prefix>]
#       [MAJOR_VAR <var_name>]
#       [MINOR_VAR <var_name>]
#       [PATCH_VAR <var_name>]
#   )
#
#   extract_version_string(
#       HEADER_FILE <path>
#       [VERSION_PREFIX <prefix>]
#       OUTPUT_VAR <var_name>
#   )

include_guard(GLOBAL)

# -------------------------- helpers --------------------------

# Read only version-related #define lines (robust, line-by-line).
function(_dv_read_candidate_lines header_file version_prefix out_lines_var)
    if(NOT EXISTS "${header_file}")
        message(FATAL_ERROR "DynamicVersion: Header file not found: ${header_file}")
    endif()

    # Match lines like:
    #   #define CTOON_VERSION_MAJOR 0
    #   #define CTOON_VERSION_MAJOR (0)
    #
    # Notes:
    # - We anchor at the beginning to avoid matching in comments/strings.
    # - We keep it strict enough to be reliable but not fragile.
    set(_regex "^[ \t]*#[ \t]*define[ \t]+${version_prefix}VERSION_(MAJOR|MINOR|PATCH)[ \t]+\\(?[0-9]+\\)?([ \t].*)?$")

    file(STRINGS "${header_file}" _lines REGEX "${_regex}")
    set(${out_lines_var} "${_lines}" PARENT_SCOPE)
endfunction()

# Extract a single component (MAJOR/MINOR/PATCH) from candidate lines.
function(_dv_extract_component header_file version_prefix lines component_name out_var)
    # e.g. component_name="MAJOR" → match VERSION_MAJOR
    set(_re "^[ \t]*#[ \t]*define[ \t]+${version_prefix}VERSION_${component_name}[ \t]+\\(?([0-9]+)\\)?")

    foreach(_l IN LISTS lines)
        if(_l MATCHES "${_re}")
            set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    message(FATAL_ERROR
        "DynamicVersion: Could not find ${version_prefix}VERSION_${component_name} in ${header_file}\n"
        "DynamicVersion: Ensure the header defines:\n"
        "  #define ${version_prefix}VERSION_${component_name} <number>\n"
    )
endfunction()

# -------------------------- public API --------------------------

function(extract_version_components)
    set(oneValueArgs HEADER_FILE VERSION_PREFIX MAJOR_VAR MINOR_VAR PATCH_VAR)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(NOT ARG_HEADER_FILE)
        message(FATAL_ERROR "extract_version_components: HEADER_FILE is required")
    endif()

    # Optional prefix defaults to empty
    set(_prefix "${ARG_VERSION_PREFIX}")

    _dv_read_candidate_lines("${ARG_HEADER_FILE}" "${_prefix}" _candidate_lines)

    _dv_extract_component("${ARG_HEADER_FILE}" "${_prefix}" "${_candidate_lines}" "MAJOR" _major)
    _dv_extract_component("${ARG_HEADER_FILE}" "${_prefix}" "${_candidate_lines}" "MINOR" _minor)
    _dv_extract_component("${ARG_HEADER_FILE}" "${_prefix}" "${_candidate_lines}" "PATCH" _patch)

    # Export requested variables (only if user asked)
    if(ARG_MAJOR_VAR)
        set(${ARG_MAJOR_VAR} "${_major}" PARENT_SCOPE)
    endif()
    if(ARG_MINOR_VAR)
        set(${ARG_MINOR_VAR} "${_minor}" PARENT_SCOPE)
    endif()
    if(ARG_PATCH_VAR)
        set(${ARG_PATCH_VAR} "${_patch}" PARENT_SCOPE)
    endif()
endfunction()

function(extract_version_string)
    set(oneValueArgs HEADER_FILE VERSION_PREFIX OUTPUT_VAR)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(NOT ARG_HEADER_FILE)
        message(FATAL_ERROR "extract_version_string: HEADER_FILE is required")
    endif()
    if(NOT ARG_OUTPUT_VAR)
        message(FATAL_ERROR "extract_version_string: OUTPUT_VAR is required")
    endif()

    extract_version_components(
        HEADER_FILE "${ARG_HEADER_FILE}"
        VERSION_PREFIX "${ARG_VERSION_PREFIX}"
        MAJOR_VAR _vmajor
        MINOR_VAR _vminor
        PATCH_VAR _vpatch
    )

    set(${ARG_OUTPUT_VAR} "${_vmajor}.${_vminor}.${_vpatch}" PARENT_SCOPE)
endfunction()
