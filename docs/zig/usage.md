# Usage

## Encode and decode

```zig
const std = @import("std");
const ctoon = @import("ctoon");

test "roundtrip" {
    const gpa = std.testing.allocator;

    var fields = try std.ArrayList(ctoon.Field).initCapacity(gpa, 2);
    defer {
        for (fields.items) |*f| { gpa.free(f.key); f.value.deinit(gpa); }
        fields.deinit(gpa);
    }
    try fields.append(gpa, .{ .key = try gpa.dupe(u8, "name"), .value = .{ .str = try gpa.dupe(u8, "Alice") } });
    try fields.append(gpa, .{ .key = try gpa.dupe(u8, "age"), .value = .{ .uint = 30 } });

    const toon = try ctoon.dumps(gpa, .{ .object = fields });
    defer gpa.free(toon);
    // toon == "name: Alice\nage: 30"

    var back = try ctoon.loads(gpa, toon);
    defer back.deinit(gpa);
    // back.get("name").?.asStr().? == "Alice"
}
```

## JSON interop

```zig
var value = try ctoon.loadsJson(gpa, json_text);
defer value.deinit(gpa);

const toon = try ctoon.dumps(gpa, value);
defer gpa.free(toon);

const json_out = try ctoon.dumpsJson(gpa, value, 2); // indent = 2
defer gpa.free(json_out);
```

## Memory ownership

Every `Value` owns its string memory: `.str` payloads and object keys
are freed by `deinit`. Build `Value`s with `gpa.dupe`, not string
literals, if you intend to call `deinit` on the result — values
returned by `loads`/`loadsJson` are always heap-owned this way already.

## Error handling

On `Error.ParseError` or `Error.WriteError`, call `ctoon.lastError()`
for the message, byte position, and error code from the most recent
failure:

```zig
const result = ctoon.loads(gpa, "\"unterminated");
if (result) |_| {} else |_| {
    const err = ctoon.lastError();
    std.debug.print("{s} at byte {}\n", .{ err.message, err.pos });
}
```
