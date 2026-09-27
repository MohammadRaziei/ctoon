# ctoon_lang_wanted(<flag_var> <out_var>)
#
# TRUE when this language binding was actually asked for
# (CTOON_BUILD_ALL_LANGS or <flag_var> is ON). Call this BEFORE any
# find_program()/find_package() toolchain probe for that language and skip
# the probe entirely on FALSE - an opted-out language should cost nothing
# at configure time (no toolchain search, no version check, no "Found X"
# noise), not get auto-detected and built just because it happens to be
# installed on the machine.
function(ctoon_lang_wanted FLAG_VAR OUT_VAR)
    if(CTOON_BUILD_ALL_LANGS OR ${FLAG_VAR})
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

# ctoon_require_lang(<flag_var> <found_var> <human_name> <install_hint>)
#
# Central policy for the main ctoon project's optional, self-building
# language bindings (Go, Rust, Zig, MATLAB — anything with its own
# external toolchain that isn't compiled by this project's own CMake
# graph, unlike Python). Call this right after a
# find_program()/find_package() toolchain lookup, in every place that
# lookup happens within the main project (tests/, docs/,
# src/bindings/matlab each detect these toolchains independently) — and
# only after ctoon_lang_wanted() above has already confirmed the language
# was actually asked for (that's what makes it safe to always die loudly
# below on a missing toolchain rather than warn-and-skip).
#
# NOT used by benchmarks/ — that's a fully standalone CMake project whose
# whole purpose is to run every implementation it can find, so it always
# warn-and-skips a missing toolchain regardless of any flag here.
#
#   - If neither CTOON_BUILD_ALL_LANGS nor <flag_var> is ON: unreachable in
#     normal use (the caller should have already return()'d via
#     ctoon_lang_wanted() before probing) — kept as a defensive fallback:
#     warn and let the caller's own `if(<found_var>) ... endif()` skip it.
#   - If CTOON_BUILD_ALL_LANGS or <flag_var> IS ON: the person explicitly
#     asked for this language. A missing toolchain is now a real
#     configure-time error (FATAL_ERROR), not a silent skip — CI or a
#     local build that opted in should fail loud, not produce a
#     quietly-incomplete result.
function(ctoon_require_lang FLAG_VAR FOUND_VAR HUMAN_NAME INSTALL_HINT)
    if((CTOON_BUILD_ALL_LANGS OR ${FLAG_VAR}) AND NOT ${FOUND_VAR})
        message(FATAL_ERROR
            "${HUMAN_NAME} was explicitly requested (${FLAG_VAR}=ON or "
            "CTOON_BUILD_ALL_LANGS=ON) but its toolchain was not found. "
            "${INSTALL_HINT}")
    elseif(NOT ${FOUND_VAR})
        message(WARNING
            "${HUMAN_NAME} toolchain not found — ${HUMAN_NAME} skipped. "
            "${INSTALL_HINT} (Set ${FLAG_VAR}=ON or CTOON_BUILD_ALL_LANGS=ON "
            "to make a missing toolchain a hard error instead.)")
    endif()
endfunction()

# ctoon_require_lang_version(<actual_version> <min_version> <human_name>)
#
# A MACRO, not a function: `return()` inside it exits the CMakeLists.txt
# that called it, not just this check. Call it right after
# ctoon_require_lang() has confirmed the toolchain exists (this only makes
# sense once that's true — an unset <actual_version> would just compare
# less than everything).
#
# The toolchain being too old is a different situation from it being
# missing: unlike ctoon_require_lang()'s FATAL_ERROR-when-explicitly-
# requested policy, this always WARNs and skips regardless of
# CTOON_BUILD_ALL_LANGS/<flag_var> — a wrong version isn't something
# installing-the-package fixes the way a missing one is, and CI's real
# signal for "this language's tests didn't run" is the WARNING itself,
# still visible either way.
macro(ctoon_require_lang_version ACTUAL_VERSION MIN_VERSION HUMAN_NAME)
    if("${ACTUAL_VERSION}" VERSION_LESS "${MIN_VERSION}")
        message(WARNING
            "${HUMAN_NAME} ${ACTUAL_VERSION} found, but ${MIN_VERSION}+ is "
            "required — skipping ${HUMAN_NAME}. Upgrade ${HUMAN_NAME} to "
            "${MIN_VERSION} or newer.")
        return()
    endif()
endmacro()
