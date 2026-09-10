//! Zig bindings for [ctoon](https://github.com/mohammadraziei/ctoon) — a
//! high-performance, zero-dependency C99 library for the
//! [TOON](https://github.com/toon-format/spec) serialization format, with
//! built-in JSON interop.
//!
//! Values are represented as `Value` — a small, dependency-free tagged
//! union (below), not a struct-mapping/reflection layer. This mirrors the
//! Rust binding's own approach (`Value` enum, no `serde_json`) and the
//! Python binding's (`dict`/`list`). JSON parsing and writing is done by
//! ctoon's own C implementation (`loadsJson`/`dumpsJson`) — this module
//! has no JSON (or any other) dependency at all.
//!
//! ```zig
//! const std = @import("std");
//! const ctoon = @import("ctoon");
//!
//! test "roundtrip" {
//!     const gpa = std.testing.allocator;
//!     var obj = ctoon.Value{ .object = std.ArrayList(ctoon.Field).init(gpa) };
//!     defer obj.deinit(gpa);
//!     try obj.object.append(.{ .key = "name", .value = .{ .str = "Alice" } });
//!
//!     const toon = try ctoon.dumps(gpa, obj);
//!     defer gpa.free(toon);
//!
//!     var back = try ctoon.loads(gpa, toon);
//!     defer back.deinit(gpa);
//!     try std.testing.expectEqualStrings("Alice", back.get("name").?.str);
//! }
//! ```

const std = @import("std");
const Allocator = std.mem.Allocator;

// ── Raw FFI declarations ───────────────────────────────────────────────
// Not part of the public interface of this module — see `Value`, `loads`,
// `dumps` etc. below for the safe wrapper. Most of ctoon's tree-inspection
// and mutable-document-building API is `static inline` in ctoon.h (meant
// for code that #includes the header directly), so it produces no
// externally-linkable symbol in the compiled ctoon.c object. Those
// functions are declared here under their `ctoon_rs_`-prefixed shim names
// instead — see shim.c (shared with the Rust binding), which provides a
// thin extern-linkage wrapper for each one. The handful of functions that
// genuinely are `extern` in ctoon.h are declared here under their real
// names, unshimmed.
const c = struct {
    pub const doc = opaque {};
    pub const val = opaque {};
    pub const mut_doc = opaque {};
    pub const mut_val = opaque {};
    pub const alc = opaque {};

    pub const ctoon_type = u8;
    pub const read_flag = u32;
    pub const write_flag = u32;

    pub const TYPE_NULL: ctoon_type = 2;
    pub const TYPE_BOOL: ctoon_type = 3;
    pub const TYPE_NUM: ctoon_type = 4;
    pub const TYPE_STR: ctoon_type = 5;
    pub const TYPE_ARR: ctoon_type = 6;
    pub const TYPE_OBJ: ctoon_type = 7;

    pub const READ_NOFLAG: read_flag = 0;
    pub const WRITE_NOFLAG: write_flag = 0;

    pub const read_err = extern struct {
        code: u32,
        msg: ?[*:0]const u8,
        pos: usize,
    };

    pub const write_err = extern struct {
        code: u32,
        msg: ?[*:0]const u8,
    };

    pub const obj_iter = extern struct {
        idx: usize,
        max: usize,
        cur: ?*val,
        obj: ?*val,
    };

    // Genuinely `extern` in ctoon.h — real names, no shim needed.
    pub extern fn ctoon_read_opts(dat: [*]u8, len: usize, flg: read_flag, alc_: ?*const alc, err: *read_err) ?*doc;
    pub extern fn ctoon_read_json(dat: [*]u8, len: usize, flg: read_flag, alc_: ?*const alc, err: *read_err) ?*doc;
    pub extern fn ctoon_mut_doc_new(alc_: ?*const alc) ?*mut_doc;
    pub extern fn ctoon_mut_doc_free(doc_: ?*mut_doc) void;
    pub extern fn ctoon_mut_write_opts(doc_: ?*const mut_doc, opts: ?*const anyopaque, alc_: ?*const alc, len: *usize, err: *write_err) ?[*]u8;
    pub extern fn ctoon_write_json_mut(doc_: ?*const mut_doc, indent: c_int, flg: write_flag, alc_: ?*const alc, len: *usize, err: *write_err) ?[*]u8;

    // `static inline` in ctoon.h — shimmed via shim.c.
    pub extern fn ctoon_rs_doc_get_root(doc_: ?*doc) ?*val;
    pub extern fn ctoon_rs_doc_free(doc_: ?*doc) void;

    pub extern fn ctoon_rs_get_type(v: *val) ctoon_type;
    pub extern fn ctoon_rs_get_bool(v: *val) bool;
    pub extern fn ctoon_rs_is_uint(v: *val) bool;
    pub extern fn ctoon_rs_is_sint(v: *val) bool;
    pub extern fn ctoon_rs_get_uint(v: *val) u64;
    pub extern fn ctoon_rs_get_sint(v: *val) i64;
    pub extern fn ctoon_rs_get_real(v: *val) f64;
    pub extern fn ctoon_rs_get_str(v: *val) ?[*]const u8;
    pub extern fn ctoon_rs_get_len(v: *val) usize;
    pub extern fn ctoon_rs_get_raw(v: *val) ?[*:0]const u8;

    pub extern fn ctoon_rs_arr_size(arr: *val) usize;
    pub extern fn ctoon_rs_arr_get(arr: *val, idx: usize) ?*val;

    pub extern fn ctoon_rs_obj_size(obj: *val) usize;
    pub extern fn ctoon_rs_obj_iter_init(obj: *val, iter: *obj_iter) bool;
    pub extern fn ctoon_rs_obj_iter_has_next(iter: *const obj_iter) bool;
    pub extern fn ctoon_rs_obj_iter_next(iter: *obj_iter) ?*val;
    pub extern fn ctoon_rs_obj_iter_get_val(key_val: *val) ?*val;

    pub extern fn ctoon_rs_mut_doc_set_root(doc_: *mut_doc, root: *mut_val) void;
    pub extern fn ctoon_rs_mut_null(doc_: *mut_doc) ?*mut_val;
    pub extern fn ctoon_rs_mut_true(doc_: *mut_doc) ?*mut_val;
    pub extern fn ctoon_rs_mut_false(doc_: *mut_doc) ?*mut_val;
    pub extern fn ctoon_rs_mut_sint(doc_: *mut_doc, v: i64) ?*mut_val;
    pub extern fn ctoon_rs_mut_uint(doc_: *mut_doc, v: u64) ?*mut_val;
    pub extern fn ctoon_rs_mut_real(doc_: *mut_doc, v: f64) ?*mut_val;
    pub extern fn ctoon_rs_mut_strncpy(doc_: *mut_doc, str_: [*]const u8, len: usize) ?*mut_val;
    pub extern fn ctoon_rs_mut_obj(doc_: *mut_doc) ?*mut_val;
    pub extern fn ctoon_rs_mut_arr(doc_: *mut_doc) ?*mut_val;
    pub extern fn ctoon_rs_mut_obj_put(obj: *mut_val, key: *mut_val, v: *mut_val) bool;
    pub extern fn ctoon_rs_mut_arr_append(arr: *mut_val, v: *mut_val) bool;

    pub extern fn free(ptr: ?*anyopaque) void;
};

// ── Safe public API ────────────────────────────────────────────────────

/// Error set returned by every fallible function in this module.
pub const Error = error{
    ParseError,
    WriteError,
    UnsupportedValue,
    OutOfMemory,
};

/// Details of the most recent `ParseError`/`WriteError`, valid until the
/// next call into this module on the same thread. Mirrors the
/// Rust binding's `Error::Parse`/`Error::Write` variants, which carry
/// the same fields, without needing a payload-carrying Zig error set.
pub const ErrorInfo = struct {
    message: []const u8 = "",
    pos: usize = 0,
    code: u32 = 0,
};
threadlocal var last_error: ErrorInfo = .{};

/// Returns details for the most recent error returned by this module.
pub fn lastError() ErrorInfo {
    return last_error;
}

/// One `key: value` pair of an object, in source order — objects are
/// `[]Field`, not a hash map, so field order round-trips exactly as read.
/// TOON documents are usually human-authored, and reordering fields
/// alphabetically would be surprising.
pub const Field = struct {
    key: []const u8,
    value: Value,
};

/// A dependency-free dynamic value — no JSON library required for the
/// core API, since ctoon does its own JSON parsing/writing in C.
pub const Value = union(enum) {
    null,
    boolean: bool,
    /// A non-negative integer, as read back from TOON/JSON text with no
    /// minus sign — see the module docs for why this is a distinct
    /// variant from `sint` rather than one signed integer type. Same
    /// split the Rust binding's `Value::Uint`/`Value::Sint` make.
    uint: u64,
    sint: i64,
    real: f64,
    str: []const u8,
    array: std.ArrayList(Value),
    object: std.ArrayList(Field),

    pub fn deinit(self: *Value, gpa: Allocator) void {
        switch (self.*) {
            .array => |*a| {
                for (a.items) |*item| item.deinit(gpa);
                a.deinit(gpa);
            },
            .object => |*o| {
                for (o.items) |*field| field.value.deinit(gpa);
                o.deinit(gpa);
            },
            else => {},
        }
    }

    pub fn asBool(self: Value) ?bool {
        return switch (self) {
            .boolean => |b| b,
            else => null,
        };
    }

    pub fn asU64(self: Value) ?u64 {
        return switch (self) {
            .uint => |n| n,
            .sint => |n| if (n >= 0) @as(u64, @intCast(n)) else null,
            else => null,
        };
    }

    pub fn asI64(self: Value) ?i64 {
        return switch (self) {
            .sint => |n| n,
            .uint => |n| if (n <= std.math.maxInt(i64)) @as(i64, @intCast(n)) else null,
            else => null,
        };
    }

    pub fn asF64(self: Value) ?f64 {
        return switch (self) {
            .real => |n| n,
            .uint => |n| @floatFromInt(n),
            .sint => |n| @floatFromInt(n),
            else => null,
        };
    }

    pub fn asStr(self: Value) ?[]const u8 {
        return switch (self) {
            .str => |s| s,
            else => null,
        };
    }

    pub fn asArray(self: Value) ?[]const Value {
        return switch (self) {
            .array => |a| a.items,
            else => null,
        };
    }

    pub fn asObject(self: Value) ?[]const Field {
        return switch (self) {
            .object => |o| o.items,
            else => null,
        };
    }

    /// Looks up a key in an object. Returns `null` for a non-object value
    /// or a missing key — same shape as the Rust binding's `Value::get`.
    pub fn get(self: Value, key: []const u8) ?*const Value {
        switch (self) {
            .object => |o| {
                for (o.items) |*field| {
                    if (std.mem.eql(u8, field.key, key)) return &field.value;
                }
                return null;
            },
            else => return null,
        }
    }
};

fn setErrorFromRead(err: c.read_err) void {
    last_error = .{
        .message = if (err.msg) |m| std.mem.span(m) else "parse error",
        .pos = err.pos,
        .code = err.code,
    };
}

fn setErrorFromWrite(err: c.write_err) void {
    last_error = .{
        .message = if (err.msg) |m| std.mem.span(m) else "write error",
        .code = err.code,
    };
}

/// Walks a `ctoon_val` tree into a `Value`, allocating with `gpa`.
/// `v` must be either null or a valid pointer returned by ctoon (e.g.
/// from `ctoon_doc_get_root`, `ctoon_arr_get`, `ctoon_obj_iter_get_val`),
/// and the document it belongs to must still be alive.
fn valToValue(gpa: Allocator, v: ?*c.val) Error!Value {
    const ptr = v orelse return .null;
    return switch (c.ctoon_rs_get_type(ptr)) {
        c.TYPE_NULL => .null,
        c.TYPE_BOOL => .{ .boolean = c.ctoon_rs_get_bool(ptr) },
        c.TYPE_NUM => if (c.ctoon_rs_is_uint(ptr))
            Value{ .uint = c.ctoon_rs_get_uint(ptr) }
        else if (c.ctoon_rs_is_sint(ptr))
            Value{ .sint = c.ctoon_rs_get_sint(ptr) }
        else
            Value{ .real = c.ctoon_rs_get_real(ptr) },
        c.TYPE_STR => .{ .str = try getStr(gpa, ptr) },
        c.TYPE_ARR => blk: {
            const n = c.ctoon_rs_arr_size(ptr);
            var arr = try std.ArrayList(Value).initCapacity(gpa, n);
            errdefer {
                for (arr.items) |*item| item.deinit(gpa);
                arr.deinit(gpa);
            }
            var i: usize = 0;
            while (i < n) : (i += 1) {
                try arr.append(gpa, try valToValue(gpa, c.ctoon_rs_arr_get(ptr, i)));
            }
            break :blk .{ .array = arr };
        },
        c.TYPE_OBJ => blk: {
            const n = c.ctoon_rs_obj_size(ptr);
            var obj = try std.ArrayList(Field).initCapacity(gpa, n);
            errdefer {
                for (obj.items) |*field| field.value.deinit(gpa);
                obj.deinit(gpa);
            }
            var iter: c.obj_iter = undefined;
            _ = c.ctoon_rs_obj_iter_init(ptr, &iter);
            while (c.ctoon_rs_obj_iter_has_next(&iter)) {
                const key_val = c.ctoon_rs_obj_iter_next(&iter) orelse break;
                const field_val = try valToValue(gpa, c.ctoon_rs_obj_iter_get_val(key_val));
                try obj.append(gpa, .{ .key = try getStr(gpa, key_val), .value = field_val });
            }
            break :blk .{ .object = obj };
        },
        else => blk: {
            // raw/unknown value kind — best-effort string, matches the
            // fallback the C API's other consumers (Go, Rust) use.
            const raw = c.ctoon_rs_get_raw(ptr) orelse break :blk Value.null;
            break :blk .{ .str = try gpa.dupe(u8, std.mem.span(raw)) };
        },
    };
}

/// `v` must be a valid, non-null pointer to a `CTOON_TYPE_STR` value
/// whose underlying document is still alive.
fn getStr(gpa: Allocator, v: *c.val) Error![]const u8 {
    const ptr = c.ctoon_rs_get_str(v) orelse return "";
    const len = c.ctoon_rs_get_len(v);
    if (len == 0) return "";
    return gpa.dupe(u8, ptr[0..len]) catch return Error.OutOfMemory;
}

/// Walks a `Value` into a `ctoon_mut_val` tree. `doc` must be a valid,
/// non-null `ctoon_mut_doc` the caller owns.
fn valueToMut(doc: *c.mut_doc, v: Value) Error!*c.mut_val {
    const ptr: ?*c.mut_val = switch (v) {
        .null => c.ctoon_rs_mut_null(doc),
        .boolean => |b| if (b) c.ctoon_rs_mut_true(doc) else c.ctoon_rs_mut_false(doc),
        .uint => |n| c.ctoon_rs_mut_uint(doc, n),
        .sint => |n| c.ctoon_rs_mut_sint(doc, n),
        .real => |n| c.ctoon_rs_mut_real(doc, n),
        .str => |s| c.ctoon_rs_mut_strncpy(doc, s.ptr, s.len),
        .array => |arr| blk: {
            const a = c.ctoon_rs_mut_arr(doc) orelse break :blk null;
            for (arr.items) |elem| {
                const child = try valueToMut(doc, elem);
                if (!c.ctoon_rs_mut_arr_append(a, child)) return Error.UnsupportedValue;
            }
            break :blk a;
        },
        .object => |fields| blk: {
            const o = c.ctoon_rs_mut_obj(doc) orelse break :blk null;
            for (fields.items) |field| {
                const key = c.ctoon_rs_mut_strncpy(doc, field.key.ptr, field.key.len) orelse
                    return Error.UnsupportedValue;
                const child = try valueToMut(doc, field.value);
                if (!c.ctoon_rs_mut_obj_put(o, key, child)) return Error.UnsupportedValue;
            }
            break :blk o;
        },
    };
    return ptr orelse Error.UnsupportedValue;
}

/// Parses a TOON string into a `Value`, allocated with `gpa`. Call
/// `.deinit(gpa)` on the result when done. On `Error.ParseError`, call
/// `lastError()` for the message/position/code.
pub fn loads(gpa: Allocator, s: []const u8) Error!Value {
    // Always pass an owned, mutable copy — ctoon_read_opts takes a
    // non-const `char *` and, while it does not modify the buffer unless
    // the (never-set, here) in-situ flag is passed, that's an internal
    // behaviour this module doesn't want to depend on for soundness.
    const buf = try gpa.dupe(u8, s);
    defer gpa.free(buf);

    var err: c.read_err = std.mem.zeroes(c.read_err);
    const doc = c.ctoon_read_opts(buf.ptr, buf.len, c.READ_NOFLAG, null, &err) orelse {
        setErrorFromRead(err);
        return Error.ParseError;
    };
    defer c.ctoon_rs_doc_free(doc);
    return valToValue(gpa, c.ctoon_rs_doc_get_root(doc));
}

/// Serialises a `Value` to a TOON-formatted string, allocated with `gpa`.
/// Free the result with `gpa.free(...)`.
pub fn dumps(gpa: Allocator, v: Value) Error![]u8 {
    const doc = c.ctoon_mut_doc_new(null) orelse return Error.WriteError;
    defer c.ctoon_mut_doc_free(doc);

    const root = try valueToMut(doc, v);
    c.ctoon_rs_mut_doc_set_root(doc, root);

    var len: usize = 0;
    var err: c.write_err = std.mem.zeroes(c.write_err);
    const raw = c.ctoon_mut_write_opts(doc, null, null, &len, &err) orelse {
        setErrorFromWrite(err);
        return Error.WriteError;
    };
    defer c.free(raw);
    return gpa.dupe(u8, raw[0..len]);
}

/// Parses a JSON string into a `Value` (using ctoon's own JSON reader —
/// this module has no JSON-parsing dependency of its own).
pub fn loadsJson(gpa: Allocator, s: []const u8) Error!Value {
    const buf = try gpa.dupe(u8, s);
    defer gpa.free(buf);

    var err: c.read_err = std.mem.zeroes(c.read_err);
    const doc = c.ctoon_read_json(buf.ptr, buf.len, c.READ_NOFLAG, null, &err) orelse {
        setErrorFromRead(err);
        return Error.ParseError;
    };
    defer c.ctoon_rs_doc_free(doc);
    return valToValue(gpa, c.ctoon_rs_doc_get_root(doc));
}

/// Serialises a `Value` to a JSON string using ctoon's own JSON writer.
pub fn dumpsJson(gpa: Allocator, v: Value, indent: i32) Error![]u8 {
    const doc = c.ctoon_mut_doc_new(null) orelse return Error.WriteError;
    defer c.ctoon_mut_doc_free(doc);

    const root = try valueToMut(doc, v);
    c.ctoon_rs_mut_doc_set_root(doc, root);

    var len: usize = 0;
    var err: c.write_err = std.mem.zeroes(c.write_err);
    const raw = c.ctoon_write_json_mut(doc, indent, c.WRITE_NOFLAG, null, &len, &err) orelse {
        setErrorFromWrite(err);
        return Error.WriteError;
    };
    defer c.free(raw);
    return gpa.dupe(u8, raw[0..len]);
}

// ── Unit tests ─────────────────────────────────────────────────────────

test "roundtrip basic" {
    const gpa = std.testing.allocator;

    var fields = try std.ArrayList(Field).initCapacity(gpa, 3);
    defer {
        for (fields.items) |*f| f.value.deinit(gpa);
        fields.deinit(gpa);
    }
    try fields.append(gpa, .{ .key = "name", .value = .{ .str = "Alice" } });
    try fields.append(gpa, .{ .key = "age", .value = .{ .sint = 30 } });

    var tags = try std.ArrayList(Value).initCapacity(gpa, 2);
    try tags.append(gpa, .{ .str = "a" });
    try tags.append(gpa, .{ .str = "b" });
    try fields.append(gpa, .{ .key = "tags", .value = .{ .array = tags } });

    const toon = try dumps(gpa, .{ .object = fields });
    defer gpa.free(toon);

    var back = try loads(gpa, toon);
    defer back.deinit(gpa);

    try std.testing.expectEqualStrings("Alice", back.get("name").?.asStr().?);
    // Non-negative integer literals read back as `.uint` regardless of
    // what variant they were written with — TOON text has no type tag
    // distinguishing signed/unsigned. Same behaviour the Rust binding's
    // roundtrip_basic test documents.
    try std.testing.expectEqual(@as(u64, 30), back.get("age").?.asU64().?);
}

test "json interop" {
    const gpa = std.testing.allocator;

    var arr = try std.ArrayList(Value).initCapacity(gpa, 3);
    try arr.append(gpa, .{ .sint = 1 });
    try arr.append(gpa, .{ .sint = 2 });
    try arr.append(gpa, .{ .sint = 3 });

    var fields = try std.ArrayList(Field).initCapacity(gpa, 2);
    defer {
        for (fields.items) |*f| f.value.deinit(gpa);
        fields.deinit(gpa);
    }
    try fields.append(gpa, .{ .key = "x", .value = .{ .sint = 1 } });
    try fields.append(gpa, .{ .key = "y", .value = .{ .array = arr } });

    const j = try dumpsJson(gpa, .{ .object = fields }, 2);
    defer gpa.free(j);

    var back = try loadsJson(gpa, j);
    defer back.deinit(gpa);

    try std.testing.expectEqual(@as(u64, 1), back.get("x").?.asU64().?);
    try std.testing.expectEqual(@as(usize, 3), back.get("y").?.asArray().?.len);
}

test "parse error has message" {
    const gpa = std.testing.allocator;
    // Unterminated quoted string.
    const result = loads(gpa, "\"unterminated");
    try std.testing.expectError(Error.ParseError, result);
    try std.testing.expect(lastError().message.len > 0);
}

test "empty string value" {
    const gpa = std.testing.allocator;

    var fields = try std.ArrayList(Field).initCapacity(gpa, 1);
    defer {
        for (fields.items) |*f| f.value.deinit(gpa);
        fields.deinit(gpa);
    }
    try fields.append(gpa, .{ .key = "k", .value = .{ .str = "" } });

    const toon = try dumps(gpa, .{ .object = fields });
    defer gpa.free(toon);

    var back = try loads(gpa, toon);
    defer back.deinit(gpa);

    try std.testing.expectEqualStrings("", back.get("k").?.asStr().?);
}
