#!/usr/bin/env python3
"""Merge the toon-zig rows into zig.json (written by the ctoon Zig harness).

usage: merge_results.py <zig.json> <toon-zig.partial.json>

The two harnesses are built by different Zig versions, so they are separate
executables, but the report has one table per language: toon-zig's rows are
added to zig.json as another library. Rows of the same library already in
zig.json are replaced, so running this twice does not duplicate anything.
"""
import json
import sys

dst_path, src_path = sys.argv[1], sys.argv[2]
with open(dst_path, encoding="utf-8") as f:
    dst = json.load(f)
with open(src_path, encoding="utf-8") as f:
    src = json.load(f)

libs = {r["library"] for r in src["results"]}
dst["results"] = [r for r in dst["results"] if r["library"] not in libs] + src["results"]
# key-order rows (one per library and direction) are merged the same way
order_libs = {r["library"] for r in src.get("order_check", [])}
dst["order_check"] = [r for r in dst.get("order_check", []) if r["library"] not in order_libs] + src.get("order_check", [])

with open(dst_path, "w", encoding="utf-8") as f:
    json.dump(dst, f, indent=2)
    f.write("\n")
print(f"merged {len(src['results'])} rows ({', '.join(sorted(libs))}) into {dst_path}")
