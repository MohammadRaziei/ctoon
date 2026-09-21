# CToon.jl

Idiomatic Julia binding for ctoon, following the same approach as the
Rust and Zig bindings: `ccall` straight into a small shared library
built from ctoon's C core plus a shim (`shim.c`, shared verbatim with
those two) that gives the header's `static inline` functions real,
linkable symbols.

## Install as a dependency

Not registered in Julia's General registry yet. Two ways to depend on
it, both installing straight from GitHub with `subdir` (the standard
Pkg way to install one package out of several living in one git repo):

```julia
# One-off / interactive install:
import Pkg
Pkg.add(url="https://github.com/mohammadraziei/ctoon.git", subdir="src/bindings/julia")
```

```toml
# Or declared in your own project's Project.toml (what this repo's own
# benchmark suite does — see benchmarks/julia/Project.toml):
[deps]
CToon = "7c8e6b1a-0c1e-4a9e-9c3e-c1000ffee001"

[sources]
CToon = {url = "https://github.com/mohammadraziei/ctoon.git", rev = "master", subdir = "src/bindings/julia"}
```

Either way, Pkg runs `deps/build.jl` for you automatically as part of
the install — no separate C library install, no BinaryBuilder/JLL
step, just a C compiler on the build host. `deps/build.jl` fetches the
matching version of ctoon's C core (`src/ctoon.c`, `include/ctoon.h`)
from GitHub at build time if it isn't sitting right next to it in a
full local checkout — see that file's own comment for exactly which
ref it picks and how to override it (`CTOON_JULIA_CORE_REF`).

```julia
using CToon
CToon.parse("{\"a\": [1, 2, true]}")
# OrderedDict{String, Any}("a" => Any[1, 2, true])

CToon.dumps(Dict("a" => 1, "b" => [1, 2, 3]))
# "a: 1\nb[3]: 1,2,3"

CToon.to_json(CToon.parse_toon("a: 1\nb[3]: 1,2,3"))
# "{\n  \"a\": 1,\n  \"b\": [\n    1,\n    2,\n    3\n  ]\n}"
```

Iterating on the binding itself while using it from another project's
environment: `Pkg.develop(path="/path/to/ctoon/src/bindings/julia")`.

## Developing this binding directly

No CMake, no BinaryBuilder/JLL step needed yet — `deps/build.jl`
compiles `ctoon.c` + `shim.c` into `deps/libctoon_jl.{so,dylib,dll}`
directly with `cc` (same self-contained approach as the Rust binding's
`build.rs`):

```bash
cd src/bindings/julia
julia deps/build.jl
julia --project=. -e 'using Pkg; Pkg.instantiate(); include("test/runtests.jl")'
```

## What's here

- `parse(str)` — parse JSON text (via `ctoon_read_json`) into native
  Julia data: `OrderedDict{String,Any}` for objects (order-preserving
  — see below), `Vector{Any}` for arrays, `String`, `Int64`/`UInt64`/
  `Float64`, `Bool`, or `nothing`.
- `parse_toon(str)` — same, but for ctoon's native TOON syntax (via
  `ctoon_read_opts`) rather than JSON.
- `dumps(value)` — encode a native Julia value (the same shapes
  `parse`/`parse_toon` produce) as TOON text.
- `to_json(value)` — same as `dumps`, but as JSON text.

Objects parse into `OrderedDict` (from OrderedCollections.jl), not
`Base.Dict`, specifically so that `parse(x) |> dumps` / `parse(x) |>
to_json` round trips preserve key order — `Base.Dict` does not
preserve insertion order on iteration, which would silently scramble
key order on every round trip through this binding otherwise, unlike
every other language binding in this repo.

## What's intentionally left out

- **Packaging as a JLL** — once the API shape here is settled,
  BinaryBuilder + a `CToon_jll` package is the natural next step so
  users don't need a C compiler at all; skipped for now to keep this
  buildable everywhere with just `cc`.
- **`ctoon_obj_iter` as a real Julia struct** — right now it's read
  through a raw scratch buffer sized generously to hold the C struct.
  Mirroring its actual field layout properly is a follow-up.
- Real UUID in `Project.toml` — the one there is a placeholder.
