# ctoon (Zig)

Zig bindings for [ctoon](https://github.com/mohammadraziei/ctoon) — a
high-performance, zero-dependency C99 library for the
[TOON](https://github.com/toon-format/spec) serialization format, with
built-in JSON interop.

This module has **no dependencies** — not even for JSON: `loadsJson`/
`dumpsJson` use ctoon's own C JSON reader/writer. Values are represented
as a small native `Value` tagged union (see `src/ctoon.zig`) — the same
dynamic, dependency-free approach the Rust binding's `Value` enum and the
Python binding's `dict`/`list` take, not a struct-mapping/reflection
layer.

```zig
const std = @import("std");
const ctoon = @import("ctoon");

test "roundtrip" {
    const gpa = std.testing.allocator;

    var fields = try std.ArrayList(ctoon.Field).initCapacity(gpa, 2);
    defer {
        for (fields.items) |*f| f.value.deinit(gpa);
        fields.deinit(gpa);
    }
    try fields.append(gpa, .{ .key = "name", .value = .{ .str = "Alice" } });
    try fields.append(gpa, .{ .key = "age", .value = .{ .uint = 30 } });

    const toon = try ctoon.dumps(gpa, .{ .object = fields });
    defer gpa.free(toon);
    // toon == "name: Alice\nage: 30"

    var back = try ctoon.loads(gpa, toon);
    defer back.deinit(gpa);
    // back.get("name").?.asStr().? == "Alice"
}
```

## How it's built

`build.zig` compiles ctoon's C core (`src/ctoon.c`) directly into a
static library and links it into a public `ctoon` module — the same
approach the Rust binding's `build.rs` and the Go binding's cgo preamble
take. `zig build test` is fully self-contained; no separate build step
or CMake invocation is required to build or test this binding on its
own.

Most of ctoon's C API is `static inline` in `ctoon.h` (meant for code
that `#include`s the header directly), which produces no
externally-linkable symbol for a compiled-and-linked FFI binding to call
against. `shim.c` — shared verbatim with the Rust binding
(`src/bindings/rust/shim.c`) — provides a thin extern-linkage wrapper for
each of those functions under a `ctoon_rs_`-prefixed name; see its doc
comment for details.

## Using it as a dependency

From a downstream `build.zig.zon`, point at this directory (or a Git
ref of it) and import the module:

```zig
const ctoon_dep = b.dependency("ctoon", .{ .target = target, .optimize = optimize });
exe.root_module.addImport("ctoon", ctoon_dep.module("ctoon"));
```

## Testing

Unit tests live alongside the source (`src/ctoon.zig`). Integration
tests live under the repository's centralized `tests/zig/` folder rather
than inside this binding's own directory, keeping every language's tests
under one root location — `build.zig`'s `test` step points there
directly, same spirit as the Rust binding's `Cargo.toml` `[[test]] path`.
Run everything with `zig build test` from this directory, or via
`ctest`/`cmake --build . --target ctoon_test_zig` from the repository's
top-level build (see `tests/zig/CMakeLists.txt`).

## Requirements

Zig 0.13 or newer.
