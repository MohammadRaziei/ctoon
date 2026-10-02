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
`ctoon_bench_report`) to `benchmarks/results/report.html`. Chart.js and
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

The report lands in **`benchmarks/results/report.html`**, and that file is
meant to be **committed**: benchmarks are run on your own machine, not in CI
(shared runners are too noisy). CI only publishes the committed file, as
`/benchmarks/` on the docs site. Everything else (JSON, venvs, fetched
sources) stays under `build-bench/`. Override the location with
`-DCTOON_BENCH_REPORT_DIR=...`.

To run every language without rendering the report, or to re-render just
the report from whatever JSON is already in `build-bench/results/`
(no re-running the benchmarks, no API/network spend beyond Chart.js):

```bash
cmake --build build-bench --target ctoon_bench_langs   # every language, no report
cmake --build build-bench --target ctoon_bench_report  # report only, from existing JSON
```

## Build options

The full set of `cmake -S . -B build-bench -D...` options this project
actually defines — everything else on the command line (`CMAKE_BUILD_TYPE`,
etc.) is plain CMake, not something this project adds:

| Option                 | Default | Effect |
|-------------------------|---------|--------|
| `CTOON_BENCH_LOG_DEBUG` | `OFF`   | Writes a per-file diagnostic log (which files failed, for which library/operation, and why) to `build-bench/logs/<language>.log`. Off by default — extra I/O, only useful while chasing a success-rate anomaly. This is what produced the `[ctoon] [pre_pass] FILE: ... ERROR: ...` lines used above. |
| `CTOON_BENCH_MEMORY`    | `OFF`   | Measures peak RSS per (language, library) in isolated processes — see "Memory" below for the full explanation of why and what it costs. |

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DCTOON_BENCH_LOG_DEBUG=ON
cmake --build build-bench --target ctoon_benchmarks_matlab
cat build-bench/logs/matlab.log
```

Everything else you'll see referenced below — `CTOON_BENCH_REPEATS` (20),
`SCALING_NBUCKETS` (5), `SCALING_REPS` (5) — is a compile-time `#define` in
the relevant `bench.*` source file, **not** a `cmake -D` option; there is no
cache variable wired up for them. Changing one means editing that language's
benchmark source (or, for the C benchmark specifically, passing a raw
`-DCTOON_BENCH_REPEATS=N` *compiler* flag yourself — it's guarded with
`#ifndef` — which bypasses CMake's normal option/cache mechanism entirely
and won't show up in `cmake -L` or a cache GUI).


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
  ],   // (after verification each row also has "verified", "verify" and "raw"
       //  -- see Success vs. correctness below)
  "scaling": [ ... ]   // optional, C and C++ only, see Scaling below
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

C and C++ run one extra, fixed check: a deliberately
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

Wired up for C and C++ so far. Each of those, after the normal
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
reorder the original file data; (2) re-run each (library, operation) on each slice at
`SCALING_REPS` and record `{library, operation, size_bytes,
throughput_mb_s, docs_per_sec, success_rate}` into a `"scaling"` array
in that language's results JSON, alongside the existing `"results"`
array (see the schema above); (3) nothing to change in
`report/CMakeLists.txt` or `generate_report.py` — the report already
draws line charts for whatever `"scaling"` data it finds, and quietly
skips the line charts for a language that has none.

## Memory

Besides throughput, this suite can measure **peak RSS per (language,
library)** — for now, C and C++ (the languages that support
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
run:** the normal C run benchmarks more than one library
back-to-back in the *same* process for simplicity. `ru_maxrss` never
goes down, so reading it after such a run would have every later
library's number inflated by whichever earlier library used the most
memory — not a real number about that library. So `collect_memory.py`
re-invokes each language's benchmark binary/script once per library, in
a **fresh process** each time, via `CTOON_BENCH_ONLY=<library>` (an
environment variable `bench.c` checks — see their
docstrings), and reads that one process's peak with `/usr/bin/time -v`.
C++ has no competing implementation at all, so its ordinary run is
already isolated and needs no such flag.

Python, Go, Rust, Zig, MATLAB, and Julia don't have the `CTOON_BENCH_ONLY`
isolation flag yet, so they're not part of `ctoon_bench_memory` — same
"only what's actually wired up" convention as the rest of this suite.
Adding it to a language means: (1) an env var in that language's bench
runner that skips every library except the one named, and returns
before writing that language's normal results JSON when set; (2) a new
`--<lang>-exe`/`--<lang>-manifest`-style block in `collect_memory.py`;
(3) wiring it into `report/CMakeLists.txt`'s `MEMORY_ARGS`/`MEMORY_DEPS`,
guarded the same way the C/C++ blocks are.

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
  node/            bench.mjs      — @toon-format/toon (the reference);
                   verify_outputs.mjs — judges every recorded output (see Success vs. correctness)
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
6. **Untimed — record outputs**: each harness writes what every
   (library, operation) actually produced for every file (see
   [Success vs. correctness](#success-vs-correctness) below).
7. **Verify** (`ctoon_bench_verify`, once, after all languages): one script
   judges every recorded output against one shared criterion and rewrites
   the numbers: it adds `correct_rate` and makes MB/s count only *correct*
   conversions.
8. **Report**: throughput in MB/s (of bytes of *correct* conversions only)
   and documents/second, plus the success rate and the correct rate.

Because step 4 and step 5 test different things, a library can legitimately
score very differently on each: `toon_to_json` reads ctoon's TOON output
(**interop**), `roundtrip` reads the library's *own* output back
(**self-consistency**). That's not a contradiction; it's the two metrics
doing their jobs.

A library that gets part of the corpus wrong does not get an inflated
throughput number: those files don't count towards MB/s or docs/s, and they
lower the success rate. (`TOONc`'s parser also crashes on part of the corpus;
the C benchmark isolates that with a per-file fork-and-probe pass so one
crash doesn't take the whole run down.)

## Success vs. correctness

Two separate parameters, because they say different things:

- **`success_rate`** -- the share of corpus files a library converted
  **without an error**.
- **`correct_rate`** -- of the outputs it actually *produced*, the share that
  were **correct**. (`null` if it produced nothing.)

A parser that returns the wrong document without complaining is not a fast
parser, it's a broken one, and "no exception" alone cannot tell the two
apart -- so "98% success, 0% correct" is a result worth being able to read at
a glance. **MB/s and docs/s count only correct conversions**, so a wrong
answer never looks like a fast one.

The criterion for "correct" is the same for every language, because one
script (`node/verify_outputs.mjs`) applies it to all of them:

| operation      | the output is correct if...                                                        |
| -------------- | ---------------------------------------------------------------------------------- |
| `json_to_toon` | the **official reference decoder** (`@toon-format/toon`) decodes it to the original document |
| `toon_to_json` | it parses as JSON to the original document                                         |
| `roundtrip`    | it parses as JSON to the original document                                         |

- The TOON is decoded by the *reference* decoder, **not by the library's own**,
  on purpose: a lenient decoder hides encoder bugs from its own round trip
  (ctoon's decoder once accepted the JSON-style `\f` escape that its encoder
  wrongly wrote, so a same-library round trip passed while a conforming
  decoder rejected the output).
- "The original document" means the spec's JSON-model equality: objects are
  unordered maps, arrays are ordered, and **numbers compare as IEEE-754
  doubles** (the spec lets an encoder fall back to the host's numeric
  approximation for numbers outside its range, so a 60-digit integer that
  comes back as the nearest double is correct). Key *order* is a separate,
  stricter check -- see [Key order preservation](#key-order-preservation).
- `toon_to_json` decodes the TOON that ctoon produced for each file. If that
  shared input is itself missing or wrong, the file is left out of **every**
  library's `toon_to_json` row (numerator and denominator), and the encoder
  that made the bad input is charged for it in `json_to_toon` instead -- so
  no decoder is blamed for someone else's output.

`success_rate` is (files converted without error) / (corpus files);
`correct_rate` is (correct outputs) / (outputs produced); `throughput_mb_s`
and `docs_per_sec` count only the correct conversions (the *time* is still the
time of all attempts). The harness's own numbers are kept under `"raw"` in
each row, and a verified row carries `"verified": true` and a `"verify"`
object with counts and the first few wrong files -- the quickest way to see
*what* a library got wrong. Rows that could not be verified (no Node.js, or a
harness that doesn't record outputs yet) have no `correct_rate`, report
throughput for every conversion that raised no error, and are marked `*` in
the report.

**What a harness has to do** (the only language-specific part): in an untimed
pass before its timed runs, write the text each (library, operation) produced
for each corpus file to

```
<results dir>/dump/<language>/<library>/<operation>/<file index>.txt
```

and write **nothing** for a file the library failed on. `<file index>` is the
0-based position of the file in the corpus manifest; `<library>` must be
filesystem-safe (`/`, `@`, ... become `_`). The results directory is derived
from the results JSON path every harness already receives, so no CMake
changes are needed per language. (Zig records one framed file per operation
instead -- see `openDump` in `verify_outputs.mjs` -- to stay within the file
APIs its harness already uses.) Memory-measurement runs (`CTOON_BENCH_ONLY`)
must not dump.

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
