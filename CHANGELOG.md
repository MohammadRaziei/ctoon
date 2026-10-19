# Changelog

All notable changes to CToon are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

The version lives in `include/ctoon.h`; `python version.py` keeps `Cargo.toml`,
`build.zig.zon`, the Julia `Project.toml` and the install snippets in
`README.md` in sync with it.

## [Unreleased]

## [0.9.0] - 2026-10-19

### Added

- **TOON spec 4.4 support** (`v4.4.0`, 2026-10-08). Previous releases targeted
  spec 4.1 (`v4.1.2`).
- `CTOON_SPEC_PIN` CMake option to pin the spec version to the committed
  `supported_spec.conf`, and strict pinned-spec builds in the CMake CI
  workflows.
- GNU Octave support: build, documentation and benchmark.
- `ctoon.manager` (Python): `config` command for manager settings, a listing
  command (`listing.py`), CLI parsing that accepts mixed commands and language
  subcommands, OS-aware MATLAB detection, and a `release-list` workflow job.
- CI: MATLAB build and test workflow, MATLAB release job in the orchestrator,
  Octave job via `mex.yml`, and a C++ packaging workflow integrated into
  releases. C and C++ release artifacts are now built separately.
- Docs: "What is CToon?" overview section, a hero callout on the home page that
  links to the benchmark report, and a `Home` / `Benchmarks` navigation bar on
  every page, including the Python (Sphinx) docs.
- Benchmarks: GNU Octave benchmark, native Julia value to TOON benchmark, and
  Zig results in the report.

### Changed

- Spec updated from 4.1.2 to 4.4.0, with a refactor of the parser state.
- The supported spec version is read from `supported_spec.conf` instead of
  being hardcoded.
- Faster Python value conversion on the hot path.
- Python binding requirements are checked through pip requirements instead of
  CMake conditionals, with a requirement check for non-scikit-build builds.
- Install snippets for C and C++ (docs site and Doxygen main pages) pin
  `GIT_TAG v<version>` instead of tracking a branch. The version is stamped in
  at docs build time.
- `README.md` install snippets (CMake `GIT_TAG` and the Cargo dependency line)
  are rewritten by `version.py` on every version bump, so they cannot go
  stale. `version.py` stops with an error if it can no longer find them.
- Rust install instructions use the crates.io release (`cargo add ctoon`)
  instead of a git dependency.
- MEX tests moved from the build to a separate test job; Windows build matrix
  and MinGW setup updated for MEX builds.
- CI workflows use native `actions/upload-artifact` instead of the
  `Wandalen/wretry` wrapper, with retry logic added where needed.
- Docs footer links point to GitHub issues for bug reports; the contributing
  guide's new-language checklist was streamlined.
- The benchmark report was restyled to match the docs site (same top bar, hero,
  tables and footer, light and dark theme shared with the docs). Charts keep
  their types, shapes and order.
- Benchmarks: C++ scaling support removed and report generation simplified.
  Benchmarks were rerun for all languages, with Zig included. The Zig
  benchmark accepts the `python-zig` executable from pip's `ziglang` wheel and
  requires Zig 0.16.x.

### Fixed

- Resource leaks in the HTTP client and test server used by `ctoon.manager`.
- NUL handling in native JSON output; the benchmark now uses ctoon's own parser
  for its input.
- Home page statistics wrapped awkwardly on phones and left a stray divider;
  they now form a 2x2 grid on narrow screens.
- The `FetchContent_Declare` example in `README.md` had its arguments on one
  line and pointed at a branch that does not exist.
- Benchmark report chart grid lines stayed dark after switching to the light
  theme.

## [0.8.3] - 2026-10-03

### Added

- Spec tag and date are exposed in all language bindings.
- TOON spec version documentation for every language.
- README badges for the TOON spec version and the online documentation.

### Changed

- Unified the spec version write logic and the `CTOON_SPEC` macro generation.
- `CTOON_VERSION_STR` macro replaces the macro-based version string.
- `SpecVersion.cmake` unifies header updates and generates the README badge.

## [0.8.2] - 2026-10-02

### Added

- `CTOON_READ_NON_STRICT` read flag and the Python `strict=` keyword.
- Standalone CLI; Julia, Rust and Zig support in the docs; Go and Rust builds in
  the CI pipeline.
- Documentation comparing TOON and JSON, with an overview section.
- Linux aarch64 wheels, built on a native ARM runner.
- Spec macros are generated and updated automatically in the header.

### Changed

- Quoted-string escaping is unified between the TOON and JSON parsers, per
  spec 4.1.
- Locale-dependent `strtod` replaced with a custom numeric check for strings.
- Language lists in the docs are generated from a registry.
- Upgraded to scikit-build-core 1.0; refactored the Julia CI tests.
- Benchmark documentation refined; Python scope limited for the scaling and
  memory tests.

## [0.8.1] - 2026-09-28

### Added

- `CONTRIBUTING.md` with CMake-based build guidelines and multi-language
  binding documentation.
- MIT license file.

### Changed

- Python build configuration updated; the release command in `version.py` was
  made more robust (empty commit message, exception handling, return status).

## [0.8.0] - 2026-09-28

### Added

- Julia language binding, with Documenter.jl documentation, CI jobs and an
  optional benchmark. Core sources are fetched locally or from GitHub at build
  time.
- Benchmarks: standalone HTML report, peak RSS measurement, scaling line
  charts, key order preservation checks, and a Node.js benchmark for
  `@toon-format/toon`.
- `SpecVersion.cmake` for centralized TOON spec version resolution, with spec
  fixtures fetched once for all bindings.
- Native JSON round-trip tests for the language bindings, with numeric-tolerant
  equality.
- Minimum toolchain version checks for MATLAB, Go, Julia, Rust and Zig, and
  `ctoon_lang_wanted()` to keep configure time low for optional bindings.
- Optional TOON spec version field in the docs hero stats.
- MATLAB: support for non-scalar logical arrays in the MEX binding.

### Changed

- Project build options now default to OFF.
- Language list format in the docs switched from txt to json.
- Rust: `serde_json` `preserve_order` enabled to keep JSON key order.
- Zig tests and benchmarks updated for Zig 0.16 API changes.
- MATLAB tests source their expected TOON output from the spec fixtures.

### Fixed

- Julia: unsigned underflow when deserializing empty arrays, parse errors now
  reported as `ReadErr`, const mismatch in the shim, FFI return types.
- MATLAB: struct arrays are encoded as JSON arrays of objects, and NaN produced
  by `jsondecode` for null round-trips back to null.
- Zig benchmark: libc resolution, search prefixes and `LD_LIBRARY_PATH`.
- Rust coverage HTML link path.

## [0.7.0] - 2026-09-14

### Added

- Rust: configurable serialization options.

### Changed

- JSON support is optional via `CTOON_DISABLE_JSON`.

## [0.6.2] - 2026-09-14

### Added

- Zig binding, with README, source, build and tests.
- Rust crate packaging, verification and publishing.
- CI: Zig tests and cross-platform release artifacts.

### Changed

- `Cargo.toml` and `build.zig.zon` versions are synced with the C header.
- Zig updated to 0.16.0; Zig benchmarks integrated.
- Redundant allocator parameter removed from internal `deinit` calls.

### Fixed

- Zig: string ownership in `Value.deinit`; UBSan disabled for the C sources.
- CI: concurrency groups stabilized across workflows; macOS runner issues with
  the Zig linker.

## [0.6.1] - 2026-09-10

### Added

- Rust bindings.
- Round-trip benchmark to measure library self-consistency; Node.js benchmark
  support.

## [0.6.0] - 2026-09-05

### Added

- `encoding/json`-compatible Marshal and Unmarshal functions (Go).

### Changed

- Go: `bridge.h` added and encoding/decoding optimized.
- Benchmarks are fully standalone, with improved documentation.

## [0.5.2] - 2026-09-03

### Added

- Standalone CMake project for the benchmarks, with a regression test.

### Fixed

- Escaped characters in the keyed tabular format.
- Go: improved `Decode` and `LoadsJSON`.

## [0.5.1] - 2026-08-31

### Added

- C API documentation generated with Doxygen, and C and C++ API docs with the
  awesome-css theme.
- `Python::SABIModule` added to the `Development.Module` dependencies.

### Changed

- `write()` renamed to `to_string()` for clarity.

## [0.5.0] - 2026-08-31

### Added

- Spec conformance tests.
- Custom indent size when parsing TOON (`ctoon_read_opts_indent`), with an
  `indent` parameter for Python `loads` and `load`.
- Last-write-wins deduplication in non-strict mode.
- Tabular header field trees and improved container handling.
- CLI accepts empty TOON input for decoding.
- Go: input validation in `Loads`.

### Changed

- Improved string quoting, number formatting and number parsing, and number
  equality comparison.

## [0.4.0] - 2026-05-19

### Added

- MATLAB bindings: buildtool plan, `ctoon_install` script, TOON
  encode/decode/read, `dump`/`dumps` and `load`/`loads` helpers, and LCOV
  coverage reports for R2024a.
- MATLAB API documentation.

### Changed

- MATLAB build and test targets streamlined for CI; MEX target now uses
  `matlab_add_mex`.

### Fixed

- MATLAB: string fields now use `ctoon_mut_strcpy`.

## [0.3.0] - 2026-05-15

### Added

- MATLAB MEX binding, with CMake build integration and tests.
- MATLAB binding documentation generation.

### Fixed

- Docs theme topbar brand link; `<section>` replaced with `<div>` to avoid CSS
  conflicts.

## [0.2.2] - 2026-05-10

### Added

- Markdown-based documentation site generation (`CToonPages` CMake module),
  CLI reference tab, favicon and custom CSS support.
- Go-style LCOV support in the coverage dashboard.

### Fixed

- LCOV parsing of Go-style duplicate `DA` entries.
- Documentation links and coverage navbar link.

## [0.2.1] - 2026-05-05

### Added

- `TECHNICAL.md` covering CToon internals.
- Release orchestrator workflow, PyPI publishing, and musl Linux wheel badges
  in the release template.
- GitHub Pages deployment.

### Changed

- Redesigned the docs landing page with a modern theme and dark mode; docs use
  external CSS instead of SVG.
- CI workflows modularized; GitHub Actions upgraded.

## [0.2.0] - 2026-05-03

First tagged release.

### Added

- C99 core library (`ctoon.h`, `ctoon.c`) for reading and writing TOON and
  JSON, with a built-in JSON reader.
- C++ wrapper (`ctoon.hpp`) with builder-style write options and
  `string_view` overloads.
- `ctoon` command-line tool.
- Python bindings built on nanobind, published as wheels with cibuildwheel.
- Go bindings.
- Documentation system with Doxygen and Sphinx, and a coverage dashboard.
- CI for build, test, docs and coverage on Linux, macOS and Windows.
- Version management script (`version.py`).

[Unreleased]: https://github.com/MohammadRaziei/ctoon/compare/v0.9.0...HEAD
[0.9.0]: https://github.com/MohammadRaziei/ctoon/compare/v0.8.3...v0.9.0
[0.8.3]: https://github.com/MohammadRaziei/ctoon/compare/v0.8.2...v0.8.3
[0.8.2]: https://github.com/MohammadRaziei/ctoon/compare/v0.8.1...v0.8.2
[0.8.1]: https://github.com/MohammadRaziei/ctoon/compare/v0.8.0...v0.8.1
[0.8.0]: https://github.com/MohammadRaziei/ctoon/compare/v0.7.0...v0.8.0
[0.7.0]: https://github.com/MohammadRaziei/ctoon/compare/v0.6.2...v0.7.0
[0.6.2]: https://github.com/MohammadRaziei/ctoon/compare/v0.6.1...v0.6.2
[0.6.1]: https://github.com/MohammadRaziei/ctoon/compare/v0.6.0...v0.6.1
[0.6.0]: https://github.com/MohammadRaziei/ctoon/compare/v0.5.2...v0.6.0
[0.5.2]: https://github.com/MohammadRaziei/ctoon/compare/v0.5.1...v0.5.2
[0.5.1]: https://github.com/MohammadRaziei/ctoon/compare/v0.5.0...v0.5.1
[0.5.0]: https://github.com/MohammadRaziei/ctoon/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/MohammadRaziei/ctoon/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/MohammadRaziei/ctoon/compare/v0.2.2...v0.3.0
[0.2.2]: https://github.com/MohammadRaziei/ctoon/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/MohammadRaziei/ctoon/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/MohammadRaziei/ctoon/releases/tag/v0.2.0
