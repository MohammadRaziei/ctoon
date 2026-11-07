# CToon Benchmarks

Throughput benchmarks for every CToon binding - C, C++, Python, Go,
Rust, Zig, MATLAB, Octave. Every language runs alongside every maintained
competing implementation that exists for that language. This project
has **no relationship to the repository root**, not even for ctoon
itself: every language fetches ctoon the exact same way it fetches any
competitor, from `https://github.com/mohammadraziei/ctoon.git` (C/C++),
`go get github.com/mohammadraziei/ctoon` (Go), a git dependency in
`Cargo.toml` (Rust), `zig fetch --save` (Zig), or `pip install
git+https://github.com/mohammadraziei/ctoon.git` (Python).
**ctoon is never a special case here** - it's one more row in the same
results table as gotoon, toon-go, or toon-rust (the Zig peer, toon-zig,
is built with its own Zig version - see Layout below).

## Running

`benchmarks/` is a self-contained world of its own - nothing here reads
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
the machine it ran on (via `ctoon_bench_system_info`), and - once that
finishes - copies one raw JSON file per language (`c.json`, `cpp.json`, ...,
plus `system_info.json`) to `benchmarks/results/` (via `ctoon_bench_report`).
There is no HTML here on purpose: the documentation build renders the public
page from that JSON (`docs/benchmarks/render_benchmarks.py`, called from
`docs/CMakeLists.txt`; Chart.js loads from the jsDelivr CDN).
Modeled on [pygixml's benchmark suite](https://github.com/MohammadRaziei/pygixml/tree/main/benchmarks) -
same split: benchmarks write JSON, docs render the page.

The page draws, for **every language**, one bar chart of throughput per
operation (JSON→TOON, TOON→JSON, JSON→TOON→JSON, TOON→struct, struct→TOON),
one bar per library, so all languages look the same. **C alone** adds line
charts on top, one per operation, with x-axis = input size: the corpus is
split into 5 equal-count buckets by file size and each bucket is re-measured
on its own (fewer reps than the main comparison); see "Scaling" below.
Peak-memory-per-library is a separate, **opt-in** bar chart - see
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
cmake --build build-bench --target ctoon_benchmarks_octave
```

The results land in **`benchmarks/results/*.json`**, and those files are
meant to be **committed**: benchmarks are run on your own machine, not in CI
(shared runners are too noisy). CI only renders the page from the committed
JSON and publishes it, as `/benchmarks/` on the docs site. Everything else
(venvs, fetched sources, dumps) stays under `build-bench/`. Override the
location with `-DCTOON_BENCH_REPORT_DIR=...`.

To run every language without copying the results out, or to re-copy just
the JSON from whatever is already in `build-bench/results/` (no re-running
the benchmarks):

```bash
cmake --build build-bench --target ctoon_bench_langs   # every language, no copy
cmake --build build-bench --target ctoon_bench_report  # copy only, from existing JSON
```

To render the page by hand from the committed JSON (needs `jinja2`):

```bash
python docs/benchmarks/render_benchmarks.py benchmarks/results /tmp/benchmarks/index.html
```

## Build options

The full set of `cmake -S . -B build-bench -D...` options this project
actually defines - everything else on the command line (`CMAKE_BUILD_TYPE`,
etc.) is plain CMake, not something this project adds:

| Option                 | Default | Effect |
|-------------------------|---------|--------|
| `CTOON_BENCH_LOG_DEBUG` | `OFF`   | Writes a per-file diagnostic log (which files failed, for which library/operation, and why) to `build-bench/logs/<language>.log`. Off by default - extra I/O, only useful while chasing a success-rate anomaly. This is what produced the `[ctoon] [pre_pass] FILE: ... ERROR: ...` lines used above. |
| `CTOON_BENCH_MEMORY`    | `OFF`   | Measures peak RSS per (language, library) in isolated processes - see "Memory" below for the full explanation of why and what it costs. |
| `CTOON_PYPI_INDEX`      | empty   | PyPI index URL used by every venv the suite creates (`uv pip install --index-url ...`), e.g. `-DCTOON_PYPI_INDEX=https://mirror-pypi.runflare.com/simple`. Empty means default PyPI. Only affects those venvs, no global pip/uv config. |
| `CTOON_BENCH_PYTHON_VERSION` | `3.12` | Python version of every venv (`uv venv --python ...`): the Python benchmark's and the Zig ones. |
| `CTOON_ZIG_VERSION`     | `0.16.0` | `ziglang` version installed for the ctoon Zig benchmark (`zig/ctoon/build.zig` targets 0.16.x). |
| `CTOON_TOONZIG_ZIG_VERSION` | `0.15.2` | `ziglang` version installed for the toon-zig benchmark. |
| `CTOON_TOONZIG_REF`     | `v0.1.3` | toon-zig git tag fetched with `zig fetch --save`. |

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DCTOON_BENCH_LOG_DEBUG=ON
cmake --build build-bench --target ctoon_benchmarks_matlab
cat build-bench/logs/matlab.log
```

Everything else you'll see referenced below - `CTOON_BENCH_REPEATS` (20),
`SCALING_NBUCKETS` (5), `SCALING_REPS` (5) - is a compile-time `#define` in
the relevant `bench.*` source file, **not** a `cmake -D` option; there is no
cache variable wired up for them. Changing one means editing that language's
benchmark source (or, for the C benchmark specifically, passing a raw
`-DCTOON_BENCH_REPEATS=N` *compiler* flag yourself - it's guarded with
`#ifndef` - which bypasses CMake's normal option/cache mechanism entirely
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
  "scaling": [ ... ]   // optional, C only, see Scaling below
}
```

`"results"` is one aggregate number per (library, operation) over the
*whole* corpus, at the full `REPEATS` rep count - this is the table in
the report and in the terminal output. `"scaling"` is the same
(library, operation) pairs measured again on 5 size-sorted buckets of
that same corpus, at a lower rep count (`SCALING_REPS`, default 5) since
it runs 5x more passes - this is only used for the line charts. A
language with no `"scaling"` array (not wired up for that language yet)
just gets no line charts in its report section; everything else about it
still renders normally.

The JSON files are kept **separate per language** rather than merged -
each language tests a different set of libraries and has its own corpus
loading overhead, so there's no meaningful single "language-agnostic"
number to combine them into.

## Key order preservation

**Every language and every library** runs one extra, fixed check, one
direction at a time (no round trip, so a library that only reads or only
writes TOON still gets a result for the direction it has):

| direction | input | what is checked |
|---|---|---|
| `json_to_toon` | `order_check/sample.json` | keys of the TOON output, taken line by line (`key:`, `key[N]:`, or the fields in `key[N]{a,b}:`), must follow the JSON sample's order |
| `toon_to_json` | `order_check/sample.toon` | keys of the JSON output (`"key":` occurrences) must follow the TOON sample's order |

Both samples are deliberately non-alphabetical (a nested object and an
array of objects), so sorting or hash-based reordering shows up. The
comparison is on the *first-occurrence* order of the keys, so the tabular
form `list[2]{z,a}:` and the expanded `- z: 1` form are equal. Each harness
carries its own small scanner (a regex where the language has one; no regex
dependency in C, C++, Rust and Zig) and reads the two shared sample files,
so the sample is defined once.

Recorded as an `"order_check"` array (`{library, operation, preserved,
detail}`, `operation` being `json_to_toon` or `toon_to_json`) in each
language's results JSON, and rendered as a small table (one row per
library, a ✓/✗ cell per direction, `–` for a direction the library does not
have) under that language's results, with the two samples shown once near
the top of the page. `detail` is empty on success; on failure it lists the
order that came out (`got apple,zebra,...`) or the error that was raised.

Two things to know when reading it:

- **Go.** gotoon and toon-go take native Go values, so, as in the timed rows,
  the JSON sample goes through `encoding/json` into a `map` first, and Go maps
  have no order. Their ✗ reflects that, not necessarily what the library could
  do with an ordered input.
- **TOONc** only reads TOON, so it only has the `toon_to_json` check (gotoon
  likewise only has `json_to_toon`).

## Scaling

Wired up for **C only**, on purpose: the speed-vs-size curve is a C-specific
extra, every other language is reported as the same whole-corpus bar chart so
languages stay comparable. The C benchmark, after the normal
whole-corpus comparison, sorts the corpus by file size, splits it into
`SCALING_NBUCKETS` (5) equal-*count* buckets, and re-measures every
(library, operation) pair on each bucket at a reduced `SCALING_REPS` (5,
vs. the normal `REPEATS`/20) - a bucket's x-axis position is the
*median* file size within it. This is what feeds the report's line
charts for C (one per operation, one line per library, marker per bucket).

Adding it to a language not yet wired up means: (1) sort the loaded
corpus by size and split into `SCALING_NBUCKETS` slices - `bench.c` does
this via a sorted array of pointers so it never has to duplicate or
reorder the original file data; (2) re-run each (library, operation) on each slice at
`SCALING_REPS` and record `{library, operation, size_bytes,
throughput_mb_s, docs_per_sec, success_rate}` into a `"scaling"` array
in that language's results JSON, alongside the existing `"results"`
array (see the schema above); (3) nothing to change in
`report/CMakeLists.txt` or `docs/benchmarks/render_benchmarks.py` - the page already
draws line charts for whatever `"scaling"` data it finds, and quietly
skips the line charts for a language that has none.

## Memory

Besides throughput, this suite can measure **peak RSS per (language,
library)** - for now, C and C++ (the languages that support
isolated per-library runs so far; see below). It's **off by default**
(reruns every library several times over in fresh processes just to
read peak RSS, which roughly doubles that language's benchmark wall time
for information most people don't need on every run) - turn it on at
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
file) - one number per library, since it's a whole-process high-water
mark, not something finer-grained than that - and the report grows a
peak-memory bar chart and table column per language that has it. When
off (the default), that field, chart, and column are simply absent -
`ctoon_bench_report` doesn't depend on `ctoon_bench_memory` at all in
that case.

**Why isolated processes, not just reading memory after the normal
run:** the normal C run benchmarks more than one library
back-to-back in the *same* process for simplicity. `ru_maxrss` never
goes down, so reading it after such a run would have every later
library's number inflated by whichever earlier library used the most
memory - not a real number about that library. So `collect_memory.py`
re-invokes each language's benchmark binary/script once per library, in
a **fresh process** each time, via `CTOON_BENCH_ONLY=<library>` (an
environment variable `bench.c` checks - see their
docstrings), and reads that one process's peak with `/usr/bin/time -v`.
C++ has no competing implementation at all, so its ordinary run is
already isolated and needs no such flag.

Python, Go, Rust, Zig, MATLAB, Octave, and Julia don't have the `CTOON_BENCH_ONLY`
isolation flag yet, so they're not part of `ctoon_bench_memory` - same
"only what's actually wired up" convention as the rest of this suite.
Adding it to a language means: (1) an env var in that language's bench
runner that skips every library except the one named, and returns
before writing that language's normal results JSON when set; (2) a new
`--<lang>-exe`/`--<lang>-manifest`-style block in `collect_memory.py`;
(3) wiring it into `report/CMakeLists.txt`'s `MEMORY_ARGS`/`MEMORY_DEPS`,
guarded the same way the C/C++ blocks are.

## Layout

Mirrors `tests/`, one folder per language - each fetches its own
dependencies (including ctoon) and defines a `ctoon_benchmarks_<lang>`
target:

```
benchmarks/
  CMakeLists.txt   orchestrator: fetches the corpus, detects toolchains,
                   adds each language below if its toolchain is present
  c/               bench.c        - ctoon, TOONc
  cpp/             bench.cpp      - ctoon (no competitor exists)
  python/          bench.py       - ctoon, toon_format, toons
  go/              bench.go       - ctoon, gotoon, toon-go
  rust/            main.rs        - ctoon, toon-rust
  zig/ctoon/       main.zig       - ctoon (Zig 0.16.x)
  zig/zig-toon/    main.zig       - toon-zig (Zig 0.15.2); its rows are merged into the same
                   results/zig.json by merge_results.py
  matlab/          bench.m        - ctoon (no competitor exists)
  octave/          bench.m        - ctoon (no competitor exists; same method as matlab/)
  node/            bench.mjs      - @toon-format/toon (the reference);
                   verify_outputs.mjs - judges every recorded output (see Success vs. correctness)
```

## Methodology

Every language benchmark follows the same steps over the same corpus (a
shared manifest file generated once by the top-level `CMakeLists.txt`):

1. **Load** every file in the corpus into memory. Not timed.
2. **Pre-pass**: convert each JSON file to TOON once with ctoon (the only
   library in most of these benchmarks with its own JSON parser), to have
   TOON input ready for step 4. Not timed.
3. **Timed - json_to_toon**: repeatedly parse JSON and re-serialise to
   TOON, `x20` over the whole corpus. For **ctoon** this is a single call
   to its one-shot text conversion function in every language
   (`ctoon_json_to_toon`, `ctoon::json_to_toon`, `ctoon.json_to_toon`,
   `ctoon.JSONToToon`, `ctoon::json_to_toon`, `ctoon.jsonToToon`,
   `CToon.json_to_toon`, …) -- no language-level dict/struct is built in
   between. Libraries without such a function go through their own value
   type, as before. `toon_to_json` and `roundtrip` likewise use
   `toon_to_json` / `json_to_toon` + `toon_to_json` for ctoon.
4. **Timed - toon_to_json**: repeatedly parse the TOON text from step 2
   (ctoon's own output) and re-serialise to JSON, `x20` over the whole
   corpus. This measures **interop** with ctoon's specific TOON output.
5. **Timed - roundtrip**: parse JSON → encode TOON → parse that same
   TOON → encode JSON, all four steps chained as **one** operation per
   file using each library's *own* encoder and decoder together - not
   the two legs above run separately, and not cross-library. This
   measures a library's **self-consistency**: can it read back what it
   itself just wrote.
6. **Timed - toon_to_struct / struct_to_toon**: the two halves of the
   above with the text format taken out, i.e. **load** and **dump** against
   the language's own in-memory structure (a Python `dict`, a Go
   `map`/struct value, a MATLAB `struct`, a ctoon document in C/C++, …):
   `toon_to_struct` parses the pre-pass TOON into that structure and stops;
   `struct_to_toon` serialises an already-built structure to TOON. The
   structure fed to `struct_to_toon` is built **untimed, before the clock
   starts, by the language's own independent JSON parser** (never by
   ctoon's CLI or the library under test), so the two operations isolate the
   TOON reader and writer from JSON parsing. Languages/libraries with no
   such in-memory API simply have no row for these two operations.
7. **Untimed - record outputs**: each harness writes what every
   (library, operation) actually produced for every file (see
   [Success vs. correctness](#success-vs-correctness) below).
8. **Verify** (`ctoon_bench_verify`, once, after all languages): one script
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
| `struct_to_toon` | the **official reference decoder** decodes it to the original document           |
| `toon_to_struct` | the harness serialises the decoded structure to JSON (untimed, with the language's own JSON writer) and that parses to the original document |

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

- **[toon-format/spec](https://github.com/toon-format/spec)** fixtures -
  small, varied JSON shapes intentionally covering the format's edge cases.
- **[JSON-Schema-Test-Suite](https://github.com/json-schema-org/JSON-Schema-Test-Suite)**:
  ~450 real-world JSON files (not synthetic data generated for this
  benchmark), the primary corpus.

Both are fetched via `FetchContent` (shallow clones) - nothing is vendored
into this repo.

## Toolchain detection

A language is **skipped with a warning** only when its toolchain itself
isn't found (no Go compiler, no MATLAB installation, no Octave with `mkoctfile`, etc.) - or, for Go
and Rust specifically, when the version found is too old:

- The single Go `go.mod` here tests ctoon, gotoon, and toon-go together as
  peers (no separate `vs_*` subdirectory), and toon-go needs Go ≥ 1.23.
- `toon-rust` (crates.io `toon-format` 0.5.0) uses a standard-library
  integer method stabilized in **Rust 1.87** - checked with both an
  older published version (`0.1.0`, confirmed to be an unimplemented
  placeholder - the crate reserved its name early) and the git `HEAD`,
  neither avoids this; it's a genuine requirement of the only functional
  release, not a git-vs-crates.io difference.
- `toon-zig` declares `minimum_zig_version = "0.15.2"`, while the ctoon
  Zig benchmark is built with Zig 0.16.x, and one executable cannot be
  built by two compilers. So it is a second project, `zig/zig-toon/`, built
  with its own Zig 0.15.2; `ctoon_benchmarks_zig` runs both. Zig is never
  taken from the system: each version is installed from PyPI (`ziglang`)
  into its own venv (`venv_zig_0.16.0`, `venv_zig_0.15.2`) with its own
  Zig cache, all separate from the Python benchmark's venv. toon-zig
  decodes the TOON that ctoon produced (read from ctoon's dump), like
  every other library's `toon_to_json` leg. toon-zig implements TOON
  spec v3.0 while the corpus here is v4.4, so expect it to fail or
  differ on some files; the correctness column shows how many.

Everything else - a Python venv, `pip install`, `go get`, `cargo build`,
`zig fetch --save`, CMake `FetchContent` - happens inside that language's
own `CMakeLists.txt` once the toolchain is confirmed present.
