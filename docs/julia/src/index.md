# CToon.jl

Julia binding for [ctoon](https://github.com/mohammadraziei/ctoon), via
direct `ccall` into the C core — the same approach the Rust and Zig
bindings take.

## Installation

CToon.jl lives inside the main `ctoon` monorepo, under
`src/bindings/julia/`, alongside every other language's binding.
It isn't registered in Julia's General registry yet, so install it
straight from GitHub with `subdir` — the standard Pkg mechanism for
installing one package out of several that live in one git repo:

```julia
import Pkg
Pkg.add(url="https://github.com/mohammadraziei/ctoon.git", subdir="src/bindings/julia")
```

That's it — `Pkg.add` runs `deps/build.jl` for you automatically as
part of the install (it compiles `ctoon.c` plus a small C shim into a
shared library; no separate C library install, no BinaryBuilder/JLL
step, just a C compiler on the build host).

```julia
using CToon
CToon.parse("{\"a\": [1, 2, true]}")
# OrderedDict{String, Any}("a" => Any[1, 2, true])

CToon.dumps(Dict("a" => 1, "b" => [1, 2, 3]))
# "a: 1\nb[3]: 1,2,3"
```

### Developing against a local checkout

Working on the binding itself, from inside a clone of the `ctoon` repo:

```bash
cd src/bindings/julia
julia deps/build.jl
julia --project=. -e 'using Pkg; Pkg.instantiate(); include("test/runtests.jl")'
```

`Pkg.develop(path="src/bindings/julia")` also works from any other
project's environment, if you want to iterate on the binding while
using it from elsewhere.

## Scope

- `parse` / `parse_toon` — JSON and TOON reading respectively, into
  native `OrderedDict{String,Any}` / `Vector{Any}` / `String` /
  `Int64`/`UInt64`/`Float64` / `Bool` / `nothing`. `OrderedDict`
  specifically (not `Base.Dict`) so that a `parse` → `dumps`/`to_json`
  round trip preserves key order.
- `dumps` / `to_json` — the reverse: any of those same native shapes,
  encoded as TOON or JSON text.

## API Reference

```@autodocs
Modules = [CToon]
```
