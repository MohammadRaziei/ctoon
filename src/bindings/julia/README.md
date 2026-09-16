# CToon.jl (skeleton)

Idiomatic Julia binding for ctoon, following the same approach as the
Rust and Zig bindings: `ccall` straight into a small shared library
built from ctoon's C core plus a shim (`shim.c`, shared verbatim with
those two) that gives the header's `static inline` functions real,
linkable symbols.

## Build & try it

No CMake, no BinaryBuilder/JLL step needed yet — `deps/build.jl`
compiles `ctoon.c` + `shim.c` into `deps/libctoon_jl.{so,dylib,dll}`
directly with `cc` (same self-contained approach as the Rust binding's
`build.rs`):

```bash
cd src/bindings/julia
julia deps/build.jl
julia --project=. -e 'using Pkg; Pkg.instantiate(); include("test/runtests.jl")'
```

```julia
using CToon
CToon.parse("{\"a\": [1, 2, true]}")
# Dict{String, Any}("a" => Any[1, 2, true])
```

## What's here

- `parse(str)` — read-only, covers `null`/`bool`/`int`/`uint`/`real`/
  `string`/array/object, converting into native `Dict{String,Any}` /
  `Vector{Any}` / etc.

## What's intentionally left out (skeleton scope)

- **Writing / mutable documents** — `shim.c` already exports the
  `ctoon_mut_*` wrappers needed for this; `CToon.jl` just doesn't call
  them yet.
- **Packaging as a JLL** — once the API shape here is settled,
  BinaryBuilder + a `CToon_jll` package is the natural next step so
  users don't need a C compiler at all; skipped for now to keep this
  skeleton buildable everywhere with just `cc`.
- **`ctoon_obj_iter` as a real Julia struct** — right now it's read
  through a raw scratch buffer sized generously to hold the C struct.
  Mirroring its actual field layout properly is a follow-up.
- Real UUID in `Project.toml` — the one there is a placeholder.
