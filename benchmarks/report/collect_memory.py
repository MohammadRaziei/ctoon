"""
collect_memory.py — peak RSS per (language, library), each measured in
its own FRESH process (via the external `time -v` utility), then merged
in as a "peak_rss_mb" field on that library's rows in the language's
existing <language>.json (the same file bench_c/bench_cpp/bench.py
already wrote the throughput numbers into).

Why a fresh process per library: several language harnesses (currently
just Python; C similarly) run more than one library back-to-back in ONE
process for the normal timed run, for simplicity. ru_maxrss (what `time
-v` reports) is a whole-process HIGH-WATER MARK that never goes down, so
reading it after such a run would have every later library's number
inflated by whichever earlier library used the most memory -- not a
number about that library at all. Each language exposes a
CTOON_BENCH_ONLY=<library> environment variable (see bench.c's and
bench.py's docstrings) that runs just one library and skips writing the
normal results JSON, specifically so this script can invoke it in
isolation. C++ has only one library at all (ctoon -- no competing TOON
implementation for C++ exists), so its ordinary run is already isolated
and CTOON_BENCH_ONLY is irrelevant there.

peak_rss_mb is a per-(language, library) number, not per-operation --
one whole run (json_to_toon + toon_to_json + roundtrip together) has one
high-water mark. It's written onto every one of that library's rows for
readability in the existing per-row table, not because it's finer-
grained than that.

Any language block below is skipped if the relevant executable path
wasn't passed in (e.g. because that language's toolchain wasn't found at
configure time) -- same "only what's actually available" convention as
the rest of this benchmark suite.
"""
import argparse
import json
import os
import re
import subprocess
import sys

_RSS_RE = re.compile(r"Maximum resident set size \(kbytes\):\s*(\d+)")


def _peak_rss_mb(cmd, env_extra):
    env = os.environ.copy()
    env.update(env_extra)
    proc = subprocess.run(
        ["/usr/bin/time", "-v"] + cmd,
        env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
    )
    m = _RSS_RE.search(proc.stderr)
    if not m:
        raise RuntimeError(f"collect_memory: couldn't parse peak RSS from: {' '.join(cmd)}\n{proc.stderr}")
    return int(m.group(1)) / 1024.0


def _merge(results_json_path, peaks_by_library):
    with open(results_json_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    for row in data.get("results", []):
        if row["library"] in peaks_by_library:
            row["peak_rss_mb"] = round(peaks_by_library[row["library"]], 2)
    with open(results_json_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    return sorted(peaks_by_library.items())


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("results_dir")
    p.add_argument("--c-exe")
    p.add_argument("--c-manifest")
    p.add_argument("--cpp-exe")
    p.add_argument("--py-python")
    p.add_argument("--py-script")
    p.add_argument("--py-manifest")
    args = p.parse_args()

    if args.c_exe and args.c_manifest:
        peaks = {}
        for lib in ("ctoon", "TOONc"):
            peaks[lib] = _peak_rss_mb([args.c_exe], {"CTOON_BENCH_ONLY": lib})
        merged = _merge(os.path.join(args.results_dir, "c.json"), peaks)
        for lib, mb in merged:
            print(f"collect_memory: c/{lib} peak={mb:.2f}MB", file=sys.stderr)

    if args.cpp_exe:
        peak = _peak_rss_mb([args.cpp_exe], {})
        merged = _merge(os.path.join(args.results_dir, "cpp.json"), {"ctoon": peak})
        for lib, mb in merged:
            print(f"collect_memory: cpp/{lib} peak={mb:.2f}MB", file=sys.stderr)

    if args.py_python and args.py_script and args.py_manifest:
        peaks = {}
        for lib in ("ctoon", "toon_format", "toons"):
            cmd = [args.py_python, args.py_script, args.py_manifest, "/dev/null"]
            peaks[lib] = _peak_rss_mb(cmd, {"CTOON_BENCH_ONLY": lib})
        merged = _merge(os.path.join(args.results_dir, "python.json"), peaks)
        for lib, mb in merged:
            print(f"collect_memory: python/{lib} peak={mb:.2f}MB", file=sys.stderr)


if __name__ == "__main__":
    main()
