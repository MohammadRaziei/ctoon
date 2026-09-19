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
whose language toolchain is present (via `ctoon_bench_langs`), records
the machine it ran on (via `ctoon_bench_system_info`), and — once that
finishes — renders one standalone HTML report combining everything (via
`ctoon_bench_report`) to `build-bench/results/report.html`. Chart.js and
every language's JSON are embedded inline, so the file needs no server
or network to view and is safe to send around on its own.
Modeled on [pygixml's own report generator](https://github.com/MohammadRaziei/pygixml/tree/main/benchmarks/report) —
same CMake-fetched Chart.js, same jinja2-rendered single-file output.

The report draws **three line charts per language** — one per operation
(JSON→TOON, TOON→JSON, roundtrip) — with one line per library, markers
at each point, x-axis = input size. That size axis comes from splitting
the corpus into 5 equal-count buckets by file size and re-measuring each
bucket on its own (fewer reps than the main comparison); see "Scaling"
below. Peak-memory-per-library is a separate, **opt-in** bar chart — see
"Memory" below.

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
  ],
  "scaling": [
    {"library": "ctoon", "operation": "json_to_toon", "size_bytes": 1051.0,
     "throughput_mb_s": 124.1, "docs_per_sec": 119639.3, "success_rate": 1.0},
    ...
  ]
}
```

`"results"` is one aggregate number per (library, operation) over the
*whole* corpus, at the full `REPEATS` rep count — this is the table in
the report and in the terminal output. `"scaling"` is the same
(library, operation) pairs measured again on 5 size-sorted buckets of
that same corpus, at a lower rep count (`SCALING_REPS`, default 5) since
it runs 5x more passes — this is only used for the line charts. A
language with no `"scaling"` array (not wired up for that language yet)
just gets no line charts in its report section; everything else about it
still renders normally.

The JSON files are kept **separate per language** rather than merged —
each language tests a different set of libraries and has its own corpus
loading overhead, so there's no meaningful single "language-agnostic"
number to combine them into.

## Key order preservation

Every language runs one extra, fixed check: a deliberately
non-alphabetical sample (nested object, array of objects) round-tripped
through each library, then checked with a `"key":` regex/scanner against
the JSON text — not a full structural diff, just "did the keys come back
in the same order they went in". Recorded as an `"order_check"` array
(`{library, preserved, detail}`) in each language's results JSON, and
rendered as its own small ✓/✗ table under that language's results table
in the report — with the sample itself shown once, near the top of the
report, so it's clear exactly what was tested. `detail` is empty on
success; on failure it says what came back instead (or, more likely for
a library like TOONc that only *reads* TOON, that a step in the chain
threw or crashed).

## Scaling

Wired up for C, C++, and Python so far. Each of those, after the normal
whole-corpus comparison, sorts the corpus by file size, splits it into
`SCALING_NBUCKETS` (5) equal-*count* buckets, and re-measures every
(library, operation) pair on each bucket at a reduced `SCALING_REPS` (5,
vs. the normal `REPEATS`/20) — a bucket's x-axis position is the
*median* file size within it. This is what feeds the report's three
line charts per language (one per operation, one line per library,
marker per bucket).

Adding it to a language not yet wired up means: (1) sort the loaded
corpus by size and split into `SCALING_NBUCKETS` slices — `bench.c` does
this via a sorted array of pointers so it never has to duplicate or
reorder the original file data, `bench.py` just does `sorted(files,
key=...)`; (2) re-run each (library, operation) on each slice at
`SCALING_REPS` and record `{library, operation, size_bytes,
throughput_mb_s, docs_per_sec, success_rate}` into a `"scaling"` array
in that language's results JSON, alongside the existing `"results"`
array (see the schema above); (3) nothing to change in
`report/CMakeLists.txt` or `generate_report.py` — the report already
draws line charts for whatever `"scaling"` data it finds, and quietly
skips the line charts for a language that has none.

## Memory

Besides throughput, this suite can measure **peak RSS per (language,
library)** — for now, C, C++, and Python (the languages that support
isolated per-library runs so far; see below). It's **off by default**
(reruns every library several times over in fresh processes just to
read peak RSS, which roughly doubles that language's benchmark wall time
for information most people don't need on every run) — turn it on at
configure time:

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DCTOON_BENCH_MEMORY=ON
cmake --build build-bench --target ctoon_benchmarks
```

Or run it on its own against an already-configured build:

```bash
cmake --build build-bench --target ctoon_bench_memory
```

When on, it adds a `"peak_rss_mb"` field onto that library's rows in the
existing `<language>.json` files (merged in place, not a separate
file) — one number per library, since it's a whole-process high-water
mark, not something finer-grained than that — and the report grows a
peak-memory bar chart and table column per language that has it. When
off (the default), that field, chart, and column are simply absent —
`ctoon_bench_report` doesn't depend on `ctoon_bench_memory` at all in
that case.

**Why isolated processes, not just reading memory after the normal
run:** the normal C and Python runs benchmark more than one library
back-to-back in the *same* process for simplicity. `ru_maxrss` never
goes down, so reading it after such a run would have every later
library's number inflated by whichever earlier library used the most
memory — not a real number about that library. So `collect_memory.py`
re-invokes each language's benchmark binary/script once per library, in
a **fresh process** each time, via `CTOON_BENCH_ONLY=<library>` (an
environment variable both `bench.c` and `bench.py` check — see their
docstrings), and reads that one process's peak with `/usr/bin/time -v`.
C++ has no competing implementation at all, so its ordinary run is
already isolated and needs no such flag.

One caveat worth knowing: Python's `toon_to_json`/`roundtrip` need TOON
input, and only ctoon can produce that (it's the only library here with
a JSON parser too) — so *every* library's isolated Python run pays that
same shared, constant cost during its pre-pass. It's identical across
libraries, so relative comparisons between them are still fair; it just
means these numbers aren't directly comparable to C's (which has no such
shared step).

Go, Rust, Zig, MATLAB, and Julia don't have the `CTOON_BENCH_ONLY`
isolation flag yet, so they're not part of `ctoon_bench_memory` — same
"only what's actually wired up" convention as the rest of this suite.
Adding it to a language means: (1) an env var in that language's bench
runner that skips every library except the one named, and returns
before writing that language's normal results JSON when set; (2) a new
`--<lang>-exe`/`--<lang>-manifest`-style block in `collect_memory.py`;
(3) wiring it into `report/CMakeLists.txt`'s `MEMORY_ARGS`/`MEMORY_DEPS`,
guarded the same way the C/C++/Python blocks are.

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
