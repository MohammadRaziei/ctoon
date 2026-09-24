# Driver script for the Julia test suite, run as
# `julia --project=<binding dir> run_tests.jl <binding dir>` — see
# tests/julia/CMakeLists.txt.
#
# Takes the binding dir as an explicit argument (ARGS[1]) rather than
# relying on the working directory + relative include() paths:
# Julia's include() resolves a relative path against the *including
# file's own directory* (tests/julia/, where this script lives), not
# against the process's cwd — so "deps/build.jl" would resolve to
# tests/julia/deps/build.jl, which doesn't exist. Absolute paths built
# from an explicit argument sidestep that entirely.
#
# Order matters: deps/build.jl must run — and finish writing
# deps/deps.jl — *before* Pkg.instantiate(), because instantiate()
# auto-precompiles every package in the active project, including
# CToon itself (this is the active project, via --project). Precompiling
# CToon.jl means loading it, and CToon.jl errors immediately if
# deps/deps.jl doesn't exist yet.
isempty(ARGS) && error("run_tests.jl: expected the binding directory as ARGS[1]")
binding_dir = ARGS[1]

include(joinpath(binding_dir, "deps", "build.jl"))

import Pkg
Pkg.instantiate()

include(joinpath(binding_dir, "test", "runtests.jl"))

# Lives under the root tests/ folder (this script's own directory), like
# every other language's tests, rather than under the binding's own
# test/ -- same split test/runtests.jl vs tests/julia/run_tests.jl above
# already makes. Run as a second, separate include() (not folded into
# test/runtests.jl itself) so a JSON.jl resolution problem here is
# clearly distinguishable from a failure in CToon.jl's own test suite.
include(joinpath(@__DIR__, "native_json_roundtrip_test.jl"))
