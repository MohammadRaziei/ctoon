# CToon.jl

Julia binding for [ctoon](https://github.com/mohammadraziei/ctoon), via
direct `ccall` into the C core — the same approach the Rust and Zig
bindings take.

## Installation

```bash
cd src/bindings/julia
julia deps/build.jl
```

## API Reference

```@autodocs
Modules = [CToon]
```
