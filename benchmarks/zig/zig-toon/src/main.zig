// toon-zig benchmark (github.com/LatentEvals/toon-zig) - built with Zig 0.15.2.
//
// A separate executable from ../ctoon's harness because the two need
// different Zig versions; see build.zig. Same methodology, same five
// operations, same result schema, and its rows are merged into zig.json by
// merge_results.py as the library "toon-zig".
//
//   usage: toon-zig-bench <manifest> <results dir> <partial results.json> [log]
//
// Methodology (as in every language benchmark here):
//   1. Load every corpus file into memory (untimed).
//   2. Shared decode input: toon_to_json / toon_to_struct read the TOON that
//      ctoon produced for each file (ctoon's harness, ../ctoon, dumps it to
//      <results dir>/dump_zig_ctoon_json_to_toon.frames - exactly what
//      node/verify_outputs.mjs treats as the shared input), not TOON produced
//      by toon-zig itself, so this measures interop with ctoon's output.
//   3. Untimed: build std.json.Value for every file with the standard
//      library's JSON parser. struct_to_toon encodes these values.
//   4. Timed, each repeated `repeats` times over the corpus:
//        json_to_toon    std.json parse -> toon.stringify. toon-zig has no
//                        text-to-text call, so the JSON parse is part of the
//                        measurement (same std.json.Value is the shared type).
//        toon_to_json    toon.parse -> std.json.Stringify
//        roundtrip       json -> toon -> toon.parse -> json
//        toon_to_struct  toon.parse only (std.json.Value is the "native structure")
//        struct_to_toon  toon.stringify of the pre-built value only
//   5. Report throughput (MB/s of bytes actually converted successfully) and
//      documents/sec.
//
// Memory: the ctoon harness uses one arena for the whole run and never frees.
// Here every file's work happens in a scratch arena that is reset after each
// file - the same bump allocation and no per-object frees, but the memory
// stays bounded, because toon-zig's parse result owns an arena of its own.
// The reset is inside the timed region (it is cheap); the allocator is
// otherwise not tuned for either library.

const std = @import("std");
const toon = @import("toon");

const Allocator = std.mem.Allocator;

const repeats: u32 = 20;
const library = "toon-zig";

const Op = enum { json_to_toon, toon_to_json, roundtrip, toon_to_struct, struct_to_toon };
const all_ops = [_]Op{ .json_to_toon, .toon_to_json, .roundtrip, .toon_to_struct, .struct_to_toon };

const BenchFile = struct {
    path: []const u8,
    json: []const u8,
    /// ctoon's TOON for this file (shared decode input); null if ctoon failed on it.
    toon: ?[]const u8 = null,
    /// std.json.Value, built untimed; the input of struct_to_toon.
    value: ?std.json.Value = null,
};

const Result = struct {
    operation: []const u8,
    throughput_mb_s: f64,
    docs_per_sec: f64,
    success_rate: f64,
    total_time_s: f64,
};

var log_file: ?std.fs.File = null;

fn logFail(operation: []const u8, path: []const u8, err: anyerror) void {
    const f = log_file orelse return;
    var buf: [1024]u8 = undefined;
    const line = std.fmt.bufPrint(&buf, "[{s}] [{s}] FILE: {s} ERROR: {s}\n", .{ library, operation, path, @errorName(err) }) catch return;
    f.writeAll(line) catch {};
}

/// One operation on one file; everything it allocates comes from `a`.
/// Returns the text the operation produced. error.Skip means "this file has
/// no input for this operation" (ctoon produced no TOON for it) and is not
/// counted as an attempt by the caller. `for_dump` only matters for
/// toon_to_struct: the timed run stops after decoding, the (untimed) dump
/// also serialises the decoded value to JSON so the verifier can judge it.
fn produce(op: Op, a: Allocator, f: BenchFile, for_dump: bool) ![]const u8 {
    switch (op) {
        .json_to_toon => {
            const parsed = try std.json.parseFromSlice(std.json.Value, a, f.json, .{});
            return try toon.stringify(a, parsed.value, .{});
        },
        .toon_to_json => {
            const text = f.toon orelse return error.Skip;
            const res = try toon.parse(a, text, .{});
            return try std.json.Stringify.valueAlloc(a, res.value, .{});
        },
        .roundtrip => {
            const parsed = try std.json.parseFromSlice(std.json.Value, a, f.json, .{});
            const text = try toon.stringify(a, parsed.value, .{});
            const back = try toon.parse(a, text, .{});
            return try std.json.Stringify.valueAlloc(a, back.value, .{});
        },
        .toon_to_struct => {
            const text = f.toon orelse return error.Skip;
            const res = try toon.parse(a, text, .{});
            if (for_dump) return try std.json.Stringify.valueAlloc(a, res.value, .{});
            std.mem.doNotOptimizeAway(&res);
            return "";
        },
        .struct_to_toon => {
            const v = f.value orelse return error.Skip;
            return try toon.stringify(a, v, .{});
        },
    }
}

/// Bytes a successful operation counts towards throughput: the TOON size for
/// the operations fed by TOON, the JSON size for the others (same convention
/// as the other harnesses).
fn weight(op: Op, f: BenchFile) usize {
    return switch (op) {
        .toon_to_json, .toon_to_struct => if (f.toon) |t| t.len else 0,
        else => f.json.len,
    };
}

fn record(gpa: Allocator, results: *std.ArrayList(Result), operation: []const u8, bytes: f64, ops: u64, seconds: f64, attempted: usize) !void {
    const throughput: f64 = if (ops > 0) bytes / seconds / 1e6 else 0.0;
    const success_rate: f64 = if (attempted > 0) @as(f64, @floatFromInt(ops)) / @as(f64, @floatFromInt(attempted)) else 0.0;
    const docs_per_sec: f64 = @as(f64, @floatFromInt(ops)) / seconds;
    std.debug.print("{s:<8} {s:<14} {d:>9.2} MB/s {d:>14.0} {d:>8.0}%  {d:>8.4} s  (x{} reps)\n", .{ library, operation, throughput, docs_per_sec, success_rate * 100.0, seconds, repeats });
    try results.append(gpa, .{
        .operation = operation,
        .throughput_mb_s = throughput,
        .docs_per_sec = docs_per_sec,
        .success_rate = success_rate,
        .total_time_s = seconds,
    });
}

fn bench(gpa: Allocator, scratch: *std.heap.ArenaAllocator, results: *std.ArrayList(Result), op: Op, files: []const BenchFile) !void {
    var ops: u64 = 0;
    var bytes: f64 = 0;
    var timer = try std.time.Timer.start();
    var r: u32 = 0;
    while (r < repeats) : (r += 1) {
        for (files) |f| {
            if (produce(op, scratch.allocator(), f, false)) |out| {
                std.mem.doNotOptimizeAway(out.ptr);
                ops += 1;
                bytes += @floatFromInt(weight(op, f));
            } else |err| {
                if (r == 0 and err != error.Skip) logFail(@tagName(op), f.path, err);
            }
            _ = scratch.reset(.retain_capacity);
        }
    }
    const seconds = @as(f64, @floatFromInt(timer.read())) / 1e9;
    try record(gpa, results, @tagName(op), bytes, ops, seconds, files.len * repeats);
}

/// Untimed. One framed file per operation next to the other dumps, in the
/// layout node/verify_outputs.mjs reads: records of
/// "<file index>\t<byte length>\n<bytes>"; a file toon-zig failed on has no
/// record. Named dump_zig_toon-zig_<operation>.frames.
fn frame(w: *std.Io.Writer, idx: usize, text: []const u8) void {
    w.print("{d}\t{d}\n", .{ idx, text.len }) catch return;
    w.writeAll(text) catch return;
}

fn dumpOutputs(scratch: *std.heap.ArenaAllocator, results_dir: []const u8, files: []const BenchFile) void {
    for (all_ops) |op| {
        var aw: std.Io.Writer.Allocating = .init(std.heap.page_allocator);
        defer aw.deinit();
        for (files, 0..) |f, i| {
            if (produce(op, scratch.allocator(), f, true)) |text| frame(&aw.writer, i, text) else |_| {}
            _ = scratch.reset(.retain_capacity);
        }
        const path = std.fmt.allocPrint(std.heap.page_allocator, "{s}/dump_zig_{s}_{s}.frames", .{ results_dir, library, @tagName(op) }) catch continue;
        defer std.heap.page_allocator.free(path);
        std.fs.cwd().writeFile(.{ .sub_path = path, .data = aw.written() }) catch {};
    }
}

pub fn main() !void {
    // `keep` holds everything that lives for the whole run (corpus, shared
    // TOON, pre-built values, results); `scratch` is reset after every file.
    var keep = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    const gpa = keep.allocator();
    var scratch = std.heap.ArenaAllocator.init(std.heap.page_allocator);

    const args = try std.process.argsAlloc(gpa);
    if (args.len < 4) {
        std.debug.print("usage: toon-zig-bench <manifest> <results dir> <partial results.json> [log_file]\n", .{});
        std.process.exit(1);
    }
    const manifest_path = args[1];
    const results_dir = args[2];
    const out_path = args[3];
    if (args.len >= 5 and args[4].len > 0) {
        log_file = std.fs.cwd().createFile(args[4], .{}) catch |err| blk: {
            std.debug.print("warning: could not open log file {s}: {s}\n", .{ args[4], @errorName(err) });
            break :blk null;
        };
    }
    defer if (log_file) |f| f.close();

    const manifest_text = std.fs.cwd().readFileAlloc(gpa, manifest_path, 64 * 1024 * 1024) catch |err| {
        std.debug.print("Cannot open manifest: {s} ({s})\n", .{ manifest_path, @errorName(err) });
        std.process.exit(1);
    };

    // Same loading rule as ../ctoon's harness (a file that cannot be read is
    // left out), so file indices line up with ctoon's dump.
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

    std.debug.print("CToon Benchmarks - Zig (toon-zig)\n", .{});
    std.debug.print("Corpus: {} files, {d:.2} MB (JSON)\n\n", .{ files.items.len, @as(f64, @floatFromInt(total_json_bytes)) / 1e6 });
    std.debug.print("{s:<8} {s:<14} {s:>12} {s:>14} {s:>9} {s:>10} {s:>12}\n", .{ "Library", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time" });

    // Shared decode input: ctoon's TOON, from the ctoon Zig harness's dump.
    const frames_path = try std.fmt.allocPrint(gpa, "{s}/dump_zig_ctoon_json_to_toon.frames", .{results_dir});
    const frames = std.fs.cwd().readFileAlloc(gpa, frames_path, 1024 * 1024 * 1024) catch |err| {
        std.debug.print("Cannot read {s} ({s}): the ctoon Zig benchmark has to run first.\n", .{ frames_path, @errorName(err) });
        std.process.exit(1);
    };
    var pos: usize = 0;
    while (pos < frames.len) {
        const nl = std.mem.indexOfScalarPos(u8, frames, pos, '\n') orelse break;
        const header = frames[pos..nl];
        const tab = std.mem.indexOfScalar(u8, header, '\t') orelse break;
        const idx = std.fmt.parseInt(usize, header[0..tab], 10) catch break;
        const len = std.fmt.parseInt(usize, header[tab + 1 ..], 10) catch break;
        if (nl + 1 + len > frames.len) break;
        if (idx < files.items.len) files.items[idx].toon = frames[nl + 1 .. nl + 1 + len];
        pos = nl + 1 + len;
    }

    // Untimed: std.json.Value for struct_to_toon, built by the standard
    // library, never by toon-zig.
    for (files.items) |*f| {
        const parsed = std.json.parseFromSlice(std.json.Value, gpa, f.json, .{}) catch |err| {
            logFail("pre_pass", f.path, err);
            continue;
        };
        f.value = parsed.value;
    }

    dumpOutputs(&scratch, results_dir, files.items);

    var results: std.ArrayList(Result) = .empty;
    for (all_ops) |op| try bench(gpa, &scratch, &results, op, files.items);

    // ── write the partial results JSON (merged into zig.json afterwards) ──
    var aw: std.Io.Writer.Allocating = .init(std.heap.page_allocator);
    defer aw.deinit();
    const w = &aw.writer;
    try w.print("{{\n  \"language\": \"zig\",\n", .{});
    try w.print("  \"corpus\": {{\"files\": {}, \"bytes\": {}}},\n", .{ files.items.len, total_json_bytes });
    try w.print("  \"results\": [\n", .{});
    for (results.items, 0..) |r, i| {
        try w.print(
            "    {{\"library\": \"{s}\", \"operation\": \"{s}\", \"throughput_mb_s\": {d:.4}, \"docs_per_sec\": {d:.2}, \"success_rate\": {d:.4}, \"total_time_s\": {d:.6}}}{s}\n",
            .{ library, r.operation, r.throughput_mb_s, r.docs_per_sec, r.success_rate, r.total_time_s, if (i + 1 < results.items.len) "," else "" },
        );
    }
    try w.print("  ]\n}}\n", .{});

    std.fs.cwd().writeFile(.{ .sub_path = out_path, .data = aw.written() }) catch |err| {
        std.debug.print("warning: could not write {s}: {s}\n", .{ out_path, @errorName(err) });
        return;
    };
    std.debug.print("\nResults written to {s}\n", .{out_path});
}
