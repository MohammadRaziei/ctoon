// CToon Zig benchmark — standalone build, no relationship to the repo
// root's own build.zig; fetches ctoon as a real dependency (see
// build.zig.zon) the same way this directory's Go/Rust benchmarks fetch
// their own peers from their own package ecosystems. Targets Zig 0.16.0
// — see the repo root's build.zig for notes on what changed from 0.13
// across the 0.14/0.16 build-system API.
//
// toon-zig (github.com/LatentEvals/toon-zig) is NOT currently a
// dependency, despite being the only pure-Zig TOON peer found — its
// decoder.zig calls std.json.ObjectMap.init(allocator), the pre-0.16
// single-arg form; Zig 0.16's ArrayHashMap-based ObjectMap needs
// `.init(gpa, key_list, value_list)` or the zero-value `.{}` unmanaged
// form instead, so it fails to compile under 0.16. This is upstream's
// issue to fix, not something to patch around here (this directory only
// ever fetches real dependencies — see its CMakeLists.txt). Revisit
// once toon-zig supports 0.16; main.zig kept the toon-zig benchmarking
// code commented out for the same reason — see its own header comment.
const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    const ctoon_dep = b.dependency("ctoon", .{ .target = target, .optimize = optimize });

    const exe = b.addExecutable(.{
        .name = "ctoon-bench-zig",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    exe.root_module.addImport("ctoon", ctoon_dep.module("ctoon"));
    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| run_cmd.addArgs(args);

    const run_step = b.step("run", "Run the ctoon Zig benchmark");
    run_step.dependOn(&run_cmd.step);
}
