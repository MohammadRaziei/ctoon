# SpecVersion.cmake
#
# Resolve which released version of toon-format/spec a pinned commit
# corresponds to, WITHOUT cloning it — just two file(DOWNLOAD) calls against
# GitHub's raw content server. Lets the root CMakeLists.txt log/know the
# spec version before tests/CMakeLists.txt does the real (full) clone via
# FetchContent.
#
# Sources (both read at the same GIT_REF, so they always describe the exact
# commit being pinned, never whatever is newest on main):
#   package.json  -> "version": "4.1.1"        => TAG_VAR    = v4.1.1
#   CHANGELOG.md  -> ## [4.1] - 2026-07-26      => VERSION_VAR = 4.1
#                                                  DATE_VAR    = 2026-07-26
#
# Public API:
#   extract_spec_version(
#       GIT_REF     <commit-or-tag>
#       TAG_VAR     <var_name>
#       VERSION_VAR <var_name>
#       DATE_VAR    <var_name>
#   )
#
# On fetch/parse failure this WARNs and leaves the output variables unset,
# matching this project's existing policy for the spec repo (see
# tests/CMakeLists.txt) of degrading spec-version reporting gracefully
# rather than failing the whole configure over a network hiccup.

include_guard(GLOBAL)

function(extract_spec_version)
    cmake_parse_arguments(SV "" "GIT_REF;TAG_VAR;VERSION_VAR;DATE_VAR" "" ${ARGN})

    set(_raw_base "https://raw.githubusercontent.com/toon-format/spec/${SV_GIT_REF}")
    set(_pkg "${CMAKE_CURRENT_BINARY_DIR}/toon-spec-package.json")
    set(_changelog "${CMAKE_CURRENT_BINARY_DIR}/toon-spec-CHANGELOG.md")

    file(DOWNLOAD "${_raw_base}/package.json" "${_pkg}" STATUS _pkg_status TLS_VERIFY ON)
    file(DOWNLOAD "${_raw_base}/CHANGELOG.md" "${_changelog}" STATUS _cl_status TLS_VERIFY ON)
    list(GET _pkg_status 0 _pkg_code)
    list(GET _cl_status 0 _cl_code)
    if(NOT _pkg_code EQUAL 0 OR NOT _cl_code EQUAL 0)
        message(WARNING
            "SpecVersion: could not fetch toon-format/spec@${SV_GIT_REF} "
            "(package.json status ${_pkg_code}, CHANGELOG.md status ${_cl_code}) "
            "- spec version will not be reported.")
        return()
    endif()

    file(STRINGS "${_pkg}" _version_line REGEX "\"version\"[ \t]*:")
    file(STRINGS "${_changelog}" _changelog_line
         REGEX "^## \\[[0-9]+\\.[0-9]+\\] - [0-9-]+" LIMIT_COUNT 1)

    if(NOT _version_line MATCHES "\"version\"[ \t]*:[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\""
       OR NOT _changelog_line MATCHES "\\[([0-9]+\\.[0-9]+)\\] - ([0-9-]+)")
        message(WARNING "SpecVersion: fetched toon-format/spec@${SV_GIT_REF} but could not parse its version - spec version will not be reported.")
        return()
    endif()

    string(REGEX REPLACE "^.*\"version\"[ \t]*:[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\".*$" "\\1" _pkg_version "${_version_line}")
    string(REGEX REPLACE "^.*\\[([0-9]+\\.[0-9]+)\\] - ([0-9-]+).*$" "\\1" _cl_version "${_changelog_line}")
    string(REGEX REPLACE "^.*\\[([0-9]+\\.[0-9]+)\\] - ([0-9-]+).*$" "\\2" _cl_date "${_changelog_line}")

    set(${SV_TAG_VAR} "v${_pkg_version}" PARENT_SCOPE)
    set(${SV_VERSION_VAR} "${_cl_version}" PARENT_SCOPE)
    set(${SV_DATE_VAR} "${_cl_date}" PARENT_SCOPE)
endfunction()
