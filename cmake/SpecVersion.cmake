# SpecVersion.cmake
#
# Resolve which toon-format/spec release this project builds against, with
# a persistent, git-committed cache (supported_spec.conf) so normal
# reconfigures and offline/CI builds don't need the network at all.
#
# Public API:
#   resolve_spec_version(
#       TAG_VAR     <var_name>   # e.g. v4.1.1
#       VERSION_VAR <var_name>   # e.g. 4.1
#       DATE_VAR    <var_name>   # e.g. 2026-07-26
#   )
#
# Caching strategy:
#   ${CMAKE_BINARY_DIR}/supported_spec.conf  - "have we already resolved
#       this in the current build tree?" marker. Absent  -> try a fresh
#       resolve. Present -> skip straight to the committed cache below, no
#       network calls on every reconfigure.
#   ${CMAKE_SOURCE_DIR}/supported_spec.conf  - the actual, git-committed
#       cache/fallback. Written only after a fresh resolve succeeds;
#       otherwise read as-is.
#
# Fresh-resolve steps (only run when the build-tree marker above is
# missing):
#   1. Read package.json + CHANGELOG.md off `main` (they only change when a
#      release is cut, so this is a safe way to find the latest release
#      without a manually-maintained commit hash).
#   2. Confirm that release's tag is actually fetchable, via `curl -I`
#      against the same codeload archive URL FetchContent will later clone
#      - HEAD only, nothing is downloaded. If curl itself can't run (not
#      installed, DNS down, etc.) we don't let that block the cache: assume
#      the tag is fine and move on. An actual HTTP error (404 etc.) IS
#      treated as a real failure, though.
#   3. Success -> write supported_spec.conf to both the build tree (marker)
#      and the source tree (cache). Anything short of that -> fall back to
#      reading the source tree's supported_spec.conf if it exists, else
#      WARN (non-fatal) and leave the output variables empty.

include_guard(GLOBAL)

# -------------------------- helpers --------------------------

# Fetch package.json/CHANGELOG.md at GIT_REF and parse out the version.
# WARNs and leaves outputs unset on any fetch/parse failure.
function(_sv_fetch_from_github)
    cmake_parse_arguments(SV "" "GIT_REF;TAG_VAR;VERSION_VAR;DATE_VAR" "" ${ARGN})

    set(_raw_base "https://raw.githubusercontent.com/toon-format/spec/${SV_GIT_REF}")
    set(_pkg "${CMAKE_BINARY_DIR}/toon-spec-package.json")
    set(_changelog "${CMAKE_BINARY_DIR}/toon-spec-CHANGELOG.md")

    file(DOWNLOAD "${_raw_base}/package.json" "${_pkg}" STATUS _pkg_status TLS_VERIFY ON)
    file(DOWNLOAD "${_raw_base}/CHANGELOG.md" "${_changelog}" STATUS _cl_status TLS_VERIFY ON)
    list(GET _pkg_status 0 _pkg_code)
    list(GET _cl_status 0 _cl_code)
    if(NOT _pkg_code EQUAL 0 OR NOT _cl_code EQUAL 0)
        message(WARNING
            "SpecVersion: could not fetch toon-format/spec@${SV_GIT_REF} "
            "(package.json status ${_pkg_code}, CHANGELOG.md status ${_cl_code}).")
        return()
    endif()

    file(STRINGS "${_pkg}" _version_line REGEX "\"version\"[ \t]*:")
    file(STRINGS "${_changelog}" _changelog_line
         REGEX "^## \\[[0-9]+\\.[0-9]+\\] - [0-9-]+" LIMIT_COUNT 1)

    if(NOT _version_line MATCHES "\"version\"[ \t]*:[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\""
       OR NOT _changelog_line MATCHES "\\[([0-9]+\\.[0-9]+)\\] - ([0-9-]+)")
        message(WARNING "SpecVersion: fetched toon-format/spec@${SV_GIT_REF} but could not parse its version.")
        return()
    endif()

    string(REGEX REPLACE "^.*\"version\"[ \t]*:[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)\".*$" "\\1" _pkg_version "${_version_line}")
    string(REGEX REPLACE "^.*\\[([0-9]+\\.[0-9]+)\\] - ([0-9-]+).*$" "\\1" _cl_version "${_changelog_line}")
    string(REGEX REPLACE "^.*\\[([0-9]+\\.[0-9]+)\\] - ([0-9-]+).*$" "\\2" _cl_date "${_changelog_line}")

    set(${SV_TAG_VAR} "v${_pkg_version}" PARENT_SCOPE)
    set(${SV_VERSION_VAR} "${_cl_version}" PARENT_SCOPE)
    set(${SV_DATE_VAR} "${_cl_date}" PARENT_SCOPE)
endfunction()

# HEAD-check a URL with curl, without downloading its body. TRUE on a real
# 2xx, FALSE on a real HTTP error (404 etc). If curl can't even be run,
# that's not an answer about the URL either way, so we don't block on it:
# TRUE ("assume it's fine"), per explicit request.
function(_sv_url_ok url out_var)
    set(${out_var} TRUE PARENT_SCOPE) # default: couldn't check -> assume OK
    find_program(_sv_curl curl)
    if(NOT _sv_curl)
        return()
    endif()

    execute_process(
        COMMAND ${_sv_curl} -sI -o /dev/null -w "%{http_code}" --max-time 15 "${url}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _http_code
        ERROR_QUIET
    )
    if(NOT _rc EQUAL 0)
        return() # curl couldn't run the request at all (no net, DNS, ...)
    endif()

    if(NOT _http_code MATCHES "^2[0-9][0-9]$")
        set(${out_var} FALSE PARENT_SCOPE) # curl got a real answer: an error
    endif()
endfunction()

function(_sv_write_conf path tag version date)
    file(WRITE "${path}" "CTOON_SPEC_TAG=${tag}\nCTOON_SPEC_VERSION=${version}\nCTOON_SPEC_DATE=${date}\n")
endfunction()

function(_sv_read_conf path out_tag out_version out_date)
    file(STRINGS "${path}" _lines)
    foreach(_l IN LISTS _lines)
        if(_l MATCHES "^CTOON_SPEC_TAG=(.*)$")
            set(_tag "${CMAKE_MATCH_1}")
        elseif(_l MATCHES "^CTOON_SPEC_VERSION=(.*)$")
            set(_version "${CMAKE_MATCH_1}")
        elseif(_l MATCHES "^CTOON_SPEC_DATE=(.*)$")
            set(_date "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    set(${out_tag} "${_tag}" PARENT_SCOPE)
    set(${out_version} "${_version}" PARENT_SCOPE)
    set(${out_date} "${_date}" PARENT_SCOPE)
endfunction()

# -------------------------- public API --------------------------

function(resolve_spec_version)
    cmake_parse_arguments(RSV "" "TAG_VAR;VERSION_VAR;DATE_VAR" "" ${ARGN})

    set(_build_conf "${CMAKE_BINARY_DIR}/supported_spec.conf")
    set(_root_conf "${CMAKE_SOURCE_DIR}/supported_spec.conf")

    if(EXISTS "${_build_conf}")
        # Already resolved earlier in this build tree - trust the
        # committed cache, no network calls on every reconfigure.
        if(EXISTS "${_root_conf}")
            _sv_read_conf("${_root_conf}" _tag _version _date)
        else()
            message(WARNING "SpecVersion: ${_build_conf} exists but ${_root_conf} is missing.")
        endif()
    else()
        _sv_fetch_from_github(GIT_REF main TAG_VAR _tag VERSION_VAR _version DATE_VAR _date)

        set(_resolved FALSE)
        if(_tag)
            _sv_url_ok("https://codeload.github.com/toon-format/spec/zip/refs/tags/${_tag}" _tag_ok)
            if(_tag_ok)
                set(_resolved TRUE)
            endif()
        endif()

        if(_resolved)
            _sv_write_conf("${_build_conf}" "${_tag}" "${_version}" "${_date}")
            _sv_write_conf("${_root_conf}" "${_tag}" "${_version}" "${_date}")
        elseif(EXISTS "${_root_conf}")
            _sv_read_conf("${_root_conf}" _tag _version _date)
        else()
            message(WARNING "SpecVersion: could not resolve toon-format/spec version and no cached ${_root_conf} exists.")
        endif()
    endif()

    if(RSV_TAG_VAR)
        set(${RSV_TAG_VAR} "${_tag}" PARENT_SCOPE)
    endif()
    if(RSV_VERSION_VAR)
        set(${RSV_VERSION_VAR} "${_version}" PARENT_SCOPE)
    endif()
    if(RSV_DATE_VAR)
        set(${RSV_DATE_VAR} "${_date}" PARENT_SCOPE)
    endif()
endfunction()
