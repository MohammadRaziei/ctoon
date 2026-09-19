#!/usr/bin/env python3
"""CToon Python benchmark.

Every implementation here — ctoon included — is one more peer entry,
installed from its own source (GitHub or PyPI), tested the same way. No
"ctoon vs X" framing: one shared results table, one row per
(library, operation) pair.

Methodology (same across every language benchmark in this repo):
  1. Load every file in the corpus manifest into memory (untimed).
  2. Untimed pre-pass: convert each JSON file to TOON once (with ctoon).
  3. Timed "json_to_toon": repeatedly parse JSON and re-serialise to TOON.
  4. Timed "toon_to_json": repeatedly parse the TOON text from step 2 and
     re-serialise to JSON. Measures interop with ctoon's specific output.
  5. Timed "roundtrip": json -> toon -> parse toon -> json, all four steps
     chained as one operation using each library's own encoder/decoder
     together. Measures self-consistency, not interop — see the top-level
     README for why a library's toon_to_json and roundtrip success rates
     can legitimately differ a lot.
  6. Report throughput (MB/s of bytes actually read by successful
     conversions only) and documents/sec.

Implementations:
  - ctoon        this project (github.com/mohammadraziei/ctoon)
  - toon_format  github.com/toon-format/toon-python (official)
  - toons        github.com/alesanfra/toons (community, Rust backend)

If the CTOON_BENCH_ONLY environment variable is set to a library name
("ctoon", "toon_format", or "toons"), only that library's benchmark runs,
and the results JSON is NOT written -- used by the memory harness to get
one library's peak RSS in a fresh process. Note: the untimed pre-pass
(building each file's TOON text) always uses ctoon regardless of which
library is being isolated, since toon_to_json/roundtrip need TOON input
from *somewhere* -- so every library's isolated number includes that one
constant, shared cost. It's the same for every library, so relative
memory comparisons between them are still fair; it just means these
memory numbers aren't directly comparable to, say, C's (which has no such
shared step). Normal timed runs leave this unset.
"""
import argparse
import json
import os
import resource
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
SCALING_NBUCKETS = 5
SCALING_REPS = 5

# ------------------------------------------------------------ order check --
# A fixed, deliberately non-alphabetical sample -- if a library reorders
# keys (e.g. sorts them, or hashes them into a dict with no defined
# order), this catches it. Checked by extracting `"key":` occurrences
# from the round-tripped JSON text with a regex (robust to whichever
# indentation/spacing a library's own JSON writer uses) and comparing
# that sequence to the sample's own key order -- not a full JSON walk,
# but enough for a single-level-deep, honest yes/no per library.
ORDER_CHECK_SAMPLE = (
    '{"zebra": 1, "apple": 2, "mango": 3, '
    '"nested": {"beta": true, "alpha": false}, '
    '"list": [{"z": 1, "a": 2}, {"z": 3, "a": 4}]}'
)
_KEY_RE = __import__("re").compile(r'"([A-Za-z_][A-Za-z0-9_]*)"\s*:')


def _key_order(text):
    return _KEY_RE.findall(text)


def check_order_preserved(roundtrip_fn):
    """roundtrip_fn(json_text) -> json_text. Returns (preserved: bool,
    detail: str) -- detail explains a failure or exception, empty on
    success."""
    try:
        out = roundtrip_fn(ORDER_CHECK_SAMPLE)
    except Exception as e:
        return False, f"exception: {e}"
    expected = _key_order(ORDER_CHECK_SAMPLE)
    got = _key_order(out)
    if got == expected:
        return True, ""
    return False, f"expected {expected}, got {got}"


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


def log_fail(log_file, library, operation, path, exc):
    if log_file is None:
        return
    log_file.write(f"[{library}] [{operation}] FILE: {path} ERROR: {exc}\n")


def bench_json_to_toon(files, json_to_toon_fn, log_file, library, reps=REPEATS):
    t0 = time.perf_counter()
    ops = 0
    bytes_done = 0
    for rep in range(reps):
        for f in files:
            try:
                json_to_toon_fn(f["json"])
                ops += 1
                bytes_done += len(f["json"].encode("utf-8"))
            except Exception as e:
                if rep == 0:
                    log_fail(log_file, library, "json_to_toon", f["path"], e)
    return time.perf_counter() - t0, ops, bytes_done


def bench_toon_to_json(files, toon_to_json_fn, log_file, library, reps=REPEATS):
    t0 = time.perf_counter()
    ops = 0
    bytes_done = 0
    for rep in range(reps):
        for f in files:
            if not f["toon"]:
                continue
            try:
                toon_to_json_fn(f["toon"])
                ops += 1
                bytes_done += len(f["toon"].encode("utf-8"))
            except Exception as e:
                if rep == 0:
                    log_fail(log_file, library, "toon_to_json", f["path"], e)
    return time.perf_counter() - t0, ops, bytes_done


def bench_roundtrip(files, roundtrip_fn, log_file, library, reps=REPEATS):
    """roundtrip_fn(json_text) -> json string, chaining json->toon->json
    as one operation per file rather than running the two legs separately."""
    t0 = time.perf_counter()
    ops = 0
    bytes_done = 0
    for rep in range(reps):
        for f in files:
            try:
                roundtrip_fn(f["json"])
                ops += 1
                bytes_done += len(f["json"].encode("utf-8"))
            except Exception as e:
                if rep == 0:
                    log_fail(log_file, library, "roundtrip", f["path"], e)
    return time.perf_counter() - t0, ops, bytes_done


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest")
    parser.add_argument("results_json")
    parser.add_argument("log_file", nargs="?", default=None,
                         help="optional: write per-file failure diagnostics here")
    args = parser.parse_args()

    files = load_corpus(args.manifest)
    if not files:
        print("Corpus manifest is empty - nothing to benchmark.", file=sys.stderr)
        return 1

    total_json_bytes = sum(len(f["json"].encode("utf-8")) for f in files)

    print("CToon Benchmarks — Python")
    print(f"Corpus: {len(files)} files, {total_json_bytes / 1e6:.2f} MB (JSON)\n")

    log_file = open(args.log_file, "w") if args.log_file else None

    # Untimed pre-pass: TOON text with ctoon, shared decode input for all
    for f in files:
        try:
            data = json.loads(f["json"])
            f["toon"] = ctoon.dumps(data)
        except Exception as e:
            log_fail(log_file, "ctoon", "pre_pass", f["path"], e)

    rows = []
    results = []
    scaling = []
    order_check = []

    def add_rows(name, json_to_toon_fn, toon_to_json_fn, roundtrip_fn):
        t_enc, ops_enc, bytes_enc = bench_json_to_toon(files, json_to_toon_fn, log_file, name)
        rows.append([
            name, "json_to_toon",
            f"{bytes_enc / t_enc / 1e6:.2f} MB/s" if ops_enc else "n/a",
            f"{ops_enc / t_enc:.0f}",
            f"{100 * ops_enc / (len(files) * REPEATS):.0f}%",
            f"{t_enc:.4f} s",
        ])
        results.append({
            "library": name, "operation": "json_to_toon",
            "throughput_mb_s": (bytes_enc / t_enc / 1e6) if ops_enc else 0.0,
            "docs_per_sec": ops_enc / t_enc, "success_rate": ops_enc / (len(files) * REPEATS),
            "total_time_s": t_enc,
        })

        t_dec, ops_dec, bytes_dec = bench_toon_to_json(files, toon_to_json_fn, log_file, name)
        rows.append([
            name, "toon_to_json",
            f"{bytes_dec / t_dec / 1e6:.2f} MB/s" if ops_dec else "n/a",
            f"{ops_dec / t_dec:.0f}",
            f"{100 * ops_dec / (len(files) * REPEATS):.0f}%",
            f"{t_dec:.4f} s",
        ])
        results.append({
            "library": name, "operation": "toon_to_json",
            "throughput_mb_s": (bytes_dec / t_dec / 1e6) if ops_dec else 0.0,
            "docs_per_sec": ops_dec / t_dec, "success_rate": ops_dec / (len(files) * REPEATS),
            "total_time_s": t_dec,
        })

        t_rt, ops_rt, bytes_rt = bench_roundtrip(files, roundtrip_fn, log_file, name)
        rows.append([
            name, "roundtrip",
            f"{bytes_rt / t_rt / 1e6:.2f} MB/s" if ops_rt else "n/a",
            f"{ops_rt / t_rt:.0f}",
            f"{100 * ops_rt / (len(files) * REPEATS):.0f}%",
            f"{t_rt:.4f} s",
        ])
        results.append({
            "library": name, "operation": "roundtrip",
            "throughput_mb_s": (bytes_rt / t_rt / 1e6) if ops_rt else 0.0,
            "docs_per_sec": ops_rt / t_rt, "success_rate": ops_rt / (len(files) * REPEATS),
            "total_time_s": t_rt,
        })

    # Scaling: same libraries, re-measured on SCALING_NBUCKETS equal-count
    # buckets of the corpus split by file size -- see bench.c's identical
    # scheme for the full rationale (one line chart point per bucket,
    # x = median file size in that bucket).
    sorted_files = sorted(files, key=lambda f: len(f["json"]))

    def add_scaling(name, json_to_toon_fn, toon_to_json_fn, roundtrip_fn):
        n = len(sorted_files)
        base, rem = divmod(n, SCALING_NBUCKETS)
        start = 0
        for b in range(SCALING_NBUCKETS):
            count = base + (1 if b < rem else 0)
            if count == 0:
                continue
            bucket = sorted_files[start:start + count]
            size_bytes = len(bucket[count // 2]["json"].encode("utf-8"))

            for op, fn, key in (
                ("json_to_toon", json_to_toon_fn, "json"),
                ("toon_to_json", toon_to_json_fn, "toon"),
                ("roundtrip", roundtrip_fn, "json"),
            ):
                runner = {"json_to_toon": bench_json_to_toon,
                          "toon_to_json": bench_toon_to_json,
                          "roundtrip": bench_roundtrip}[op]
                t, ops, bytes_done = runner(bucket, fn, None, name, reps=SCALING_REPS)
                scaling.append({
                    "library": name, "operation": op, "size_bytes": size_bytes,
                    "throughput_mb_s": (bytes_done / t / 1e6) if ops else 0.0,
                    "docs_per_sec": (ops / t) if t else 0.0,
                    "success_rate": ops / (count * SCALING_REPS) if count else 0.0,
                })
            start += count

    only = os.environ.get("CTOON_BENCH_ONLY", "").strip()

    add_rows(
        "ctoon",
        lambda text: ctoon.dumps(json.loads(text)),
        lambda text: ctoon.dumps_json(ctoon.loads(text), indent=2),
        lambda text: ctoon.dumps_json(ctoon.loads(ctoon.dumps(json.loads(text))), indent=2),
    ) if not only or only == "ctoon" else None
    if not only:
        add_scaling(
            "ctoon",
            lambda text: ctoon.dumps(json.loads(text)),
            lambda text: ctoon.dumps_json(ctoon.loads(text), indent=2),
            lambda text: ctoon.dumps_json(ctoon.loads(ctoon.dumps(json.loads(text))), indent=2),
        )
        preserved, detail = check_order_preserved(
            lambda text: ctoon.dumps_json(ctoon.loads(ctoon.dumps(json.loads(text))), indent=2))
        order_check.append({"library": "ctoon", "preserved": preserved, "detail": detail})

    if toon_format and (not only or only == "toon_format"):
        add_rows(
            "toon_format",
            lambda text: toon_format.encode(json.loads(text)),
            lambda text: json.dumps(toon_format.decode(text), indent=2),
            lambda text: json.dumps(toon_format.decode(toon_format.encode(json.loads(text))), indent=2),
        )
    if toon_format and not only:
        add_scaling(
            "toon_format",
            lambda text: toon_format.encode(json.loads(text)),
            lambda text: json.dumps(toon_format.decode(text), indent=2),
            lambda text: json.dumps(toon_format.decode(toon_format.encode(json.loads(text))), indent=2),
        )
        preserved, detail = check_order_preserved(
            lambda text: json.dumps(toon_format.decode(toon_format.encode(json.loads(text))), indent=2))
        order_check.append({"library": "toon_format", "preserved": preserved, "detail": detail})

    if toons and (not only or only == "toons"):
        add_rows(
            "toons",
            lambda text: toons.dumps(json.loads(text)),
            lambda text: toons.to_json(text, indent=2),
            lambda text: toons.to_json(toons.dumps(json.loads(text)), indent=2),
        )
    if toons and not only:
        add_scaling(
            "toons",
            lambda text: toons.dumps(json.loads(text)),
            lambda text: toons.to_json(text, indent=2),
            lambda text: toons.to_json(toons.dumps(json.loads(text)), indent=2),
        )
        preserved, detail = check_order_preserved(
            lambda text: toons.to_json(toons.dumps(json.loads(text)), indent=2))
        order_check.append({"library": "toons", "preserved": preserved, "detail": detail})

    if only:
        peak_kb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        print(f"CTOON_BENCH_MEM_RESULT library={only} peak_rss_kb={peak_kb}", file=sys.stderr)
        if log_file:
            log_file.close()
        return 0

    headers = ["Library", "Operation", "Throughput", "Docs/sec", "Success", f"Total time (x{REPEATS} reps)"]
    if tabulate:
        print(tabulate(rows, headers=headers))
    else:
        fmt = "{:<12} {:<14} {:>12} {:>10} {:>8} {:>24}"
        print(fmt.format(*headers))
        for row in rows:
            print(fmt.format(*row))

    with open(args.results_json, "w") as f:
        json.dump({
            "language": "python",
            "corpus": {"files": len(files), "bytes": total_json_bytes},
            "results": results,
            "scaling": scaling,
            "order_check": order_check,
        }, f, indent=2)
    print(f"\nResults written to {args.results_json}")

    if log_file:
        log_file.close()
        print(f"Debug log written to {args.log_file}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
