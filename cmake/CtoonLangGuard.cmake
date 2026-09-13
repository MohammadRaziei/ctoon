# ctoon_require_lang(<flag_var> <found_var> <human_name> <install_hint>)
#
# Central policy for the main ctoon project's optional, self-building
# language bindings (Go, Rust, Zig, MATLAB — anything with its own
# external toolchain that isn't compiled by this project's own CMake
# graph, unlike Python). Call this right after a
# find_program()/find_package() toolchain lookup, in every place that
# lookup happens within the main project (tests/, docs/,
# src/bindings/matlab each detect these toolchains independently).
#
# NOT used by benchmarks/ — that's a fully standalone CMake project whose
# whole purpose is to run every implementation it can find, so it always
# warn-and-skips a missing toolchain regardless of any flag here.
#
#   - If neither CTOON_BUILD_ALL_LANGS nor <flag_var> is ON: the language
#     is opt-in and wasn't requested. If its toolchain is missing, that's
#     expected on a partial dev machine — warn and let the caller's own
#     `if(<found_var>) ... endif()` skip it silently, same as before this
#     module existed.
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
