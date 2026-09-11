//! Integration tests for the ctoon Zig binding — public API only (the
//! module's own unit tests, in src/bindings/zig/src/ctoon.zig, additionally
//! cover internals). Lives under the top-level tests/ folder rather than
//! next to the binding, so every language's tests stay under one root —
//! same layout tests/rust, tests/go, tests/python use.

const std = @import("std");
const testing = std.testing;
const ctoon = @import("ctoon");

test "loads basic object" {
    const gpa = testing.allocator;
    var v = try ctoon.loads(gpa, "name: Alice\nage: 30");
    defer v.deinit(gpa);

    try testing.expectEqualStrings("Alice", v.get("name").?.asStr().?);
    try testing.expectEqual(@as(u64, 30), v.get("age").?.asU64().?);
}

test "dumps basic object" {
    const gpa = testing.allocator;
    var fields = try std.ArrayList(ctoon.Field).initCapacity(gpa, 2);
    defer {
        for (fields.items) |*f| { gpa.free(f.key); f.value.deinit(gpa); }
        fields.deinit();
    }
    try fields.append(.{ .key = try gpa.dupe(u8, "name"), .value = .{ .str = try gpa.dupe(u8, "Alice") } });
    try fields.append(.{ .key = try gpa.dupe(u8, "age"), .value = .{ .uint = 30 } });

    const toon = try ctoon.dumps(gpa, .{ .object = fields });
    defer gpa.free(toon);

    try testing.expectEqualStrings("name: Alice\nage: 30", toon);
}

test "roundtrip array" {
    const gpa = testing.allocator;
    var arr = try std.ArrayList(ctoon.Value).initCapacity(gpa, 3);
    defer {
        for (arr.items) |*item| item.deinit(gpa);
        arr.deinit();
    }
    try arr.append(.{ .uint = 1 });
    try arr.append(.{ .uint = 2 });
    try arr.append(.{ .uint = 3 });

    const toon = try ctoon.dumps(gpa, .{ .array = arr });
    defer gpa.free(toon);

    var back = try ctoon.loads(gpa, toon);
    defer back.deinit(gpa);

    const items = back.asArray().?;
    try testing.expectEqual(@as(usize, 3), items.len);
    try testing.expectEqual(@as(u64, 1), items[0].asU64().?);
    try testing.expectEqual(@as(u64, 2), items[1].asU64().?);
    try testing.expectEqual(@as(u64, 3), items[2].asU64().?);
}

test "roundtrip nested" {
    const gpa = testing.allocator;

    var item_a = try std.ArrayList(ctoon.Field).initCapacity(gpa, 2);
    try item_a.append(.{ .key = try gpa.dupe(u8, "sku"), .value = .{ .str = try gpa.dupe(u8, "A") } });
    try item_a.append(.{ .key = try gpa.dupe(u8, "qty"), .value = .{ .uint = 2 } });

    var item_b = try std.ArrayList(ctoon.Field).initCapacity(gpa, 2);
    try item_b.append(.{ .key = try gpa.dupe(u8, "sku"), .value = .{ .str = try gpa.dupe(u8, "B") } });
    try item_b.append(.{ .key = try gpa.dupe(u8, "qty"), .value = .{ .uint = 1 } });

    var items = try std.ArrayList(ctoon.Value).initCapacity(gpa, 2);
    try items.append(.{ .object = item_a });
    try items.append(.{ .object = item_b });

    var order = try std.ArrayList(ctoon.Field).initCapacity(gpa, 2);
    try order.append(.{ .key = try gpa.dupe(u8, "id"), .value = .{ .str = try gpa.dupe(u8, "ORD-1") } });
    try order.append(.{ .key = try gpa.dupe(u8, "items"), .value = .{ .array = items } });

    var root = try std.ArrayList(ctoon.Field).initCapacity(gpa, 1);
    defer {
        for (root.items) |*f| { gpa.free(f.key); f.value.deinit(gpa); }
        root.deinit();
    }
    try root.append(.{ .key = try gpa.dupe(u8, "order"), .value = .{ .object = order } });

    const toon = try ctoon.dumps(gpa, .{ .object = root });
    defer gpa.free(toon);

    var back = try ctoon.loads(gpa, toon);
    defer back.deinit(gpa);

    const back_items = back.get("order").?.get("items").?.asArray().?;
    try testing.expectEqualStrings("A", back_items[0].get("sku").?.asStr().?);
    try testing.expectEqual(@as(u64, 1), back_items[1].get("qty").?.asU64().?);
}

test "json to toon to json" {
    const gpa = testing.allocator;
    const json_in = "{\"x\":1,\"y\":[1,2,3],\"z\":{\"deep\":true}}";

    var value = try ctoon.loadsJson(gpa, json_in);
    defer value.deinit(gpa);

    const toon = try ctoon.dumps(gpa, value);
    defer gpa.free(toon);

    var back = try ctoon.loads(gpa, toon);
    defer back.deinit(gpa);

    const json_out = try ctoon.dumpsJson(gpa, back, 0);
    defer gpa.free(json_out);

    var reparsed = try ctoon.loadsJson(gpa, json_out);
    defer reparsed.deinit(gpa);

    try testing.expectEqual(@as(u64, 1), reparsed.get("x").?.asU64().?);
    try testing.expect(reparsed.get("z").?.get("deep").?.asBool().?);
}

test "null and bool" {
    const gpa = testing.allocator;
    var fields = try std.ArrayList(ctoon.Field).initCapacity(gpa, 3);
    defer {
        for (fields.items) |*f| { gpa.free(f.key); f.value.deinit(gpa); }
        fields.deinit();
    }
    try fields.append(.{ .key = try gpa.dupe(u8, "a"), .value = .null });
    try fields.append(.{ .key = try gpa.dupe(u8, "b"), .value = .{ .boolean = true } });
    try fields.append(.{ .key = try gpa.dupe(u8, "c"), .value = .{ .boolean = false } });

    const toon = try ctoon.dumps(gpa, .{ .object = fields });
    defer gpa.free(toon);

    var back = try ctoon.loads(gpa, toon);
    defer back.deinit(gpa);

    try testing.expectEqual(ctoon.Value.null, back.get("a").?.*);
    try testing.expect(back.get("b").?.asBool().?);
    try testing.expect(!back.get("c").?.asBool().?);
}

test "negative and float" {
    const gpa = testing.allocator;
    var fields = try std.ArrayList(ctoon.Field).initCapacity(gpa, 2);
    defer {
        for (fields.items) |*f| { gpa.free(f.key); f.value.deinit(gpa); }
        fields.deinit();
    }
    try fields.append(.{ .key = try gpa.dupe(u8, "neg"), .value = .{ .sint = -42 } });
    try fields.append(.{ .key = try gpa.dupe(u8, "pi"), .value = .{ .real = 3.14 } });

    const toon = try ctoon.dumps(gpa, .{ .object = fields });
    defer gpa.free(toon);

    var back = try ctoon.loads(gpa, toon);
    defer back.deinit(gpa);

    try testing.expectEqual(@as(i64, -42), back.get("neg").?.asI64().?);
    try testing.expectApproxEqAbs(@as(f64, 3.14), back.get("pi").?.asF64().?, 0.0001);
}

test "parse error propagates message and position" {
    const gpa = testing.allocator;
    const result = ctoon.loads(gpa, "\"unterminated");
    try testing.expectError(ctoon.Error.ParseError, result);
    try testing.expect(ctoon.lastError().message.len > 0);
}
