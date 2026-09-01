#!/usr/bin/env python3
"""CToon Python benchmark — vs. competing implementations.

Same methodology as every other language benchmark in this repo (see
benchmarks/README.md), so the numbers line up with the C, C++, and Go
results for the same corpus:

  1. Load every file in the corpus manifest into memory (untimed).
  2. Untimed pre-pass: convert each JSON file to TOON once (with ctoon).
  3. Timed "JSON -> TOON": repeatedly parse JSON and re-serialise to TOON.
  4. Timed "TOON -> JSON": repeatedly parse the TOON text from step 2 and
     re-serialise to JSON.
  5. Report throughput (MB/s of the bytes actually read by that operation)
     and documents/sec.

Runs against every implementation importable in the environment:
  - ctoon              (this repo)
  - toon_format        github.com/toon-format/toon-python (official, pure Python)
  - toons              github.com/alesanfra/toons (community, Rust backend)
"""
import argparse
import json
import sys
import time

import ctoon

try:
    from tabulate import tabulate
except ImportError:
    tabulate = None

try:
    import toon_format
except ImportError:
    toon_format = None

try:
    import toons
except ImportError:
    toons = None

REPEATS = 20


def load_corpus(manifest_path):
    files = []
    with open(manifest_path, "r") as mf:
        paths = [line.strip() for line in mf if line.strip()]
    for path in paths:
        try:
            with open(path, "r", encoding="utf-8") as f:
                files.append({"path": path, "json": f.read(), "toon": None})
        except OSError:
            continue
    return files


def bench_json_to_toon(files, json_to_toon_fn):
    """json_to_toon_fn(json_text) -> toon string."""
    t0 = time.perf_counter()
    ops = 0
    bytes_done = 0
    for _ in range(REPEATS):
        for f in files:
            try:
                json_to_toon_fn(f["json"])
                ops += 1
                bytes_done += len(f["json"].encode("utf-8"))
            except Exception:
                pass
    return time.perf_counter() - t0, ops, bytes_done


def bench_toon_to_json(files, toon_to_json_fn):
    """toon_to_json_fn(toon_text) -> json string."""
    t0 = time.perf_counter()
    ops = 0
    bytes_done = 0
    for _ in range(REPEATS):
        for f in files:
            if not f["toon"]:
                continue
            try:
                toon_to_json_fn(f["toon"])
                ops += 1
                bytes_done += len(f["toon"].encode("utf-8"))
            except Exception:
                pass
    return time.perf_counter() - t0, ops, bytes_done


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest")
    args = parser.parse_args()

    files = load_corpus(args.manifest)
    if not files:
        print("Corpus manifest is empty - nothing to benchmark.", file=sys.stderr)
        return 1

    total_json_bytes = sum(len(f["json"].encode("utf-8")) for f in files)

    print("CToon Python Benchmark — vs. competing implementations")
    print(f"Corpus: {len(files)} files, {total_json_bytes / 1e6:.2f} MB (JSON)")
    libs = ["ctoon (this repo)"]
    if toon_format:
        libs.append("toon_format (official, pure Python)")
    if toons:
        libs.append("toons (community, Rust backend)")
    print("Implementations: " + ", ".join(libs) + "\n")

    # ── Untimed pre-pass: TOON text with ctoon, shared as decode input ──
    total_toon_bytes = 0
    for f in files:
        try:
            data = json.loads(f["json"])
            f["toon"] = ctoon.dumps(data)
            total_toon_bytes += len(f["toon"].encode("utf-8"))
        except Exception:
            pass

    rows = []

    def add_rows(name, json_to_toon_fn, toon_to_json_fn):
        t_enc, ops_enc, bytes_enc = bench_json_to_toon(files, json_to_toon_fn)
        rate_enc = f"{100 * ops_enc / (len(files) * REPEATS):.0f}%"
        rows.append([
            name, "JSON -> TOON",
            f"{bytes_enc / t_enc / 1e6:.2f} MB/s" if ops_enc else "n/a",
            f"{ops_enc / t_enc:.0f}",
            rate_enc,
            f"{t_enc:.4f} s",
        ])

        t_dec, ops_dec, bytes_dec = bench_toon_to_json(files, toon_to_json_fn)
        rate_dec = f"{100 * ops_dec / (len(files) * REPEATS):.0f}%"
        rows.append([
            name, "TOON -> JSON",
            f"{bytes_dec / t_dec / 1e6:.2f} MB/s" if ops_dec else "n/a",
            f"{ops_dec / t_dec:.0f}",
            rate_dec,
            f"{t_dec:.4f} s",
        ])

    add_rows(
        "ctoon",
        lambda text: ctoon.dumps(json.loads(text)),
        lambda text: ctoon.dumps_json(ctoon.loads(text), indent=2),
    )

    if toon_format:
        add_rows(
            "toon_format",
            lambda text: toon_format.encode(json.loads(text)),
            lambda text: json.dumps(toon_format.decode(text), indent=2),
        )

    if toons:
        add_rows(
            "toons",
            lambda text: toons.dumps(json.loads(text)),
            lambda text: toons.to_json(text, indent=2),
        )

    headers = ["Library", "Operation", "Throughput", "Docs/sec", "Success", f"Total time (x{REPEATS} reps)"]
    if tabulate:
        print(tabulate(rows, headers=headers))
    else:
        fmt = "{:<14} {:<14} {:>12} {:>10} {:>8} {:>24}"
        print(fmt.format(*headers))
        for row in rows:
            print(fmt.format(*row))

    return 0


if __name__ == "__main__":
    sys.exit(main())
