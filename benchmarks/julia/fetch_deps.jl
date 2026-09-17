# Fetches ctoon's Julia binding from GitHub via a git URL + subdir —
# the Pkg equivalent of `go get`/`pip install git+...` — into this
# benchmark's own isolated Pkg environment. Run as
# `julia --project=<this dir> fetch_deps.jl <github url>`.
#
# A real script file rather than `-e "import Pkg; Pkg.add(...)"`
# deliberately: a CMake COMMAND string containing both a semicolon and
# nested escaped quotes gets mangled by the Unix Makefiles generator —
# see tests/julia/run_tests.jl's comment for the full explanation.
isempty(ARGS) && error("fetch_deps.jl: expected the ctoon GitHub URL as ARGS[1]")
github_url = ARGS[1]

import Pkg
Pkg.add(url=github_url, subdir="src/bindings/julia")
