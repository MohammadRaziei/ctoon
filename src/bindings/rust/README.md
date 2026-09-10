# ctoon

Rust bindings for [ctoon](https://github.com/mohammadraziei/ctoon) — a
high-performance, zero-dependency C99 library for the
[TOON](https://github.com/toon-format/spec) serialization format, with
built-in JSON interop.

This crate has **no dependencies** — not even for JSON: `loads_json`/
`dumps_json` use ctoon's own C JSON reader/writer, not `serde_json`.
Values are represented as a small native [`Value`] enum (see `src/value.rs`)
— the same dynamic `dict`/`list`-shaped approach the Python binding takes,
not a derive-macro/struct-mapping layer.

```rust
let mut fields = Vec::new();
fields.push(("name".to_string(), ctoon::Value::from("Alice")));
fields.push(("age".to_string(), ctoon::Value::from(30i64)));
let doc = ctoon::Value::Object(fields);

let toon = ctoon::dumps(&doc).unwrap();
assert_eq!(toon, "name: Alice\nage: 30");

let back = ctoon::loads(&toon).unwrap();
assert_eq!(back["name"].as_str(), Some("Alice"));
```

## How it's built

`build.rs` compiles ctoon's C core (`src/ctoon.c`) directly into this
crate — the same approach the Go binding takes with its cgo preamble.
`cargo build`/`cargo test` are fully self-contained; no separate build
step or CMake invocation is required.

Most of ctoon's C API is `static inline` in `ctoon.h` (meant for code that
`#include`s the header directly), which produces no externally-linkable
symbol for a compiled-and-linked FFI binding to call. `shim.c` provides a
thin extern-linkage wrapper for each of those functions — see its doc
comment for details.

## Testing

Unit tests live alongside the source (`src/lib.rs`). Integration tests
live under the repository's centralized `tests/rust/` folder rather than
this crate's own `tests/` directory, keeping every language's tests under
one root location — `Cargo.toml`'s `[[test]] path` points there. Run
everything with `cargo test` from this directory.
