# CToon Benchmarks

Throughput benchmarks for every CToon binding — C, C++, Python, Go,
Rust, Zig, MATLAB. Every language runs alongside every maintained
competing implementation that exists for that language. This project
has **no relationship to the repository root**, not even for ctoon
itself: every language fetches ctoon the exact same way it fetches any
competitor, from `https://github.com/mohammadraziei/ctoon.git` (C/C++),
`go get github.com/mohammadraziei/ctoon` (Go), a git dependency in
`Cargo.toml` (Rust), `zig fetch --save` (Zig), or `pip install
git+https://github.com/mohammadraziei/ctoon.git` (Python).
**ctoon is never a special case here** — it's one more row in the same
results table as gotoon, toon-go, or toon-rust (Zig has no peer
currently benchmarked — see Layout below).

## Running

`benchmarks/` is a self-contained world of its own — nothing here reads
or references anything outside this folder, so you `cd` into it first and
run everything from there:

```bash
cd benchmarks
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench --target ctoon_benchmarks
```

`ctoon_benchmarks` is the umbrella target: it fetches every dependency
(the corpus, and every language's own libraries), runs every benchmark
whose language toolchain is present (via `ctoon_bench_langs`), and — once
those finish — renders one standalone HTML report combining all of them
(via `ctoon_bench_report`) to `build-bench/results/report.html`. Chart.js
and every language's JSON are embedded inline, so the file needs no
server or network to view and is safe to send around on its own.
Modeled on [pygixml's own report generator](https://github.com/MohammadRaziei/pygixml/tree/main/benchmarks/report) —
same CMake-fetched Chart.js, same jinja2-rendered single-file output.

To run just one language (skips the report):

```bash
cmake --build build-bench --target ctoon_benchmarks_c
cmake --build build-bench --target ctoon_benchmarks_cpp
cmake --build build-bench --target ctoon_benchmarks_python
cmake --build build-bench --target ctoon_benchmarks_go
cmake --build build-bench --target ctoon_benchmarks_rust
cmake --build build-bench --target ctoon_benchmarks_zig
cmake --build build-bench --target ctoon_benchmarks_matlab
```

To run every language without rendering the report, or to re-render just
the report from whatever JSON is already in `build-bench/results/`
(no re-running the benchmarks, no API/network spend beyond Chart.js):

```bash
cmake --build build-bench --target ctoon_bench_langs   # every language, no report
cmake --build build-bench --target ctoon_bench_report  # report only, from existing JSON
```

Each per-language target above also prints a results table and writes a
JSON file to `build-bench/results/<language>.json`:

```json
{
  "language": "python",
  "corpus": {"files": 462, "bytes": 3567890},
  "results": [
    {"library": "ctoon", "operation": "json_to_toon", "throughput_mb_s": 87.0,
     "docs_per_sec": 12345.0, "success_rate": 1.0, "total_time_s": 1.23},
    ...
  ]
}
```

The JSON files are kept **separate per language** rather than merged —
each language tests a different set of libraries and has its own corpus
loading overhead, so there's no meaningful single "language-agnostic"
number to combine them into.

## Layout

Mirrors `tests/`, one folder per language — each fetches its own
dependencies (including ctoon) and defines a `ctoon_benchmarks_<lang>`
target:

```
benchmarks/
  CMakeLists.txt   orchestrator: fetches the corpus, detects toolchains,
                   adds each language below if its toolchain is present
  c/               bench.c        — ctoon, TOONc
  cpp/             bench.cpp      — ctoon (no competitor exists)
  python/          bench.py       — ctoon, toon_format, toons
  go/              bench.go       — ctoon, gotoon, toon-go
  rust/            main.rs        — ctoon, toon-rust
  zig/             main.zig       — ctoon (toon-zig doesn't compile on Zig 0.16 yet)
  matlab/          bench.m        — ctoon (no competitor exists)
```

## Methodology

Every language benchmark follows the same steps over the same corpus (a
shared manifest file generated once by the top-level `CMakeLists.txt`):

1. **Load** every file in the corpus into memory. Not timed.
2. **Pre-pass**: convert each JSON file to TOON once with ctoon (the only
   library in most of these benchmarks with its own JSON parser), to have
   TOON input ready for step 4. Not timed.
3. **Timed — json_to_toon**: repeatedly parse JSON and re-serialise to
   TOON, `x20` over the whole corpus.
4. **Timed — toon_to_json**: repeatedly parse the TOON text from step 2
   (ctoon's own output) and re-serialise to JSON, `x20` over the whole
   corpus. This measures **interop** with ctoon's specific TOON output.
5. **Timed — roundtrip**: parse JSON → encode TOON → parse that same
   TOON → encode JSON, all four steps chained as **one** operation per
   file using each library's *own* encoder and decoder together — not
   the two legs above run separately, and not cross-library. This
   measures a library's **self-consistency**: can it read back what it
   itself just wrote.
6. **Report**: throughput in MB/s (of bytes actually read by *successful*
   conversions only) and documents/second, plus a success rate.

Because step 4 and step 5 test different things, a library can legitimately
score very differently on each — e.g. `toon-go`'s decoder rejects a good
chunk of *ctoon's* TOON output (low `toon_to_json` success rate) while
happily reading back its *own* encoder's output almost every time (high
`roundtrip` success rate). That's not a contradiction; it's the two
metrics doing their jobs.

A library that fails to round-trip part of the corpus does not get an
inflated throughput number — see each language's own findings on this:
`TOONc`'s parser crashes on part of the corpus (isolated with a per-file
fork-and-probe pass in the C benchmark so one crash doesn't take the whole
run down), and `toon-go`'s decoder rejects TOON output that ctoon itself
round-trips correctly.

## Corpus

- **[toon-format/spec](https://github.com/toon-format/spec)** fixtures —
  small, varied JSON shapes intentionally covering the format's edge cases.
- **[JSON-Schema-Test-Suite](https://github.com/json-schema-org/JSON-Schema-Test-Suite)**
  — ~450 real-world JSON files (not synthetic data generated for this
  benchmark), the primary corpus.

Both are fetched via `FetchContent` (shallow clones) — nothing is vendored
into this repo.

## Toolchain detection

A language is **skipped with a warning** only when its toolchain itself
isn't found (no Go compiler, no MATLAB installation, etc.) — or, for Go
and Rust specifically, when the version found is too old:

- The single Go `go.mod` here tests ctoon, gotoon, and toon-go together as
  peers (no separate `vs_*` subdirectory), and toon-go needs Go ≥ 1.23.
- `toon-rust` (crates.io `toon-format` 0.5.0) uses a standard-library
  integer method stabilized in **Rust 1.87** — checked with both an
  older published version (`0.1.0`, confirmed to be an unimplemented
  placeholder — the crate reserved its name early) and the git `HEAD`,
  neither avoids this; it's a genuine requirement of the only functional
  release, not a git-vs-crates.io difference.
- `toon-zig` is not currently benchmarked: its decoder calls
  `std.json.ObjectMap.init(allocator)`, the pre-0.16 single-arg form
  that Zig 0.16's ArrayHashMap-based `ObjectMap` no longer accepts —
  an upstream compatibility gap, not something patched around here.
  Revisit once upstream supports 0.16; see `benchmarks/zig/build.zig`.

Everything else — a Python venv, `pip install`, `go get`, `cargo build`,
`zig fetch --save`, CMake `FetchContent` — happens inside that language's
own `CMakeLists.txt` once the toolchain is confirmed present.
