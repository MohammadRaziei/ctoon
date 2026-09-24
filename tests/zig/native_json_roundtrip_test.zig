//! std.json (Zig's stdlib JSON parser — no dependency needed, unlike
//! Rust/Julia's serde_json/JSON.jl, which had to be added as a dev-only
//! dependency for their equivalent of this file) vs ctoon's own JSON
//! reader/writer (`loadsJson`/`dumps` in src/bindings/zig/src/ctoon.zig,
//! which call ctoon.c's own cj_* parser via shim.c).
//!
//! Every other Zig test builds a `ctoon.Value` by hand or goes through
//! `ctoon.loadsJson()` -- ctoon's OWN JSON parser. None of them ever run a
//! real JSON document through Zig's own, independent JSON parser and hand
//! that result to ctoon's TOON writer. That conversion path (`toCtoonValue`
//! below, std.json.Value -> ctoon.Value) is the Zig analogue of the
//! struct-array / null-as-NaN / logical-array bugs found in the MATLAB
//! binding's own type-conversion layer -- see tests/matlab/test_ctoon.m's
//! "MATLAB-native jsondecode() round trips" section, and the Python/Go/
//! Rust siblings of this same file for the other bindings.
//!
//! An ArenaAllocator backs the whole test: std.json.parseFromSlice and
//! ctoon.Value's own tree structure both require per-node deinit() calls
//! to avoid leaking, and freeing the *converted* tree correctly (as
//! opposed to the parsed std.json.Value tree, which parseFromSliceLeaky
//! already hands off to the arena) is exactly the kind of bookkeeping
//! bug-prone enough that an arena is the standard way to sidestep it
//! entirely in a test -- one deinit for everything, not "did I free this
//! specific ctoon.Value branch correctly".
//!
//! NOTE: written against Zig 0.16's std.json.Value shape as of this
//! writing (.null, .bool, .integer: i64, .float: f64, .number_string,
//! .string, .array: std.json.Array, .object: std.json.ObjectMap). Unlike
//! every other file changed in this pass, this one could not be run here
//! (no Zig toolchain in this sandbox) even to type-check, only cross-
//! checked by hand against ctoon.zig's own Value definition. If Zig's
//! std.json field names have moved since, the fix is almost certainly
//! confined to toCtoonValue()'s switch arms below, nothing else in this
//! file needing to be reasoned through again.
//!
//! Expected TOON strings are copied verbatim from toon-format/spec's own
//! tests/fixtures/encode/*.json -- never from running ctoon's own CLI or
//! any of ctoon's own bindings against these inputs (that would only prove
//! the implementation agrees with itself). Each case names its source file.

const std = @import("std");
const testing = std.testing;
const ctoon = @import("ctoon");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// `tests/data/`, relative to the repo root `zig build test` is invoked
/// from (see tests/zig/CMakeLists.txt's WORKING_DIRECTORY, and build.zig's
/// own root_source_file paths, both repo-root-relative the same way).
const local_data_dir = "tests/data";

/// Returns null (meaning "skip") when CTOON_SPEC_EXAMPLES_DIR isn't set or
/// doesn't exist -- only available under CMake/ctest, which fetches
/// toon-format/spec centrally (see tests/CMakeLists.txt). `std.testing`
/// has no runtime "skip" status any more than Rust's harness does; an
/// early return is the same convention that file uses.
fn specExamplesDir(gpa: std.mem.Allocator) ?[]const u8 {
    const dir = std.process.getEnvVarOwned(gpa, "CTOON_SPEC_EXAMPLES_DIR") catch return null;
    if (dir.len == 0) {
        gpa.free(dir);
        return null;
    }
    var d = std.fs.cwd().openDir(dir, .{}) catch {
        gpa.free(dir);
        return null;
    };
    d.close();
    return dir;
}

fn toCtoonValue(gpa: std.mem.Allocator, v: std.json.Value) !ctoon.Value {
    return switch (v) {
        .null => .null,
        .bool => |b| .{ .boolean = b },
        .integer => |n| if (n >= 0) .{ .uint = @intCast(n) } else .{ .sint = n },
        .float => |f| .{ .real = f },
        .number_string => |s| blk: {
            // A number too large/precise to fit i64/f64 losslessly, kept
            // as its original text by std.json. ctoon.Value has no
            // "arbitrary precision" variant either (same as every other
            // binding here), so fall back to f64 -- consistent with what
            // this same corpus already exercises for exact-int precision
            // elsewhere (see tests/python/test_native_json_roundtrip.py's
            // module doc on tests/data/twitter.toon's id field, which
            // stays within i64 range and so does NOT hit this branch).
            break :blk .{ .real = try std.fmt.parseFloat(f64, s) };
        },
        .string => |s| .{ .str = try gpa.dupe(u8, s) },
        .array => |arr| blk: {
            var out = try std.ArrayList(ctoon.Value).initCapacity(gpa, arr.items.len);
            for (arr.items) |item| out.appendAssumeCapacity(try toCtoonValue(gpa, item));
            break :blk .{ .array = out };
        },
        .object => |obj| blk: {
            var out = try std.ArrayList(ctoon.Field).initCapacity(gpa, obj.count());
            var it = obj.iterator();
            while (it.next()) |entry| {
                out.appendAssumeCapacity(.{
                    .key = try gpa.dupe(u8, entry.key_ptr.*),
                    .value = try toCtoonValue(gpa, entry.value_ptr.*),
                });
            }
            break :blk .{ .object = out };
        },
    };
}

/// Numeric-tolerant equality between a std.json.Value and a ctoon.Value
/// (rather than converting one to the other and diffing that, which would
/// hide exactly the kind of int/float mismatch this test exists to catch).
fn valuesEqual(native: std.json.Value, own: ctoon.Value) bool {
    return switch (native) {
        .null => own == .null,
        .bool => |a| own == .boolean and own.boolean == a,
        .integer => |a| switch (own) {
            .uint => |b| a >= 0 and @as(u64, @intCast(a)) == b,
            .sint => |b| a == b,
            else => false,
        },
        .float => |a| own == .real and own.real == a,
        .number_string => |a| own == .real and (std.fmt.parseFloat(f64, a) catch return false) == own.real,
        .string => |a| own == .str and std.mem.eql(u8, a, own.str),
        .array => |a| blk: {
            if (own != .array or a.items.len != own.array.items.len) break :blk false;
            for (a.items, own.array.items) |x, y| {
                if (!valuesEqual(x, y)) break :blk false;
            }
            break :blk true;
        },
        .object => |a| blk: {
            if (own != .object or a.count() != own.object.items.len) break :blk false;
            var it = a.iterator();
            while (it.next()) |entry| {
                var found = false;
                for (own.object.items) |field| {
                    if (std.mem.eql(u8, field.key, entry.key_ptr.*)) {
                        if (!valuesEqual(entry.value_ptr.*, field.value)) break :blk false;
                        found = true;
                        break;
                    }
                }
                if (!found) break :blk false;
            }
            break :blk true;
        },
    };
}

fn trimTrailingNewlines(s: []const u8) []const u8 {
    var end = s.len;
    while (end > 0 and s[end - 1] == '\n') : (end -= 1) {}
    return s[0..end];
}

/// Every `<name>.json` in `dir` with a matching `<name>.toon`, sorted for a
/// deterministic run order.
fn pairFiles(gpa: std.mem.Allocator, dir_path: []const u8) ![][]const u8 {
    var dir = try std.fs.cwd().openDir(dir_path, .{ .iterate = true });
    defer dir.close();

    var names = std.ArrayList([]const u8).empty;
    var it = dir.iterate();
    while (try it.next()) |entry| {
        if (entry.kind != .file) continue;
        if (!std.mem.endsWith(u8, entry.name, ".json")) continue;
        const stem = entry.name[0 .. entry.name.len - ".json".len];
        const toon_name = try std.fmt.allocPrint(gpa, "{s}.toon", .{stem});
        defer gpa.free(toon_name);
        dir.access(toon_name, .{}) catch continue;
        try names.append(gpa, try gpa.dupe(u8, stem));
    }
    std.mem.sort([]const u8, names.items, {}, struct {
        fn lessThan(_: void, a: []const u8, b: []const u8) bool {
            return std.mem.order(u8, a, b) == .lt;
        }
    }.lessThan);
    return names.toOwnedSlice(gpa);
}

fn checkPair(gpa: std.mem.Allocator, dir_path: []const u8, name: []const u8) !void {
    var dir = try std.fs.cwd().openDir(dir_path, .{});
    defer dir.close();

    const json_name = try std.fmt.allocPrint(gpa, "{s}.json", .{name});
    defer gpa.free(json_name);
    const raw_json = try dir.readFileAlloc(gpa, json_name, 64 * 1024 * 1024);

    const parsed = try std.json.parseFromSliceLeaky(std.json.Value, gpa, raw_json, .{});

    var own = try ctoon.loadsJson(gpa, raw_json);
    defer own.deinit(gpa);

    if (!valuesEqual(parsed, own)) {
        std.debug.print("{s}.json: std.json and ctoon.loadsJson disagree on the parsed value\n", .{name});
        return error.ParserMismatch;
    }

    const converted = try toCtoonValue(gpa, parsed);
    const produced = try ctoon.dumps(gpa, converted);
    defer gpa.free(produced);

    const toon_name = try std.fmt.allocPrint(gpa, "{s}.toon", .{name});
    defer gpa.free(toon_name);
    const expected = try dir.readFileAlloc(gpa, toon_name, 64 * 1024 * 1024);

    try testing.expectEqualStrings(trimTrailingNewlines(expected), trimTrailingNewlines(produced));

    var round_tripped = try ctoon.loads(gpa, produced);
    defer round_tripped.deinit(gpa);
    if (!valuesEqual(parsed, round_tripped)) {
        std.debug.print("{s}: TOON round trip changed the value\n", .{name});
        return error.RoundTripMismatch;
    }
}

// ---------------------------------------------------------------------------
// tests/data/*.json + *.toon — local to this repo, always available.
// ---------------------------------------------------------------------------

test "native json roundtrip: local data corpus" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const gpa = arena.allocator();

    const names = try pairFiles(gpa, local_data_dir);
    try testing.expect(names.len > 0);
    for (names) |name| {
        try checkPair(gpa, local_data_dir, name);
    }
}

// ---------------------------------------------------------------------------
// toon-format/spec's examples/conversions/*.json + *.toon.
// ---------------------------------------------------------------------------

test "native json roundtrip: spec examples corpus" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const gpa = arena.allocator();

    const dir_path = specExamplesDir(gpa) orelse {
        std.debug.print("CTOON_SPEC_EXAMPLES_DIR not set/found — skipping (only available under CMake/ctest).\n", .{});
        return;
    };

    const names = try pairFiles(gpa, dir_path);
    try testing.expect(names.len > 0);
    for (names) |name| {
        try checkPair(gpa, dir_path, name);
    }
}
