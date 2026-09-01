# CToon Benchmarks

Throughput benchmarks for every CToon binding: C, C++, Python, Go, and
MATLAB. This is a **standalone** CMake project — it is not built as part of
the main `ctoon` build and the root `CMakeLists.txt` does not know it
exists. It links the other way around: this project pulls the repo root in
as a subdirectory to get the `ctoon::ctoon` / `ctoon::ctoonpp` targets.

## Running

```bash
cmake -S benchmarks -B build-bench -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench --target ctoon_benchmark
```

That builds and runs every benchmark whose language toolchain is present on
the machine (see [Toolchain detection](#toolchain-detection) below). To run
just one language:

```bash
cmake --build build-bench --target ctoon_benchmark_c
cmake --build build-bench --target ctoon_benchmark_cpp
cmake --build build-bench --target ctoon_benchmark_python
cmake --build build-bench --target ctoon_benchmark_go
cmake --build build-bench --target ctoon_benchmark_go_vs_gotoon
cmake --build build-bench --target ctoon_benchmark_go_vs_toongo   # needs Go >= 1.23
cmake --build build-bench --target ctoon_benchmark_matlab
```

## Layout

Mirrors `tests/`, one folder per language:

```
benchmarks/
  CMakeLists.txt        orchestrator: fetches the corpus, builds the manifest,
                         pulls in the repo root, adds each language below
  c/                     bench_ctoon.c        (ctoon::ctoon)
  cpp/                    bench_ctoon.cpp     (ctoon::ctoonpp)
  python/                 bench_ctoon.py + requirements.txt
  go/                     bench_ctoon.go      (module github.com/mohammadraziei/ctoon)
  matlab/                 bench_ctoon.m       (+ctoon MEX binding)
```

## Methodology

Every language benchmark follows the exact same steps, over the exact same
corpus (a shared manifest file, generated once by the top-level
`CMakeLists.txt`, so every language reads the identical set of files):

1. **Load** every file in the corpus into memory. Not timed.
2. **Pre-pass**: convert each JSON file to TOON once, to have TOON input
   ready for step 4. Not timed.
3. **Timed — JSON → TOON**: repeatedly parse JSON and re-serialise to TOON,
   `x20` over the whole corpus.
4. **Timed — TOON → JSON**: repeatedly parse the TOON text produced in step
   2 and re-serialise to JSON, `x20` over the whole corpus.
5. **Report**: throughput in MB/s (of the bytes actually read by that
   specific operation — JSON bytes for step 3, TOON bytes for step 4) and
   documents/second.

Files that fail to round-trip (e.g. a JSON document whose root is a bare
scalar, which some corpora include as edge cases) are skipped and don't
count toward either total.

This is **not universally** a benchmark against competing libraries — but
where a maintained competitor exists for a binding we ship, it's included:

- **C** vs [TOONc](https://github.com/UsboKirishima/TOONc) — TOON → JSON
  only (TOONc has no JSON parser and no TOON writer of its own).
- **Python** vs [toon-format](https://github.com/toon-format/toon-python)
  (official) and [toons](https://github.com/alesanfra/toons) (community,
  Rust backend) — both directions.
- **Go** vs [toon-go](https://github.com/toon-format/toon-go) (official,
  requires Go ≥ 1.23 — see below) and
  [gotoon](https://github.com/alpkeskin/gotoon) (community, encode-only —
  it has no decoder).
- **C++** has no competitor at all — we're the reference implementation
  for that binding — so it exists purely to track the C++ wrapper's own
  overhead over the raw C API across changes.
- **MATLAB** has no listed competitor either.

A competitor that fails to round-trip part of the corpus is not silently
given an inflated number: every result reports a **success rate**
(successful ops ÷ attempted ops), and throughput is computed only from the
bytes of files that actually succeeded. Some of these failures are
genuine, verified parser incompatibilities — e.g. `toon-go`'s decoder
rejects valid TOON output that ctoon itself round-trips correctly, and
`TOONc`'s parser crashes outright on part of the corpus rather than
failing gracefully (isolated in the C benchmark with a per-file
fork-and-probe pass so one crash doesn't take down the whole run).

## Corpus

Three sources, combined into one manifest:

- **`tests/data/*.json`** — the project's own curated samples, including the
  well-known `twitter.json` (~620 KB, deeply nested, realistic shape).
- **[toon-format/spec](https://github.com/toon-format/spec)** fixtures — the
  same pinned checkout `tests/cpp` uses for spec-conformance testing. Small,
  varied JSON shapes that intentionally cover the format's edge cases.
- **[JSON-Schema-Test-Suite](https://github.com/json-schema-org/JSON-Schema-Test-Suite)**
  — ~540 real-world JSON files (not synthetic data generated for this
  benchmark), used as the primary "JSON → TOON" conversion corpus as
  requested. Deeply nested schemas and test-case arrays give a realistic
  mix of object/array/scalar shapes.

Both external repos are fetched via CMake's `FetchContent` (shallow clones,
pinned where reproducibility matters) — nothing is vendored into this repo.

## Toolchain detection

A language is **skipped with a warning** only when its toolchain itself
isn't found (no Go compiler, no MATLAB installation, etc.) — mirroring how
`tests/` already handles Go and MATLAB. The `toon-go` comparison
specifically needs Go ≥ 1.23 (a requirement from that library, not from
ctoon's own Go binding, which only needs 1.21) — if the `go` found on
`PATH` is older, that one comparison is skipped with a warning while the
base Go benchmark and the `gotoon` comparison (which has no such floor)
still run normally.

If the toolchain **is** present but a per-language dependency is missing
(e.g. the `tabulate` pip package, or the local `ctoon` package not yet
installed), it's installed automatically rather than skipped — see
`python/InstallDeps.cmake`.
