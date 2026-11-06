// toon-zig (github.com/LatentEvals/toon-zig) benchmark - a standalone
// project, separate from ../ctoon on purpose: toon-zig declares
// minimum_zig_version 0.15.2 while ../ctoon is built with Zig 0.16.x,
// and one executable cannot be compiled by two compilers. Each gets its own
// pinned `ziglang` venv, see ../ZigToolchain.cmake. This one is built
// with Zig 0.15.2.
const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    const toon_dep = b.dependency("toon", .{ .target = target, .optimize = optimize });

    const exe = b.addExecutable(.{
        .name = "toon-zig-bench",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    exe.root_module.addImport("toon", toon_dep.module("toon"));
    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| run_cmd.addArgs(args);

    const run_step = b.step("run", "Run the toon-zig benchmark");
    run_step.dependOn(&run_cmd.step);
}
