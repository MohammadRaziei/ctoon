# Documenter.jl build script for CToon.jl — the Julia-ecosystem
# equivalent of docs/python/source/conf.py (Sphinx) and `cargo doc`
# for Rust, called the same way: `julia --project=docs/julia make.jl`,
# driven by docs/julia/CreateDocs.cmake.
#
# Requires the CToon package itself to already be built (deps/build.jl
# — see src/bindings/julia/README.md) so `using CToon` and its
# docstrings can be loaded.

using Pkg

const DOCS_DIR = @__DIR__
const REPO_ROOT = normpath(joinpath(DOCS_DIR, "..", ".."))
const BINDING_DIR = joinpath(REPO_ROOT, "src", "bindings", "julia")
const OUTPUT_DIR = get(ENV, "CTOON_JULIA_DOCS_OUT", joinpath(DOCS_DIR, "build"))

const BUILD_JL = joinpath(BINDING_DIR, "deps", "build.jl")

Pkg.activate(BINDING_DIR)
# Must run before instantiate(): instantiate() auto-precompiles every
# package in the active project (CToon included, since it's the active
# project here), and precompiling CToon.jl means loading it — which
# errors unless deps/deps.jl already exists. See
# tests/julia/run_tests.jl's comment for the same ordering requirement.
include(BUILD_JL)
Pkg.instantiate()

Pkg.activate(DOCS_DIR)
Pkg.instantiate()

push!(LOAD_PATH, BINDING_DIR)

using Documenter
using CToon

makedocs(;
    sitename="CToon.jl",
    modules=[CToon],
    build=OUTPUT_DIR,
    format=Documenter.HTML(; prettyurls=false),
    pages=["Home" => "index.md"],
)
