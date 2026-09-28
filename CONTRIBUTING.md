# Contributing to CToon

Thanks for your interest in CToon! This document explains how to propose a
change, how the build system works, and what you must run before opening a
pull request.

CToon is a C99 core library (`src/ctoon.c`, `include/ctoon.h`) with bindings
for C++, Python, Go, Rust, Zig, Julia and MATLAB, plus a CLI. **CMake is the
single entry point for building, testing, coverage and documentation across
all of them** — you do not need to learn each language's tooling separately
to check your work.

---

## Table of contents

1. [Workflow: issue first, then fork](#1-workflow-issue-first-then-fork)
2. [Repository layout](#2-repository-layout)
3. [Prerequisites](#3-prerequisites)
4. [The CMake workflow](#4-the-cmake-workflow)
   - [Configure options](#41-configure-options)
   - [Building](#42-building)
   - [Running tests](#43-running-tests)
   - [Coverage](#44-coverage)
   - [Documentation](#45-documentation)
   - [The TOON spec dependency](#46-the-toon-spec-dependency)
5. [Editing or adding a feature to an existing language](#5-editing-or-adding-a-feature-to-an-existing-language)
6. [Adding a new language binding](#6-adding-a-new-language-binding)
7. [Continuous integration](#7-continuous-integration)
8. [Versioning](#8-versioning)
9. [Pull request checklist](#9-pull-request-checklist)
10. [License](#10-license)

---

## 1. Workflow: issue first, then fork

**Always open an issue before you start working — and only then fork.**

1. **Open an issue** describing the bug, feature or change you have in mind.
   Search existing issues first to avoid duplicates.
2. **Wait for feedback.** For small fixes this is usually quick; for anything
   larger (new API, behavior change, new language) please wait for a
   maintainer to agree on the approach. This saves you from writing a change
   that can't be merged.
3. **Fork** the repository and create a branch from `master`
   (e.g. `fix/rust-real-roundtrip` or `feat/go-file-api`).
4. Make your change, run the [CMake checks](#4-the-cmake-workflow) below, and
   open a pull request that references the issue (`Fixes #123`).

Pull requests without a linked issue may be closed and redirected to the
issue tracker first.

---

## 2. Repository layout

| Path | What lives there |
|------|------------------|
| `src/ctoon.c`, `include/ctoon.h` | The C core. The library version is defined in `ctoon.h`. |
| `src/bindings/<lang>/` | One folder per language binding (`cpp`, `python`, `go`, `rust`, `zig`, `julia`, `matlab`). |
| `src/cli/` | The `ctoon` command-line tool. |
| `tests/<lang>/` | Tests for each language. **All languages' tests live under the root `tests/` folder**, each with its own `CMakeLists.txt`. |
| `tests/data/` | Shared paired `*.json` / `*.toon` sample files used by round-trip tests. |
| `docs/` | Documentation sources and the docs build (`docs/<lang>/`, `supported_langs.json`, `index.html.in`). |
| `benchmarks/` | A standalone CMake project comparing implementations. |
| `cmake/` | Shared CMake modules (language guard, coverage helpers, versioning, spec version). |
| `.github/workflows/` | CI: `cmake.yml`, per-language workflows (`crates.yml`, `wheels.yml`, `zig.yml`) and `orchestrator.yml`. |
| `Cargo.toml`, `pyproject.toml`, `go.mod`, `build.zig(.zon)` | Package manifests live at the repo root; the binding sources they point to live under `src/bindings/`. |

---

## 3. Prerequisites

Required for everything:

- A C99/C++ compiler (GCC, Clang, MSVC)
- **CMake ≥ 3.19**
- **Python 3** with the dev dependencies: `pip install -r requirements-dev.txt`
- Network access at configure time the first time (CMake fetches the
  [toon-format/spec](https://github.com/toon-format/spec) test fixtures)

Per language — only what you touch is required unless you opt in to
everything (see [Configure options](#41-configure-options)):

| Language | Toolchain |
|----------|-----------|
| Python | Python ≥ 3.9 build deps from `requirements-dev.txt` (scikit-build-core, nanobind, pytest, pytest-cov) |
| Go | Go ≥ 1.21 (uses CGo, so a C compiler is needed) |
| Rust | Rust ≥ 1.70 (`cargo`) |
| Zig | Zig 0.16.0 |
| Julia | Julia 1.x and a C compiler |
| MATLAB | MATLAB with a MEX-capable compiler |

For coverage: `lcov` + `genhtml`, `cargo-llvm-cov` (Rust, plus
`rustup component add llvm-tools-preview`), and the `Coverage` Julia package.
For docs: Doxygen + Graphviz (C/C++), Sphinx (Python, from
`requirements-dev.txt`), and `markdown-it-py`.

---

## 4. The CMake workflow

### 4.1 Configure options

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

| Option | Default | Meaning |
|--------|---------|---------|
| `CTOON_BUILD_TESTS` | ON (top-level) | Build and register tests for every language. |
| `CTOON_BUILD_EXAMPLES` | ON (top-level) | Build the examples. |
| `CTOON_BUILD_DOCS` | OFF | Enable the documentation targets. |
| `CTOON_BUILD_PYTHON` | OFF | Build the Python extension (nanobind) through CMake. |
| `CTOON_BUILD_GO` / `_RUST` / `_ZIG` / `_JULIA` / `_MATLAB` | OFF | **Require** that language's toolchain. |
| `CTOON_BUILD_ALL_LANGS` | OFF | Require *every* language's toolchain. |

**How the per-language flags actually work.** Go, Rust, Zig, Julia and MATLAB
are compiled by their *own* toolchains (`go`, `cargo`, `zig build`, Julia
`Pkg`, MEX) — not by CMake's own build graph. The `CTOON_BUILD_<LANG>` flags
therefore don't add a build step; they control what `tests/` and `docs/` do
when they look for that language:

- **Flag OFF (default):** the language is skipped entirely — no toolchain
  probe, no noise. Handy on a machine that only has some toolchains.
- **Flag ON (or `CTOON_BUILD_ALL_LANGS=ON`):** you have explicitly asked for
  that language, so a missing toolchain is a **hard configure error**
  instead of a silent skip.

> ⚠️ Because of the default-skip behavior, a green run that never enabled
> your language proves nothing about it. **When you touch a language, always
> configure with its flag ON** so its tests are guaranteed to run.

Typical configurations:

```bash
# Only C / C++ / CLI (fastest)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Working on the Rust binding
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCTOON_BUILD_RUST=ON

# Everything, exactly like CI
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCTOON_BUILD_PYTHON=ON -DCTOON_BUILD_RUST=ON -DCTOON_BUILD_ZIG=ON \
      -DCTOON_BUILD_GO=ON -DCTOON_BUILD_JULIA=ON
```

### 4.2 Building

```bash
cmake --build build -j
```

This builds the C library, the C++ wrapper, the `ctoon` CLI, tests, and (with
`CTOON_BUILD_PYTHON=ON`) the Python extension.

### 4.3 Running tests

There are two equivalent ways; use whichever you prefer:

```bash
# A) ctest — runs every registered language test, verbose on failure
ctest --test-dir build --output-on-failure --extra-verbose

# B) the aggregate custom target
cmake --build build --target ctoon_test
```

Per-language targets exist too, so you can iterate quickly on one language:

```bash
cmake --build build --target ctoon_test_c
cmake --build build --target ctoon_test_cpp
cmake --build build --target ctoon_test_go
cmake --build build --target ctoon_test_rust
cmake --build build --target ctoon_test_zig
cmake --build build --target ctoon_test_julia
# ...and so on; `cmake --build build --target help | grep ctoon_test`
```

`ctest` test names follow `ctoon_<lang>_tests` (e.g. `ctoon_rust_tests`), so
you can run a single language with `ctest --test-dir build -R rust
--output-on-failure`.

Notes:

- Each `tests/<lang>/CMakeLists.txt` registers its suite with `add_test()`
  and also defines a `ctoon_test_<lang>` target that is wired into the
  aggregate `ctoon_test`.
- Spec-conformance and round-trip tests receive
  `CTOON_SPEC_FIXTURES_DIR` and `CTOON_SPEC_EXAMPLES_DIR` from CMake. If the
  fixtures could not be fetched (no network), those tests must **skip, not
  fail**.
- Round-trip tests compare against `tests/data/*.json` + `*.toon` pairs. TOON
  does not distinguish `15` from `15.0`, so a whole-valued float written to
  TOON reads back as an integer; language round-trip tests compare numbers by
  value for that case.

### 4.4 Coverage

```bash
cmake --build build --target ctoon_coverage
```

This runs the test suites under instrumentation for every language that is
enabled and available, writes per-language reports, merges the lcov-based ones
into a total, and generates a dashboard:

```
build/coverage/
├── index.html          # dashboard linking every report
├── total/              # merged lcov + HTML (C, C++, Python, Go, Rust, Julia)
├── c/  cpp/  python/  go/  rust/  julia/
└── (MATLAB coverage is Cobertura XML from its own build tool)
```

Individual targets: `ctoon_coverage_c`, `ctoon_coverage_cpp`,
`ctoon_coverage_python`, `ctoon_coverage_go`, `ctoon_coverage_rust`,
`ctoon_coverage_julia`, and `ctoon_coverage_total`.

Requirements: `lcov`/`genhtml` are needed for the merged report. Rust needs
`cargo-llvm-cov`; Julia needs the `Coverage` package. If a coverage tool is
missing, CMake warns and skips that language's coverage target. Zig has no
coverage yet.

If you change behavior, please check that the code you added is covered.

### 4.5 Documentation

```bash
cmake -B build -DCTOON_BUILD_DOCS=ON        # plus any CTOON_BUILD_<LANG>=ON you need
cmake --build build --target ctoon_docs     # everything
```

Sub-targets: `ctoon_docs_c`, `ctoon_docs_cpp`, `ctoon_docs_python`,
`ctoon_docs_rust`, `ctoon_docs_julia`, `ctoon_docs_matlab`,
`ctoon_docs_index` (the landing page). Output goes to `build/docs/out/`.

Docs are built by each language's own doc tool (Doxygen for C/C++, Sphinx for
Python, `cargo doc` for Rust, Documenter.jl for Julia, ...), then assembled
into a common site. The landing page is generated from `docs/index.html.in` by
`docs/generate_index.py` using the language registry in
`docs/supported_langs.json`. Missing doc tools produce a warning and skip that
section.

If you change a public API, update its documentation in the same PR.

### 4.6 The TOON spec dependency

CToon is tested against [toon-format/spec](https://github.com/toon-format/spec).
`cmake/SpecVersion.cmake` resolves which spec release we build against
(cached in `supported_spec.conf`), and `tests/CMakeLists.txt` fetches that
tag with `FetchContent`. The resolved version is logged at configure time and
shown on the documentation landing page. You normally don't need to touch
this.

---

## 5. Editing or adding a feature to an existing language

This is the simpler path. After your [issue](#1-workflow-issue-first-then-fork)
has been discussed:

1. Fork and branch.
2. Make your change under `src/bindings/<lang>/` (and, if needed,
   `src/ctoon.c` / `include/ctoon.h`).
3. Add or update tests under `tests/<lang>/`.
4. Update the language's docs under `docs/<lang>/` if the public API changed.
5. **Run the relevant CMake commands and make sure they pass** — CI runs the
   same commands, and it is much faster to catch problems locally:

   ```bash
   # configure with YOUR language enabled, so it can't be silently skipped
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCTOON_BUILD_<LANG>=ON
   cmake --build build -j
   ctest --test-dir build --output-on-failure --extra-verbose

   # if you changed behavior or the public API:
   cmake --build build --target ctoon_coverage
   cmake -B build -DCTOON_BUILD_DOCS=ON -DCTOON_BUILD_<LANG>=ON
   cmake --build build --target ctoon_docs
   ```

6. If you touched the **C core**, run the full suite for **all** languages
   (`-DCTOON_BUILD_ALL_LANGS=ON`), because every binding compiles against it.

You do not need to modify CI for changes to an existing language.

---

## 6. Adding a new language binding

Adding a language is a bigger commitment and **needs maintainer approval
*before* you start**. Open an issue titled `New language: <Lang>` explaining
why the binding is useful, how it will call the C core, and who will maintain
it. Do not begin implementation until a maintainer confirms in the issue.

Once approved, a new language is expected to be a complete, first-class
citizen of the CMake and CI setup — not just source code. Use an existing
binding (Rust or Go are the closest templates) as a guide. You will need to:

1. **Binding source** — `src/bindings/<lang>/` plus the package manifest the
   language needs (some manifests must live at the repo root, like
   `Cargo.toml` and `build.zig.zon`).
2. **Tests** — `tests/<lang>/`, including a round-trip test over
   `tests/data/` and a spec-conformance test using
   `CTOON_SPEC_FIXTURES_DIR` / `CTOON_SPEC_EXAMPLES_DIR`.
3. **CMake integration** — add `tests/<lang>/CMakeLists.txt` following the
   pattern in `tests/rust/CMakeLists.txt`:
   - `ctoon_lang_wanted(CTOON_BUILD_<LANG> ...)` and `return()` early when the
     language isn't requested;
   - `find_program(...)` + `ctoon_require_lang(...)` and a minimum-version
     check;
   - `add_test(NAME ctoon_<lang>_tests ...)` with a timeout and the spec
     environment variables;
   - a `ctoon_test_<lang>` custom target added as a dependency of
     `ctoon_test`;
   - a `ctoon_coverage_<lang>` target if the language has usable coverage
     tooling, registered with `ctoon_coverage`, producing lcov + HTML in the
     standard layout;
   then register it: `add_subdirectory(.../<lang>)` in `tests/CMakeLists.txt`,
   add the `CTOON_BUILD_<LANG>` option in the root `CMakeLists.txt`, and (for
   coverage) the dashboard entry in `tests/CMakeLists.txt`.
4. **Docs** — a `docs/<lang>/` folder with `index-docs.json`,
   `index-install.txt` and `index-example.txt`, an entry in
   `docs/supported_langs.json`, and a docs target in `docs/CMakeLists.txt`
   (see the existing languages for the exact shape).
5. **CI** — see the next section. This part is mandatory.
6. **Benchmarks** *(optional but welcome)* — `benchmarks/<lang>/`.
7. **Version sync** — if the manifest needs a literal version string, add it
   to `version.py` so it stays in sync with `ctoon.h`.

A new-language PR that doesn't include CI changes will not be merged, because
the language's tests would silently never run.

---

## 7. Continuous integration

CI is split into small workflows under `.github/workflows/`:

| Workflow | Purpose |
|----------|---------|
| `cmake.yml` | The main workflow. Builds **all** languages on Ubuntu, macOS and Windows and runs the `ctest` suite on Ubuntu and macOS (the Windows job currently builds only), then builds docs and coverage and uploads them as artifacts. Runs on every PR to `master`. |
| `wheels.yml` | Python — builds the sdist and wheels and uploads them as artifacts. |
| `crates.yml` | Rust — runs `cargo test`, then `cargo package` / `cargo publish --dry-run`, and uploads the `.crate` as an artifact. |
| `zig.yml` | Zig — cross-compiles release libraries and uploads them as artifacts. |
| `orchestrator.yml` | Runs on pushes to `master` and version tags. Calls the workflows above, creates the release, attaches every artifact, publishes to PyPI/crates.io on tags, and deploys the docs + coverage site to GitHub Pages. |

### The per-language workflow pattern

Each language that produces a distributable has its **own workflow**, in the
style of `wheels.yml` (Python) and `crates.yml` (Rust). They all follow the
same idea: **build the language's package/artifact, verify it, and upload it
as a GitHub Actions artifact.** The orchestrator then downloads those
artifacts and attaches them to a release. Key conventions:

- triggers: `workflow_dispatch`, `workflow_call` (so the orchestrator can
  invoke it) and `pull_request` on `master`, with a `paths:` filter limited
  to that language's files (plus `src/ctoon.c`, `include/ctoon.h` and the
  workflow itself);
- a `concurrency` group so superseded runs are cancelled;
- the last step uploads the artifact with `actions/upload-artifact`;
- `orchestrator.yml` gets a matching job that `uses:` the workflow and a
  `release-<lang>` job that downloads its artifact and attaches it to the
  release (and publishes to the language's registry on tags, when applicable).

### What a new language must change in CI

1. **Add a `.github/workflows/<lang>.yml`** following the pattern above and
   wire it into `orchestrator.yml`.
2. **Edit `cmake.yml` so the language's tests actually run.** The per-language
   workflow *packages* the language; `cmake.yml` is where its **tests** are
   checked, alongside every other language. You must add steps that
   *activate* the language there:

   - **`build-and-test` job:** add a toolchain setup step (e.g.
     `actions/setup-go`, `dtolnay/rust-toolchain`, `mlugg/setup-zig`,
     `julia-actions/setup-julia`) and add `CTOON_BUILD_<LANG>=ON` to the
     `Build Project` options. Without the flag, the language would be
     silently skipped and its tests never run in CI.
   - **`build-docs` job:** add the toolchain setup and
     `-DCTOON_BUILD_<LANG>=ON` if the language has documentation targets.
   - **`build-coverage` job:** add the toolchain, any coverage tooling
     (e.g. `cargo-llvm-cov`, `Coverage.jl`) and `-DCTOON_BUILD_<LANG>=ON`
     if the language supports coverage.

3. Verify the result across the OS matrix in the PR's CI run; fix
   platform-specific issues rather than excluding an OS without discussion.

Editing an *existing* language normally needs no CI changes.

---

## 8. Versioning

The library version is defined once, in `include/ctoon.h`
(`CTOON_VERSION_MAJOR/MINOR/PATCH`). `CMakeLists.txt` and `pyproject.toml`
read it dynamically. Manifests that require a literal version (`Cargo.toml`,
`build.zig.zon`, the Julia `Project.toml`) are kept in sync by `version.py`.
**Do not bump versions in pull requests** — maintainers handle releases with
`python version.py`.

---

## 9. Pull request checklist

Before you open your PR, confirm that:

- [ ] There is an issue, and the PR references it.
- [ ] You configured with `CTOON_BUILD_<LANG>=ON` for every language you
      touched (all languages if you changed the C core).
- [ ] `cmake --build build` succeeds.
- [ ] `ctest --test-dir build --output-on-failure` passes.
- [ ] New behavior has tests, and `ctoon_coverage` shows it is covered.
- [ ] Docs are updated and `ctoon_docs` builds (if you changed a public API).
- [ ] For a new language: maintainer approval in the issue, a per-language
      workflow, and the required `cmake.yml` steps are included.
- [ ] CI is green on every OS in the `cmake.yml` matrix.

---

## 10. License

CToon is released under the [MIT License](LICENSE). By contributing, you agree
that your contributions will be licensed under the same terms.
