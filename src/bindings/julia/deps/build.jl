# Compiles the C core + shim directly into a shared library, the same
# self-contained approach the Rust binding's build.rs takes (no CMake
# step required). This keeps `]build CToon` / first `using CToon` fully
# automatic for the user — no separate libctoon install, no BinaryBuilder
# JLL dependency (that's the natural next step once this binding is
# past the skeleton stage, but it's not needed to get a working package
# today).

binding_dir = @__DIR__ |> dirname          # .../src/bindings/julia
repo_root = joinpath(binding_dir, "..", "..", "..") |> normpath

include_dir = joinpath(repo_root, "include")
src_dir = joinpath(repo_root, "src")
ctoon_c = joinpath(src_dir, "ctoon.c")
shim_c = joinpath(binding_dir, "shim.c")

outdir = @__DIR__
libname = Sys.iswindows() ? "libctoon_jl.dll" : (Sys.isapple() ? "libctoon_jl.dylib" : "libctoon_jl.so")
libpath = joinpath(outdir, libname)

cc = get(ENV, "CC", Sys.iswindows() ? "cc" : "cc")

cmd = `$cc -O3 -fPIC -shared -DCTOON_ENABLE_JSON=1 -I$include_dir -I$src_dir $ctoon_c $shim_c -o $libpath`

@info "Building libctoon for Julia" cmd
run(cmd)

open(joinpath(outdir, "deps.jl"), "w") do io
    println(io, "const libctoon_jl = \"$(libpath)\"")
end
