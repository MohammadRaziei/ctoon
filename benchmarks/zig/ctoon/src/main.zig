// CToon Benchmark - Zig.
//
// ctoon only: toon-zig (github.com/LatentEvals/toon-zig) is benchmarked
// by ../zig-toon, a separate executable built with Zig 0.15.2 (this one needs
// Zig 0.16.x), and its rows are merged into the same zig.json. That harness
// uses the TOON dumped by this one (dump_zig_ctoon_json_to_toon.frames) as its
// shared decode input. Every result is kept as one shared results table / JSON
// schema, same as every other language benchmark here; see
// benchmarks/go/bench.go for the reference version of this comment.
//
// ctoon is fetched the same way every other language fetches its peers
// here: as a build.zig.zon dependency (see this directory's
// build.zig.zon) - not a local path, even though this repo's root is
// right above us; see this directory's CMakeLists.txt for why.
//
// Methodology (same across every language benchmark in this repo):
//   1. Load every file in the corpus manifest into memory (untimed).
//   2. Untimed pre-pass: convert each JSON file to TOON once (with
//      ctoon) - the shared decode input the toon_to_json leg reads from.
//   3. Timed "json_to_toon": repeatedly parse JSON and re-serialise to
//      TOON, with each library's own encoder.
//   4. Timed "toon_to_json": repeatedly parse the TOON text from step 2
//      and re-serialise to JSON. Measures interop with ctoon's specific
//      output.
//   5. Timed "roundtrip": json -> toon -> parse toon -> json, all
//      chained as one operation using each library's own encoder/decoder
//      together. Measures self-consistency, not interop.
//   6. Timed "toon_to_struct" / "struct_to_toon": the same two directions
//      with the text format taken out -- decode the pre-pass TOON into a
//      ctoon.Value and stop; encode a ctoon.Value built untimed in the
//      pre-pass. There is no other Zig TOON implementation here, so
//      ctoon.Value is the "native structure".
//   7. Report throughput (MB/s of bytes actually read by successful
//      conversions only) and documents/sec.
//
// Memory: this is a short-lived benchmark process, not a library, so it
// uses `init.arena` (Zig 0.16's "Juicy Main" - see main()'s signature)
// for the whole run and never frees anything individually - simpler and
// irrelevant to the numbers being measured.
//
// I/O: Zig 0.16 replaced std.fs with std.Io.Dir/std.Io.File, both of
// which take an explicit `io: std.Io` handle on every call (init.io,
// from "Juicy Main") - see https://ziglang.org/download/0.16.0/release-notes.html.

const std = @import("std");
const ctoon = @import("ctoon");

const repeats: u32 = 20;

const BenchFile = struct {
    path: []const u8,
    json: []const u8,
    toon: ?[]const u8 = null,
    /// ctoon's in-memory value for this file, built UNTIMED in the pre-pass;
    /// the input of struct_to_toon.
    value: ?ctoon.Value = null,
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

/// Untimed. Records what each ctoon operation actually produced for each file,
/// so report/verify_outputs.mjs can judge whether the output is CORRECT, not
/// merely error-free -- see that script's header. Unlike the other languages
/// (one file per output under dump/), this writes ONE framed file per
/// operation next to the results JSON, because it only uses calls this file
/// already makes (Writer.Allocating, Dir.writeFile): each record is
/// "<file index>\t<byte length>\n<bytes>", and a file ctoon failed on simply
/// has no record.
fn frame(w: *std.Io.Writer, idx: usize, text: []const u8) void {
    w.print("{d}\t{d}\n", .{ idx, text.len }) catch return;
    w.writeAll(text) catch return;
}

fn writeFrames(gpa: std.mem.Allocator, dir: []const u8, operation: []const u8, data: []const u8) void {
    const path = std.fmt.allocPrint(gpa, "{s}/dump_zig_ctoon_{s}.frames", .{ dir, operation }) catch return;
    std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = data }) catch {};
}

fn dumpOutputs(gpa: std.mem.Allocator, results_path: []const u8, files: []const BenchFile) void {
    const dir = if (std.mem.lastIndexOfScalar(u8, results_path, '/')) |i| results_path[0..i] else ".";
    var jt: std.Io.Writer.Allocating = .init(gpa); // json_to_toon
    var tj: std.Io.Writer.Allocating = .init(gpa); // toon_to_json
    var rt: std.Io.Writer.Allocating = .init(gpa); // roundtrip
    var ts: std.Io.Writer.Allocating = .init(gpa); // toon_to_struct (value dumped as JSON, untimed)
    var st: std.Io.Writer.Allocating = .init(gpa); // struct_to_toon
    defer ts.deinit();
    defer st.deinit();
    defer jt.deinit();
    defer tj.deinit();
    defer rt.deinit();

    for (files, 0..) |f, i| {
        // json_to_toon / toon_to_json / roundtrip: ctoon's one-shot text
        // conversion functions (no ctoon.Value in between).
        if (ctoon.jsonToToon(gpa, f.json, .{})) |toon| {
            frame(&jt.writer, i, toon);
            if (ctoon.toonToJson(gpa, toon, 0)) |js| frame(&rt.writer, i, js) else |_| {}
        } else |_| {}

        if (f.toon) |toon_text| {
            if (ctoon.toonToJson(gpa, toon_text, 0)) |js| frame(&tj.writer, i, js) else |_| {}
            // toon_to_struct: the decoded value, serialised to JSON
            // (untimed) only so the verifier can compare it to the
            // original document.
            if (ctoon.loads(gpa, toon_text)) |val| {
                if (ctoon.dumpsJson(gpa, val, 2)) |js| frame(&ts.writer, i, js) else |_| {}
            } else |_| {}
        }

        if (f.value) |v| {
            if (ctoon.dumps(gpa, v)) |toon| frame(&st.writer, i, toon) else |_| {}
        }
    }
    writeFrames(gpa, dir, "json_to_toon", jt.written());
    writeFrames(gpa, dir, "toon_to_json", tj.written());
    writeFrames(gpa, dir, "roundtrip", rt.written());
    writeFrames(gpa, dir, "toon_to_struct", ts.written());
    writeFrames(gpa, dir, "struct_to_toon", st.written());
}

// ------------------------------------------------------------ order check --
// Key order, one direction at a time, against the shared samples in
// benchmarks/order_check/ (sample.json / sample.toon: non-alphabetical keys,
// a nested object, a tabular array):
//   json_to_toon -- keys of the TOON output must follow the order of sample.json
//   toon_to_json -- keys of the JSON output must follow the order of sample.toon
// Both reduce to "first-occurrence order of the keys", so the tabular header
// `list[2]{z,a}:` and the expanded `- z: 1` form compare equal. Hand-written
// scanners, no regex. The samples are read relative to the working directory
// (benchmarks/zig/ctoon, which is where CMake runs this).
const Keys = std.ArrayList([]const u8);

const OrderRow = struct {
    library: []const u8,
    operation: []const u8,
    preserved: bool,
};

fn addKey(gpa: std.mem.Allocator, keys: *Keys, raw: []const u8) !void {
    var k = std.mem.trim(u8, raw, " \t");
    if (k.len >= 2 and k[0] == '"' and k[k.len - 1] == '"') k = k[1 .. k.len - 1];
    if (k.len == 0) return;
    for (keys.items) |x| if (std.mem.eql(u8, x, k)) return;
    try keys.append(gpa, k);
}

/// Every `"key":` in a JSON text, first occurrence only.
fn jsonKeys(gpa: std.mem.Allocator, text: []const u8) !Keys {
    var keys: Keys = .empty;
    var i: usize = 0;
    while (i < text.len) {
        if (text[i] != '"') {
            i += 1;
            continue;
        }
        const start = i + 1;
        var q = start;
        while (q < text.len and text[q] != '"') q += if (text[q] == '\\' and q + 1 < text.len) @as(usize, 2) else 1;
        if (q >= text.len) break;
        var r = q + 1;
        while (r < text.len and std.ascii.isWhitespace(text[r])) r += 1;
        if (r < text.len and text[r] == ':') try addKey(gpa, &keys, text[start..q]);
        i = q + 1;
    }
    return keys;
}

/// Line by line: strip indent and `- ` list markers, then `key:`, `key[N]:`
/// or `key[N]{f1,f2}:` -- the key and, for the tabular form, the fields.
fn toonKeys(gpa: std.mem.Allocator, text: []const u8) !Keys {
    var keys: Keys = .empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |line| {
        const end = line.len;
        var s: usize = 0;
        while (s < end and line[s] == ' ') s += 1;
        while (s + 1 < end and line[s] == '-' and line[s + 1] == ' ') {
            s += 2;
            while (s < end and line[s] == ' ') s += 1;
        }
        var q = s;
        if (q < end and line[q] == '"') {
            q += 1;
            while (q < end and line[q] != '"') q += if (line[q] == '\\' and q + 1 < end) @as(usize, 2) else 1;
            if (q < end) q += 1;
        } else {
            while (q < end and std.mem.indexOfScalar(u8, " \t:[{,\"", line[q]) == null) q += 1;
        }
        const key_end = q;
        var fields: ?[]const u8 = null;
        if (q < end and line[q] == '[') {
            while (q < end and line[q] != ']') q += 1;
            if (q < end) q += 1;
        }
        if (q < end and line[q] == '{') {
            const f = q + 1;
            while (q < end and line[q] != '}') q += 1;
            fields = line[f..q];
            if (q < end) q += 1;
        }
        if (q < end and line[q] == ':' and key_end > s) {
            try addKey(gpa, &keys, line[s..key_end]);
            if (fields) |fs| {
                var parts = std.mem.splitAny(u8, fs, ",|\t");
                while (parts.next()) |part| try addKey(gpa, &keys, part);
            }
        }
    }
    return keys;
}

fn recordOrder(gpa: std.mem.Allocator, rows: *std.ArrayList(OrderRow), library: []const u8, operation: []const u8, expected: []const []const u8, got: []const []const u8) !void {
    var same = expected.len == got.len;
    if (same) {
        for (expected, got) |a, b| {
            if (!std.mem.eql(u8, a, b)) {
                same = false;
                break;
            }
        }
    }
    try rows.append(gpa, .{ .library = library, .operation = operation, .preserved = same });
}

fn recordOrderFailed(gpa: std.mem.Allocator, rows: *std.ArrayList(OrderRow), library: []const u8, operation: []const u8, err: anyerror) !void {
    _ = err; // any failure is a failed check
    try rows.append(gpa, .{ .library = library, .operation = operation, .preserved = false });
}

fn runOrderChecks(gpa: std.mem.Allocator, rows: *std.ArrayList(OrderRow)) !void {
    const cwd = std.Io.Dir.cwd();
    const sample_json = cwd.readFileAlloc(io, "../../order_check/sample.json", gpa, .limited(1 << 20)) catch {
        std.debug.print("warning: order-check samples not found in ../../order_check\n", .{});
        return;
    };
    const sample_toon = cwd.readFileAlloc(io, "../../order_check/sample.toon", gpa, .limited(1 << 20)) catch {
        std.debug.print("warning: order-check samples not found in ../../order_check\n", .{});
        return;
    };
    const expected = try jsonKeys(gpa, sample_json);

    if (ctoon.jsonToToon(gpa, sample_json, .{})) |toon| {
        const got = try toonKeys(gpa, toon);
        try recordOrder(gpa, rows, "ctoon", "json_to_toon", expected.items, got.items);
    } else |err| try recordOrderFailed(gpa, rows, "ctoon", "json_to_toon", err);

    if (ctoon.toonToJson(gpa, sample_toon, 0)) |js| {
        const got = try jsonKeys(gpa, js);
        try recordOrder(gpa, rows, "ctoon", "toon_to_json", expected.items, got.items);
    } else |err| try recordOrderFailed(gpa, rows, "ctoon", "toon_to_json", err);
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

    std.debug.print("CToon Benchmarks - Zig\n", .{});
    std.debug.print("Corpus: {} files, {d:.2} MB (JSON)\n\n", .{ files.items.len, @as(f64, @floatFromInt(total_json_bytes)) / 1e6 });
    std.debug.print("{s:<8} {s:<14} {s:>12} {s:>14} {s:>9} {s:>10} {s:>12}\n", .{ "Library", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time" });

    // Untimed pre-pass: TOON text with ctoon, shared decode input.
    for (files.items) |*f| {
        const val = ctoon.loadsJson(gpa, f.json) catch {
            logFail("ctoon", "pre_pass", f.path, ctoon.Error.ParseError);
            continue;
        };
        f.value = val;
        f.toon = ctoon.dumps(gpa, val) catch {
            logFail("ctoon", "pre_pass", f.path, ctoon.Error.WriteError);
            continue;
        };
    }

    // Outputs for report/verify_outputs.mjs live next to the results JSON.
    dumpOutputs(gpa, results_path, files.items);

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
                if (ctoon.jsonToToon(gpa, f.json, .{})) |out| {
                    gpa.free(out);
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
                if (ctoon.toonToJson(gpa, toon_text, 0)) |out| {
                    gpa.free(out);
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
                const toon_text = ctoon.jsonToToon(gpa, f.json, .{}) catch |err| {
                    if (r == 0) logFail("ctoon", "roundtrip", f.path, err);
                    continue;
                };
                defer gpa.free(toon_text);
                if (ctoon.toonToJson(gpa, toon_text, 0)) |out| {
                    gpa.free(out);
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

    // toon_to_struct: decode the pre-pass TOON into a ctoon.Value, nothing else.
    {
        var ops: u64 = 0;
        var bytes: f64 = 0;
        const timer_start = std.Io.Clock.awake.now(io);
        var r: u32 = 0;
        while (r < repeats) : (r += 1) {
            for (files.items) |f| {
                const toon_text = f.toon orelse continue;
                if (ctoon.loads(gpa, toon_text)) |_| {
                    ops += 1;
                    bytes += @floatFromInt(toon_text.len);
                } else |err| {
                    if (r == 0) logFail("ctoon", "toon_to_struct", f.path, err);
                }
            }
        }
        const seconds = @as(f64, @floatFromInt(timer_start.durationTo(std.Io.Clock.awake.now(io)).nanoseconds)) / 1e9;
        try record(gpa, &results, "ctoon", "toon_to_struct", bytes, ops, seconds, total);
    }

    // struct_to_toon: encode the pre-built ctoon.Value, nothing else (weighted
    // by the JSON size, same convention as json_to_toon).
    {
        var ops: u64 = 0;
        var bytes: f64 = 0;
        const timer_start = std.Io.Clock.awake.now(io);
        var r: u32 = 0;
        while (r < repeats) : (r += 1) {
            for (files.items) |f| {
                const v = f.value orelse continue;
                if (ctoon.dumps(gpa, v)) |_| {
                    ops += 1;
                    bytes += @floatFromInt(f.json.len);
                } else |err| {
                    if (r == 0) logFail("ctoon", "struct_to_toon", f.path, err);
                }
            }
        }
        const seconds = @as(f64, @floatFromInt(timer_start.durationTo(std.Io.Clock.awake.now(io)).nanoseconds)) / 1e9;
        try record(gpa, &results, "ctoon", "struct_to_toon", bytes, ops, seconds, total);
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
    try w.print("  ],\n  \"order_check\": [\n", .{});
    var order_rows: std.ArrayList(OrderRow) = .empty;
    runOrderChecks(gpa, &order_rows) catch |err| std.debug.print("warning: key-order check failed: {}\n", .{err});
    for (order_rows.items, 0..) |r, i| {
        try w.print(
            "    {{\"library\": \"{s}\", \"operation\": \"{s}\", \"preserved\": {}}}{s}\n",
            .{ r.library, r.operation, r.preserved, if (i + 1 < order_rows.items.len) "," else "" },
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
