# ctoon_find_octave()
#
# Locates the GNU Octave toolchain used by the Octave binding and its tests
# (src/bindings/octave, tests/octave). Both places call this, so the lookup
# rules live in one spot. Sets in the caller's scope:
#
#   Octave_FOUND              TRUE only if BOTH octave-cli and mkoctfile exist
#   CTOON_OCTAVE_EXECUTABLE   octave-cli (falls back to octave); cache var, so
#                             -DCTOON_OCTAVE_EXECUTABLE=/path/to/octave-cli
#                             overrides the lookup
#   CTOON_OCTAVE_MKOCTFILE    mkoctfile; cache var, same override rule
#   CTOON_OCTAVE_VERSION      e.g. 8.4.0 (cached after the first probe)
#
# mkoctfile (Debian/Ubuntu: package `octave-dev`, Homebrew/Windows: part of
# the normal install) is only needed to BUILD the MEX gateway. Running and
# testing the built binary needs nothing but Octave itself.
macro(ctoon_find_octave)
    find_program(CTOON_OCTAVE_EXECUTABLE NAMES octave-cli octave
        DOC "GNU Octave command-line interpreter")
    if(CTOON_OCTAVE_EXECUTABLE)
        get_filename_component(_ctoon_octave_bin_dir "${CTOON_OCTAVE_EXECUTABLE}" DIRECTORY)
    endif()
    find_program(CTOON_OCTAVE_MKOCTFILE NAMES mkoctfile
        HINTS ${_ctoon_octave_bin_dir}
        DOC "GNU Octave MEX/OCT compiler driver")

    set(Octave_FOUND FALSE)
    if(CTOON_OCTAVE_EXECUTABLE AND CTOON_OCTAVE_MKOCTFILE)
        set(Octave_FOUND TRUE)
        if(NOT CTOON_OCTAVE_VERSION)
            execute_process(
                COMMAND "${CTOON_OCTAVE_EXECUTABLE}" --norc --quiet --eval "disp(OCTAVE_VERSION)"
                OUTPUT_VARIABLE _ctoon_octave_version
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE _ctoon_octave_rc)
            if(_ctoon_octave_rc EQUAL 0 AND _ctoon_octave_version MATCHES "^[0-9]+\\.[0-9]+")
                set(CTOON_OCTAVE_VERSION "${_ctoon_octave_version}" CACHE INTERNAL
                    "Detected GNU Octave version")
            endif()
        endif()
    endif()
endmacro()

# Oldest Octave the binding is verified against. The wrappers rely on
# isfolder/isfile (Octave 6+) and jsondecode (Octave 7+, used by the tests);
# the first release actually exercised is 8.x.
set(CTOON_OCTAVE_MIN_VERSION "8.1")

set(CTOON_OCTAVE_INSTALL_HINT
    "Install GNU Octave with its development files (Debian/Ubuntu: apt install octave octave-dev; Homebrew: brew install octave) and make sure octave-cli and mkoctfile are on PATH, or pass -DCTOON_OCTAVE_EXECUTABLE=... -DCTOON_OCTAVE_MKOCTFILE=....")
