# Compiles the C core + shim directly into a shared library, the same
# self-contained approach the Rust binding's build.rs takes (no CMake
# step required). This keeps `]build CToon` / first `using CToon` fully
# automatic for the user — no separate libctoon install, no BinaryBuilder
# JLL dependency (that's the natural next step once this binding is
# past the skeleton stage, but it's not needed to get a working package
# today).
#
# Where src/ctoon.c and include/ctoon.h come from — local first, GitHub
# as the fallback:
#
#   1. If this file is running from inside a full checkout of the ctoon
#      repo (developing the binding directly, or this repo's own CMake
#      benchmark suite, which always builds from a real local clone),
#      ../../../src/ctoon.c and ../../../include/ctoon.h already exist
#      on disk right next to it. Use those directly — no network, no
#      GitHub call, nothing to go stale.
#
#   2. Otherwise (this package was installed the normal Julia way, via
#      `Pkg.add(url=..., subdir="src/bindings/julia")` — which checks
#      out ONLY that subdirectory, so step 1's files genuinely don't
#      exist), fetch them from GitHub instead, trying these refs in
#      order until one actually has the files:
#        a. CTOON_JULIA_CORE_REF env var, if set — any git ref (branch,
#           tag, commit). Set to "master" by this repo's own CMake
#           benchmark suite, since a benchmark run should always
#           measure current master, not the last tagged release.
#        b. otherwise, "v$(this package's own Project.toml version)" —
#           NOT a GitHub API "latest release" lookup: that's an extra
#           network round-trip against a tightly rate-limited endpoint
#           for something we already know. version.py (repo root) keeps
#           Project.toml's version in sync with include/ctoon.h's
#           #define CTOON_VERSION_* on every release, the same way it
#           already does for Cargo.toml and build.zig.zon — so this
#           package's own version IS the release to fetch, no lookup
#           needed, and raw.githubusercontent.com (a CDN, not the REST
#           API) doesn't hit the same rate limit.
#        c. "master", as a last resort — if (a)/(b) 404s (version.py
#           wasn't run before this was published, or a stale ref), the
#           build still succeeds rather than hard-failing.
#      Fetched files are cached under deps/downloaded/.

using Downloads

const CTOON_REPO = "mohammadraziei/ctoon"

binding_dir = @__DIR__ |> dirname                            # .../src/bindings/julia
local_repo_root = normpath(joinpath(binding_dir, "..", "..", ".."))
local_ctoon_c = joinpath(local_repo_root, "src", "ctoon.c")
local_ctoon_h = joinpath(local_repo_root, "include", "ctoon.h")

function own_package_version()
    project_toml = joinpath(binding_dir, "Project.toml")
    text = read(project_toml, String)
    m = match(r"(?m)^version\s*=\s*\"([^\"]+)\"", text)
    m === nothing && error("no \"version = ...\" found in $project_toml")
    return m.captures[1]
end

function fetch_core_file(ref::AbstractString, repo_path::AbstractString, dest::AbstractString)
    url = "https://raw.githubusercontent.com/$CTOON_REPO/$ref/$repo_path"
    mkpath(dirname(dest))
    Downloads.download(url, dest)  # throws on any non-2xx status
    return dest
end

function fetch_core_at(ref::AbstractString, cache_dir::AbstractString)
    ctoon_c = fetch_core_file(ref, "src/ctoon.c", joinpath(cache_dir, "ctoon.c"))
    fetch_core_file(ref, "include/ctoon.h", joinpath(cache_dir, "ctoon.h"))
    return ctoon_c
end

function fetch_core_with_fallback(cache_dir::AbstractString)
    env_ref = get(ENV, "CTOON_JULIA_CORE_REF", "")
    candidates = isempty(env_ref) ? ["v" * own_package_version(), "master"] : [env_ref, "master"]
    unique!(candidates)

    for (i, ref) in enumerate(candidates)
        try
            @info (i == 1 ? "ctoon: fetching core source" : "ctoon: falling back to") ref
            ctoon_c = fetch_core_at(ref, cache_dir)
            return ref, ctoon_c
        catch e
            @warn "ctoon: fetching core source at ref '$ref' failed" exception = e
            i == lastindex(candidates) && rethrow()
        end
    end
end

ref = "local checkout"
if isfile(local_ctoon_c) && isfile(local_ctoon_h)
    @info "ctoon: found src/ctoon.c and include/ctoon.h in a local checkout — using those, no network needed" local_repo_root
    ctoon_c = local_ctoon_c
    include_dir = dirname(local_ctoon_h)
else
    cache_dir = joinpath(@__DIR__, "downloaded")
    ref, ctoon_c = fetch_core_with_fallback(cache_dir)
    include_dir = cache_dir
end

shim_c = joinpath(binding_dir, "shim.c")

outdir = @__DIR__
libname = Sys.iswindows() ? "libctoon_jl.dll" : (Sys.isapple() ? "libctoon_jl.dylib" : "libctoon_jl.so")
libpath = joinpath(outdir, libname)

cc = get(ENV, "CC", Sys.iswindows() ? "cc" : "cc")

cmd = `$cc -O3 -fPIC -shared -DCTOON_ENABLE_JSON=1 -I$include_dir $ctoon_c $shim_c -o $libpath`

@info "Building libctoon for Julia" cmd ref
run(cmd)

open(joinpath(outdir, "deps.jl"), "w") do io
    println(io, "const libctoon_jl = \"$(libpath)\"")
    println(io, "const libctoon_jl_core_ref = \"$(ref)\"  # for `CToon.core_ref()` / bug reports")
end
