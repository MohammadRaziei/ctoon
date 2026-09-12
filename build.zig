// Build script for the Zig bindings to ctoon.
//
// Targets Zig 0.16.0. Two structural changes from 0.13 matter here:
//   1. (0.14+) A Compile step (library/executable/test) no longer takes
//      root_source_file, target, and optimize directly — those live on
//      a *Module* now, built with b.createModule() and passed in as
//      .root_module. addStaticLibrary was replaced by
//      addLibrary(.{ .linkage = .static }).
//   2. (0.16) Compile's convenience wrappers that used to forward to its
//      root_module — addIncludePath, addCSourceFile, linkLibrary — were
//      removed. Those must be called on `<compile>.root_module` directly
//      now, not on the Compile step itself. installHeadersDirectory and
//      installArtifact are unaffected — those are genuinely install-step
//      (not module) operations.
//
// Lives at the repo root (like Cargo.toml and pyproject.toml) so
// `zig build test` works from the repo root the same way `cargo test`
// and `pip install .` do — but the binding's own source and shim stay
// under their designated location, src/bindings/zig/ (mirrors
// src/bindings/rust/ for the Rust binding).
//
// Compiles the C core directly into this binding — the same approach the
// Rust binding's build.rs and the Go binding's cgo preamble take. No CMake
// step is required: `zig build test` (or `zig build` from a downstream
// project that depends on this as a module) is fully self-contained.
//
// Plain `zig build` (no step name) additionally installs the compiled
// static library and headers under zig-out/ — see .github/workflows/zig.yml,
// which cross-compiles that for a release the same way wheels.yml builds
// platform wheels: Zig has no central *binary* package registry the way
// PyPI or crates.io do (Zig packages are fetched as source, by URL+hash),
// so a prebuilt zig-out/ tarball attached to the GitHub release is the
// closest equivalent for people who don't want to build ctoon.c themselves.
const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const include_dir = "include";
    const src_dir = "src";
    const ctoon_c = "src/ctoon.c";
    const binding_dir = "src/bindings/zig";
    const shim_c = binding_dir ++ "/shim.c";

    const ctoon_lib = b.addLibrary(.{
        .name = "ctoon",
        .linkage = .static,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    ctoon_lib.root_module.addIncludePath(b.path(include_dir));
    ctoon_lib.root_module.addIncludePath(b.path(src_dir));
    ctoon_lib.root_module.addCSourceFile(.{
        .file = b.path(ctoon_c),
        // -fno-sanitize=undefined: Zig's C frontend enables UBSan traps by
        // default for C sources in Debug/ReleaseSafe, which the other
        // bindings' build systems (CMake, cc-rs, cgo) don't turn on for
        // this same ctoon.c. Without this flag, `zig build test` SIGILLs
        // inside otherwise-correct, widely-exercised code (e.g.
        // ctoon_write_indent's pointer-at-end-of-buffer arithmetic) that
        // every other binding compiles and runs fine.
        .flags = &.{ "-DCTOON_ENABLE_JSON=1", "-fno-sanitize=undefined" },
    });
    // shim.c gives the `static inline` parts of ctoon.h's API (tree
    // inspection, mutable-document building) real extern linkage, under
    // their `ctoon_rs_`-prefixed names — see shim.c's header comment.
    // Same file the Rust binding uses (src/bindings/rust/shim.c).
    ctoon_lib.root_module.addCSourceFile(.{
        .file = b.path(shim_c),
        .flags = &.{ "-DCTOON_ENABLE_JSON=1", "-fno-sanitize=undefined" },
    });
    ctoon_lib.installHeadersDirectory(b.path(include_dir), "", .{});
    b.installArtifact(ctoon_lib);

    // Public module: `const ctoon = @import("ctoon");`
    const ctoon_module = b.addModule("ctoon", .{
        .root_source_file = b.path(binding_dir ++ "/src/ctoon.zig"),
        .target = target,
        .optimize = optimize,
    });
    ctoon_module.addIncludePath(b.path(include_dir));
    ctoon_module.linkLibrary(ctoon_lib);

    // `zig build test` — unit tests in src/bindings/zig/src/ctoon.zig
    // plus the shared cross-binding integration test in
    // tests/zig/integration_test.zig (mirrors tests/rust, tests/go, etc,
    // all likewise centralized under the repo-root tests/ folder).
    const unit_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path(binding_dir ++ "/src/ctoon.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    unit_tests.root_module.linkLibrary(ctoon_lib);
    unit_tests.root_module.addIncludePath(b.path(include_dir));

    const integration_tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("tests/zig/integration_test.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });
    integration_tests.root_module.linkLibrary(ctoon_lib);
    integration_tests.root_module.addIncludePath(b.path(include_dir));
    integration_tests.root_module.addImport("ctoon", ctoon_module);

    const run_unit_tests = b.addRunArtifact(unit_tests);
    const run_integration_tests = b.addRunArtifact(integration_tests);

    const test_step = b.step("test", "Run ctoon Zig binding tests");
    test_step.dependOn(&run_unit_tests.step);
    test_step.dependOn(&run_integration_tests.step);
}
