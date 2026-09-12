# Installation

`build.zig`/`build.zig.zon` live at the repository root (like `Cargo.toml`
and `pyproject.toml`), so no extra path setup is needed once you have the
repo — the binding's own source stays under `src/bindings/zig/`.

## As a dependency

From a downstream `build.zig.zon`, point at the ctoon repository (or a Git
ref of it):

```bash
zig fetch --save git+https://github.com/mohammadraziei/ctoon.git
```

Then import the module in your `build.zig`:

```zig
const ctoon_dep = b.dependency("ctoon", .{ .target = target, .optimize = optimize });
exe.root_module.addImport("ctoon", ctoon_dep.module("ctoon"));
```

## Building from source

```bash
git clone https://github.com/mohammadraziei/ctoon.git
cd ctoon
zig build test    # run the binding's own tests
zig build         # install the static library + headers under zig-out/
```

## Requirements

Zig 0.16.0 or newer. No other dependencies — the C core (`src/ctoon.c`)
is compiled directly into the binding.
