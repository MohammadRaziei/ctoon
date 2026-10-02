# Environment setup for the Julia test suite, run as
# `julia --project=<binding dir> setup_env.jl <binding dir>` -- see
# tests/julia/CMakeLists.txt, where it is the `ctoon_julia_setup` CTest
# fixture that `ctoon_julia_tests` depends on.
#
# This is the part of the Julia run whose cost has nothing to do with the
# tests themselves: against a fresh depot (the normal case on CI, and after
# every `rm -rf build`), Pkg has to install the General registry, download
# the binding's [deps] and their dependencies, and precompile them. That is
# network- and machine-speed-bound and takes minutes on a cold runner, so it
# gets its own, much larger timeout instead of eating into the tests' budget.
# (The ctoon C sources are NOT part of that download: deps/build.jl uses the
# local checkout when it is running from inside one -- see its header.)
#
# Same two steps, in the same order, as the top of run_tests.jl -- see the
# comment there for why build.jl has to run before Pkg.instantiate().
isempty(ARGS) && error("setup_env.jl: expected the binding directory as ARGS[1]")
binding_dir = ARGS[1]

include(joinpath(binding_dir, "deps", "build.jl"))

import Pkg
Pkg.instantiate()
