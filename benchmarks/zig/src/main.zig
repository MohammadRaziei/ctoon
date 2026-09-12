// CToon Benchmark — Zig.
//
// Peers: ctoon (this project) and toon-zig
// (github.com/LatentEvals/toon-zig), the only pure-Zig TOON
// implementation found at the time this was written. Still one shared
// results table / JSON schema as every other language benchmark here —
// see benchmarks/go/bench.go for the reference version of this comment.
//
// Both dependencies are fetched the same way every other language
// fetches its peers here: as build.zig.zon dependencies (see this
// directory's build.zig.zon) — not local paths, even though this repo's
// root is right above us; see this directory's CMakeLists.txt for why.
// toon-zig requires Zig 0.15.2, which is why ctoon's own Zig binding and
// this whole workspace are pinned to that version too — a single
// `zig build` can only use one Zig language-version API.
//
// Methodology (same across every language benchmark in this repo):
//   1. Load every file in the corpus manifest into memory (untimed).
//   2. Untimed pre-pass: convert each JSON file to TOON once (with
//      ctoon) — the shared decode input the toon_to_json leg reads from.
//   3. Timed "json_to_toon": repeatedly parse JSON and re-serialise to
//      TOON, with each library's own encoder.
//   4. Timed "toon_to_json": repeatedly parse the TOON text from step 2
//      and re-serialise to JSON. Measures interop with ctoon's specific
//      output.
//   5. Timed "roundtrip": json -> toon -> parse toon -> json, all
//      chained as one operation using each library's own encoder/decoder
//      together. Measures self-consistency, not interop.
//   6. Report throughput (MB/s of bytes actually read by successful
//      conversions only) and documents/sec.
//
// Memory: this is a short-lived benchmark process, not a library, so it
// uses one arena for the whole run and never frees anything
// individually — simpler and irrelevant to the numbers being measured.
// toon-zig's own ParseResult and std.json's Parsed(T) each carry their
// own internal arena regardless (per their own docs) — nesting those
// inside our outer arena is harmless, just an extra layer.

const std = @import("std");
const ctoon = @import("ctoon");
const toon = @import("toon");

const repeats: u32 = 20;

const BenchFile = struct {
    path: []const u8,
    json: []const u8,
    toon: ?[]const u8 = null,
};

const Result = struct {
    library: []const u8,
    operation: []const u8,
    throughput_mb_s: f64,
    docs_per_sec: f64,
    success_rate: f64,
    total_time_s: f64,
};

var log_file: ?std.fs.File = null;

fn logFail(library: []const u8, operation: []const u8, path: []const u8, err: anyerror) void {
    const f = log_file orelse return;
    var buf: [1024]u8 = undefined;
    const line = std.fmt.bufPrint(&buf, "[{s}] [{s}] FILE: {s} ERROR: {}\n", .{ library, operation, path, err }) catch return;
    f.writeAll(line) catch {};
}

fn record(gpa: std.mem.Allocator, results: *std.ArrayList(Result), library: []const u8, operation: []const u8, bytes: f64, ops: u64, seconds: f64, attempted: u64) !void {
    const throughput: f64 = if (ops > 0) bytes / seconds / 1e6 else 0.0;
    const success_rate: f64 = if (attempted > 0) @as(f64, @floatFromInt(ops)) / @as(f64, @floatFromInt(attempted)) else 0.0;
    const docs_per_sec: f64 = @as(f64, @floatFromInt(ops)) / seconds;
    std.debug.print("{s:<8} {s:<14} {d:>9.2} MB/s {d:>14.0} {d:>8.0}%  {d:>8.4} s  (x{} reps)\n", .{ library, operation, throughput, docs_per_sec, success_rate * 100.0, seconds, repeats });
    try results.append(gpa, .{
        .library = library,
        .operation = operation,
        .throughput_mb_s = throughput,
        .docs_per_sec = docs_per_sec,
        .success_rate = success_rate,
        .total_time_s = seconds,
    });
}

/// Serialises a std.json.Value to a freshly allocated JSON string, using
/// 0.15's Writer-based std.json.Stringify (the free-function
/// std.json.stringify was removed in this version).
fn jsonToString(gpa: std.mem.Allocator, value: std.json.Value) ![]u8 {
    var aw: std.Io.Writer.Allocating = .init(gpa);
    defer aw.deinit();
    var stringifier: std.json.Stringify = .{ .writer = &aw.writer, .options = .{} };
    try stringifier.write(value);
    return gpa.dupe(u8, aw.written());
}

pub fn main() !void {
    var arena_state = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena_state.deinit();
    const gpa = arena_state.allocator();

    const args = try std.process.argsAlloc(gpa);
    if (args.len < 3) {
        std.debug.print("usage: bench <manifest> <results.json> [log_file]\n", .{});
        std.process.exit(1);
    }
    const manifest_path = args[1];
    const results_path = args[2];
    if (args.len >= 4 and args[3].len > 0) {
        log_file = std.fs.cwd().createFile(args[3], .{}) catch |err| blk: {
            std.debug.print("warning: could not open log file {s}: {}\n", .{ args[3], err });
            break :blk null;
        };
    }
    defer if (log_file) |f| f.close();

    const manifest_text = std.fs.cwd().readFileAlloc(gpa, manifest_path, 64 * 1024 * 1024) catch |err| {
        std.debug.print("Cannot open manifest: {s} ({})\n", .{ manifest_path, err });
        std.process.exit(1);
    };

    var files: std.ArrayList(BenchFile) = .empty;
    var lines = std.mem.splitScalar(u8, manifest_text, '\n');
    while (lines.next()) |raw_line| {
        const path = std.mem.trim(u8, raw_line, " \t\r");
        if (path.len == 0) continue;
        const json = std.fs.cwd().readFileAlloc(gpa, path, 64 * 1024 * 1024) catch continue;
        try files.append(gpa, .{ .path = path, .json = json });
    }
    if (files.items.len == 0) {
        std.debug.print("Corpus manifest is empty or unreadable.\n", .{});
        std.process.exit(1);
    }

    var total_json_bytes: usize = 0;
    for (files.items) |f| total_json_bytes += f.json.len;

    std.debug.print("CToon Benchmarks — Zig\n", .{});
    std.debug.print("Corpus: {} files, {d:.2} MB (JSON)\n\n", .{ files.items.len, @as(f64, @floatFromInt(total_json_bytes)) / 1e6 });
    std.debug.print("{s:<8} {s:<14} {s:>12} {s:>14} {s:>9} {s:>10} {s:>12}\n", .{ "Library", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time" });

    // Untimed pre-pass: TOON text with ctoon, shared decode input.
    for (files.items) |*f| {
        const val = ctoon.loadsJson(gpa, f.json) catch {
            logFail("ctoon", "pre_pass", f.path, ctoon.Error.ParseError);
            continue;
        };
        f.toon = ctoon.dumps(gpa, val) catch {
            logFail("ctoon", "pre_pass", f.path, ctoon.Error.WriteError);
            continue;
        };
    }

    var results: std.ArrayList(Result) = .empty;
    const total = files.items.len * repeats;

    // ── ctoon ──
    {
        var ops: u64 = 0;
        var bytes: f64 = 0;
        var timer = try std.time.Timer.start();
        var r: u32 = 0;
        while (r < repeats) : (r += 1) {
            for (files.items) |f| {
                const val = ctoon.loadsJson(gpa, f.json) catch |err| {
                    if (r == 0) logFail("ctoon", "json_to_toon", f.path, err);
                    continue;
                };
                if (ctoon.dumps(gpa, val)) |_| {
                    ops += 1;
                    bytes += @floatFromInt(f.json.len);
                } else |err| {
                    if (r == 0) logFail("ctoon", "json_to_toon", f.path, err);
                }
            }
        }
        const seconds = @as(f64, @floatFromInt(timer.read())) / 1e9;
        try record(gpa, &results, "ctoon", "json_to_toon", bytes, ops, seconds, total);
    }

    {
        var ops: u64 = 0;
        var bytes: f64 = 0;
        var timer = try std.time.Timer.start();
        var r: u32 = 0;
        while (r < repeats) : (r += 1) {
            for (files.items) |f| {
                const toon_text = f.toon orelse continue;
                const val = ctoon.loads(gpa, toon_text) catch |err| {
                    if (r == 0) logFail("ctoon", "toon_to_json", f.path, err);
                    continue;
                };
                if (ctoon.dumpsJson(gpa, val, 2)) |_| {
                    ops += 1;
                    bytes += @floatFromInt(toon_text.len);
                } else |err| {
                    if (r == 0) logFail("ctoon", "toon_to_json", f.path, err);
                }
            }
        }
        const seconds = @as(f64, @floatFromInt(timer.read())) / 1e9;
        try record(gpa, &results, "ctoon", "toon_to_json", bytes, ops, seconds, total);
    }

    {
        var ops: u64 = 0;
        var bytes: f64 = 0;
        var timer = try std.time.Timer.start();
        var r: u32 = 0;
        while (r < repeats) : (r += 1) {
            for (files.items) |f| {
                const val = ctoon.loadsJson(gpa, f.json) catch |err| {
                    if (r == 0) logFail("ctoon", "roundtrip", f.path, err);
                    continue;
                };
                const toon_text = ctoon.dumps(gpa, val) catch |err| {
                    if (r == 0) logFail("ctoon", "roundtrip", f.path, err);
                    continue;
                };
                const val2 = ctoon.loads(gpa, toon_text) catch |err| {
                    if (r == 0) logFail("ctoon", "roundtrip", f.path, err);
                    continue;
                };
                if (ctoon.dumpsJson(gpa, val2, 2)) |_| {
                    ops += 1;
                    bytes += @floatFromInt(f.json.len);
                } else |err| {
                    if (r == 0) logFail("ctoon", "roundtrip", f.path, err);
                }
            }
        }
        const seconds = @as(f64, @floatFromInt(timer.read())) / 1e9;
        try record(gpa, &results, "ctoon", "roundtrip", bytes, ops, seconds, total);
    }

    // ── toon-zig ──
    {
        var ops: u64 = 0;
        var bytes: f64 = 0;
        var timer = try std.time.Timer.start();
        var r: u32 = 0;
        while (r < repeats) : (r += 1) {
            for (files.items) |f| {
                var parsed = std.json.parseFromSlice(std.json.Value, gpa, f.json, .{}) catch |err| {
                    if (r == 0) logFail("toon-zig", "json_to_toon", f.path, err);
                    continue;
                };
                defer parsed.deinit();
                if (toon.stringify(gpa, parsed.value, .{})) |out| {
                    gpa.free(out);
                    ops += 1;
                    bytes += @floatFromInt(f.json.len);
                } else |err| {
                    if (r == 0) logFail("toon-zig", "json_to_toon", f.path, err);
                }
            }
        }
        const seconds = @as(f64, @floatFromInt(timer.read())) / 1e9;
        try record(gpa, &results, "toon-zig", "json_to_toon", bytes, ops, seconds, total);
    }

    {
        var ops: u64 = 0;
        var bytes: f64 = 0;
        var timer = try std.time.Timer.start();
        var r: u32 = 0;
        while (r < repeats) : (r += 1) {
            for (files.items) |f| {
                const toon_text = f.toon orelse continue;
                var result = toon.parse(gpa, toon_text, .{}) catch |err| {
                    if (r == 0) logFail("toon-zig", "toon_to_json", f.path, err);
                    continue;
                };
                defer result.deinit();
                if (jsonToString(gpa, result.value)) |out| {
                    gpa.free(out);
                    ops += 1;
                    bytes += @floatFromInt(toon_text.len);
                } else |err| {
                    if (r == 0) logFail("toon-zig", "toon_to_json", f.path, err);
                }
            }
        }
        const seconds = @as(f64, @floatFromInt(timer.read())) / 1e9;
        try record(gpa, &results, "toon-zig", "toon_to_json", bytes, ops, seconds, total);
    }

    {
        var ops: u64 = 0;
        var bytes: f64 = 0;
        var timer = try std.time.Timer.start();
        var r: u32 = 0;
        while (r < repeats) : (r += 1) {
            for (files.items) |f| {
                var parsed = std.json.parseFromSlice(std.json.Value, gpa, f.json, .{}) catch |err| {
                    if (r == 0) logFail("toon-zig", "roundtrip", f.path, err);
                    continue;
                };
                defer parsed.deinit();
                const toon_text = toon.stringify(gpa, parsed.value, .{}) catch |err| {
                    if (r == 0) logFail("toon-zig", "roundtrip", f.path, err);
                    continue;
                };
                defer gpa.free(toon_text);
                var result2 = toon.parse(gpa, toon_text, .{}) catch |err| {
                    if (r == 0) logFail("toon-zig", "roundtrip", f.path, err);
                    continue;
                };
                defer result2.deinit();
                if (jsonToString(gpa, result2.value)) |out| {
                    gpa.free(out);
                    ops += 1;
                    bytes += @floatFromInt(f.json.len);
                } else |err| {
                    if (r == 0) logFail("toon-zig", "roundtrip", f.path, err);
                }
            }
        }
        const seconds = @as(f64, @floatFromInt(timer.read())) / 1e9;
        try record(gpa, &results, "toon-zig", "roundtrip", bytes, ops, seconds, total);
    }

    // ── write results JSON ──
    var aw: std.Io.Writer.Allocating = .init(gpa);
    defer aw.deinit();
    const w = &aw.writer;
    try w.print("{{\n  \"language\": \"zig\",\n", .{});
    try w.print("  \"corpus\": {{\"files\": {}, \"bytes\": {}}},\n", .{ files.items.len, total_json_bytes });
    try w.print("  \"results\": [\n", .{});
    for (results.items, 0..) |r, i| {
        try w.print(
            "    {{\"library\": \"{s}\", \"operation\": \"{s}\", \"throughput_mb_s\": {d:.4}, \"docs_per_sec\": {d:.2}, \"success_rate\": {d:.4}, \"total_time_s\": {d:.6}}}{s}\n",
            .{ r.library, r.operation, r.throughput_mb_s, r.docs_per_sec, r.success_rate, r.total_time_s, if (i + 1 < results.items.len) "," else "" },
        );
    }
    try w.print("  ]\n}}\n", .{});

    std.fs.cwd().writeFile(.{ .sub_path = results_path, .data = aw.written() }) catch |err| {
        std.debug.print("warning: could not write {s}: {}\n", .{ results_path, err });
        return;
    };
    std.debug.print("\nResults written to {s}\n", .{results_path});
    if (log_file != null) {
        std.debug.print("Debug log written to {s}\n", .{args[3]});
    }
}
