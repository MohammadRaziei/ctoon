# API Reference

## Functions

| Function | Signature | Description |
|---|---|---|
| `loads` | `fn(gpa: Allocator, s: []const u8) Error!Value` | Parse a TOON string into a `Value`. |
| `dumps` | `fn(gpa: Allocator, v: Value) Error![]u8` | Serialise a `Value` to a TOON-formatted string. |
| `loadsJson` | `fn(gpa: Allocator, s: []const u8) Error!Value` | Parse a JSON string into a `Value` (using ctoon's own JSON reader). |
| `dumpsJson` | `fn(gpa: Allocator, v: Value, indent: i32) Error![]u8` | Serialise a `Value` to a JSON string. |
| `lastError` | `fn() ErrorInfo` | Details of the most recent `ParseError`/`WriteError` on this thread. |
| `specVersion` | `fn() []const u8` | The TOON spec version this build targets, e.g. `"4.1"`. |
| `specTag` | `fn() SpecTag` | The toon-format/spec tag this build targets (`major`, `minor`, `patch` as `u8`), e.g. v4.1.2. |
| `specDate` | `fn() []const u8` | Release date of the targeted spec, e.g. `"2026-07-26"`. |

## `Value`

A dependency-free tagged union:

```zig
pub const Value = union(enum) {
    null,
    boolean: bool,
    uint: u64,
    sint: i64,
    real: f64,
    str: []const u8,
    array: std.ArrayList(Value),
    object: std.ArrayList(Field),
};
```

Methods: `deinit(gpa)`, `asBool()`, `asU64()`, `asI64()`, `asF64()`,
`asStr()`, `asArray()`, `asObject()`, `get(key)`.

## `Field`

One `key: value` pair of an object, in source order:

```zig
pub const Field = struct {
    key: []const u8,
    value: Value,
};
```

## `Error`

```zig
pub const Error = error{
    ParseError,
    WriteError,
    UnsupportedValue,
    OutOfMemory,
};
```

## `ErrorInfo`

```zig
pub const ErrorInfo = struct {
    message: []const u8 = "",
    pos: usize = 0,
    code: u32 = 0,
};
```
