// CToon Benchmark — Zig.
//
// ctoon (this project) only for now. toon-zig
// (github.com/LatentEvals/toon-zig) is the only pure-Zig TOON peer
// found, but doesn't compile under Zig 0.16 yet — an upstream issue,
// not something to patch around here; see build.zig's comment for the
// specifics. Once it's fixed upstream, re-add it here the same way it
// was removed: a `toon_dep`/`addImport("toon", ...)` pair in build.zig,
// and json_to_toon/toon_to_json/roundtrip legs calling `toon.stringify`/
// `toon.parse` alongside ctoon's own, using `std.json.Value` as the
// shared decode type — kept as one shared results table / JSON schema,
// same as every other language benchmark here; see
// benchmarks/go/bench.go for the reference version of this comment.
//
// ctoon is fetched the same way every other language fetches its peers
// here: as a build.zig.zon dependency (see this directory's
// build.zig.zon) — not a local path, even though this repo's root is
// right above us; see this directory's CMakeLists.txt for why.
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
// uses `init.arena` (Zig 0.16's "Juicy Main" — see main()'s signature)
// for the whole run and never frees anything individually — simpler and
// irrelevant to the numbers being measured.
//
// I/O: Zig 0.16 replaced std.fs with std.Io.Dir/std.Io.File, both of
// which take an explicit `io: std.Io` handle on every call (init.io,
// from "Juicy Main") — see https://ziglang.org/download/0.16.0/release-notes.html.

const std = @import("std");
const ctoon = @import("ctoon");

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

var io: std.Io = undefined;
var log_file: ?std.Io.File = null;

fn logFail(library: []const u8, operation: []const u8, path: []const u8, err: anyerror) void {
    const f = log_file orelse return;
    var buf: [1024]u8 = undefined;
    const line = std.fmt.bufPrint(&buf, "[{s}] [{s}] FILE: {s} ERROR: {}\n", .{ library, operation, path, err }) catch return;
    f.writeStreamingAll(io, line) catch {};
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

pub fn main(init: std.process.Init) !void {
    io = init.io;
    const gpa = init.arena.allocator();

    const args = try init.minimal.args.toSlice(gpa);
    if (args.len < 3) {
        std.debug.print("usage: bench <manifest> <results.json> [log_file]\n", .{});
        std.process.exit(1);
    }
    const manifest_path = args[1];
    const results_path = args[2];
    if (args.len >= 4 and args[3].len > 0) {
        log_file = std.Io.Dir.cwd().createFile(io, args[3], .{}) catch |err| blk: {
            std.debug.print("warning: could not open log file {s}: {}\n", .{ args[3], err });
            break :blk null;
        };
    }
    defer if (log_file) |f| f.close(io);

    const manifest_text = std.Io.Dir.cwd().readFileAlloc(io, manifest_path, gpa, .limited(64 * 1024 * 1024)) catch |err| {
        std.debug.print("Cannot open manifest: {s} ({})\n", .{ manifest_path, err });
        std.process.exit(1);
    };

    var files: std.ArrayList(BenchFile) = .empty;
    var lines = std.mem.splitScalar(u8, manifest_text, '\n');
    while (lines.next()) |raw_line| {
        const path = std.mem.trim(u8, raw_line, " \t\r");
        if (path.len == 0) continue;
        const json = std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(64 * 1024 * 1024)) catch continue;
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
        const timer_start = std.Io.Clock.awake.now(io);
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
        const seconds = @as(f64, @floatFromInt(timer_start.durationTo(std.Io.Clock.awake.now(io)).nanoseconds)) / 1e9;
        try record(gpa, &results, "ctoon", "json_to_toon", bytes, ops, seconds, total);
    }

    {
        var ops: u64 = 0;
        var bytes: f64 = 0;
        const timer_start = std.Io.Clock.awake.now(io);
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
        const seconds = @as(f64, @floatFromInt(timer_start.durationTo(std.Io.Clock.awake.now(io)).nanoseconds)) / 1e9;
        try record(gpa, &results, "ctoon", "toon_to_json", bytes, ops, seconds, total);
    }

    {
        var ops: u64 = 0;
        var bytes: f64 = 0;
        const timer_start = std.Io.Clock.awake.now(io);
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
        const seconds = @as(f64, @floatFromInt(timer_start.durationTo(std.Io.Clock.awake.now(io)).nanoseconds)) / 1e9;
        try record(gpa, &results, "ctoon", "roundtrip", bytes, ops, seconds, total);
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

    std.Io.Dir.cwd().writeFile(io, .{ .sub_path = results_path, .data = aw.written() }) catch |err| {
        std.debug.print("warning: could not write {s}: {}\n", .{ results_path, err });
        return;
    };
    std.debug.print("\nResults written to {s}\n", .{results_path});
    if (log_file != null) {
        std.debug.print("Debug log written to {s}\n", .{args[3]});
    }
}
