# Driver script for the Julia test suite, run as
# `julia --project=<binding dir> run_tests.jl` with the working directory
# set to the binding dir (src/bindings/julia) — see tests/julia/CMakeLists.txt.
#
# This exists as a real .jl file, rather than inline `-e "..."` code in
# CMakeLists.txt, specifically to sidestep a CMake quoting pitfall: a
# `-e` string containing both semicolons and nested escaped double
# quotes (needed for `include("...")`) gets mangled when CMake's Unix
# Makefiles generator writes it into a Makefile recipe — the shell ends
# up seeing unquoted `(` characters ("Syntax error: "(" unexpected").
# A plain script file has no quoting to get wrong.
import Pkg
Pkg.instantiate()
include("deps/build.jl")
include("test/runtests.jl")
