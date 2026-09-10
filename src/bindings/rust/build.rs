// Compiles the C core directly into this crate — the same approach the Go
// binding takes (its cgo preamble does `#include "ctoon.c"`). No CMake
// step is required to build this binding; `cargo build`/`cargo test` are
// fully self-contained, exactly like `go build`/`go test` are for Go.

fn main() {
    let manifest_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    // src/bindings/rust -> repo root is three levels up.
    let repo_root = std::path::Path::new(&manifest_dir)
        .join("../../..")
        .canonicalize()
        .expect("could not resolve repo root from CARGO_MANIFEST_DIR");
    let include_dir = repo_root.join("include");
    let src_dir = repo_root.join("src");
    let src_file = src_dir.join("ctoon.c");

    let shim_file = std::path::Path::new(&manifest_dir).join("shim.c");

    cc::Build::new()
        .file(&src_file)
        .file(&shim_file)
        .include(&include_dir)
        .include(&src_dir)
        .define("CTOON_ENABLE_JSON", "1")
        .opt_level(3)
        .warnings(false) // not our warnings to fix — same call ctoon's other bindings make
        .compile("ctoon");

    println!("cargo:rerun-if-changed={}", src_file.display());
    println!("cargo:rerun-if-changed={}", shim_file.display());
    println!("cargo:rerun-if-changed={}", include_dir.join("ctoon.h").display());
}
