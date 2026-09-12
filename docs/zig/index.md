# Zig Binding

Zig bindings for ctoon — a high-performance, zero-dependency C99
library for the TOON serialization format, with built-in JSON interop.

The Zig binding compiles ctoon's C core directly into itself via
`build.zig` — no separate C library install, no CMake step required to
build or test it on its own.

Values are represented as `Value`, a small, dependency-free tagged
union — the same dynamic-data approach the Rust binding's `Value` enum
and the Python binding's `dict`/`list` take, not a struct-mapping or
reflection layer. JSON parsing and writing is done by ctoon's own C
implementation (`loadsJson`/`dumpsJson`), so this module has no JSON
library dependency of its own.

## Requirements

Zig 0.16.0 or newer.
