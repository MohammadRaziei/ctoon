// CToon Zig benchmark — standalone build, no relationship to the repo
// root's own build.zig; fetches ctoon and toon-zig as real dependencies
// (see build.zig.zon) the same way this directory's Go/Rust benchmarks
// fetch their own peers from their own package ecosystems. Targets Zig
// 0.16.0 — see the repo root's build.zig for notes on what changed from
// 0.13 across the 0.14/0.16 build-system API.
const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    const ctoon_dep = b.dependency("ctoon", .{ .target = target, .optimize = optimize });
    const toon_dep = b.dependency("toon", .{ .target = target, .optimize = optimize });

    const exe = b.addExecutable(.{
        .name = "ctoon-bench-zig",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    exe.root_module.addImport("ctoon", ctoon_dep.module("ctoon"));
    exe.root_module.addImport("toon", toon_dep.module("toon"));
    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| run_cmd.addArgs(args);

    const run_step = b.step("run", "Run the ctoon Zig benchmark");
    run_step.dependOn(&run_cmd.step);
}
