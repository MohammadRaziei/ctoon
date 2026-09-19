"""
generate_report.py — combine every benchmarks/*/<language>.json result
into one standalone HTML report: Chart.js and all the raw JSON data are
embedded directly in the file (via a fetched Chart.js UMD build and
inline <script> tags), so the output needs nothing else to view or to
keep -- no server, no network, no sibling files.

Modeled on https://github.com/MohammadRaziei/pygixml/blob/main/benchmarks/report/generate_report.py,
adapted for ctoon's per-language result files (language, corpus, results[]
with library/operation/throughput_mb_s/docs_per_sec/success_rate).

What this file deliberately does NOT contain: how the corpus was
generated, or why each language's numbers aren't comparable across
languages (different runtimes, different corpus-loading overhead --
see benchmarks/README.md). This file also never combines languages into
one number -- results stay grouped exactly as the JSON files keep them.
"""
import argparse
import datetime
import glob
import json
import os

from jinja2 import Environment, FileSystemLoader

HERE = os.path.dirname(os.path.abspath(__file__))

OPERATIONS = ["json_to_toon", "toon_to_json", "roundtrip"]
OP_TITLES = {
    "json_to_toon": "JSON \u2192 TOON",
    "toon_to_json": "TOON \u2192 JSON",
    "roundtrip": "Roundtrip (self-consistency)",
}
LANG_TITLES = {
    "c": "C", "cpp": "C++", "python": "Python", "go": "Go",
    "rust": "Rust", "zig": "Zig", "matlab": "MATLAB", "julia": "Julia",
}
PALETTE = ["#ff6b81", "#35d0ba", "#ffb454", "#b98bff", "#8b93a7", "#4dd0e1"]
CTOON_COLOR = "#5b8cff"  # fixed across every chart, in every language --
                          # not a special filtering rule, just a stable
                          # visual anchor so the same bar is always the
                          # same color from section to section.

ORDER_CHECK_SAMPLE = (
    '{"zebra": 1, "apple": 2, "mango": 3, "nested": {"beta": true, "alpha": false}, '
    '"list": [{"z": 1, "a": 2}, {"z": 3, "a": 4}]}'
)  # same literal string every language's bench runner checks against


def _load_all(results_dir):
    """Every *.json in results_dir except system_info.json is one
    language's result file (see benchmarks/CMakeLists.txt)."""
    langs = []
    for path in sorted(glob.glob(os.path.join(results_dir, "*.json"))):
        if os.path.basename(path) == "system_info.json":
            continue
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        if "language" in data and "results" in data:
            langs.append(data)
    langs.sort(key=lambda d: d["language"])
    return langs


def _lib_color(lib, lib_order):
    if lib == "ctoon":
        return CTOON_COLOR
    others = [l for l in lib_order if l != "ctoon"]
    idx = others.index(lib) if lib in others else 0
    return PALETTE[idx % len(PALETTE)]


def _fmt_size(n):
    if n >= 1024 * 1024:
        return f"{n / (1024 * 1024):.1f} MB"
    if n >= 1024:
        return f"{n / 1024:.1f} KB"
    return f"{n:.0f} B"


def _build_language_section(lang_data):
    """Three line charts per language, one per operation (json_to_toon,
    toon_to_json, roundtrip) -- x-axis is input size (median size of a
    corpus-size bucket, see bench.c/.cpp/.py's "scaling" pass), one line
    with markers per library, so the shape of the speed-vs-size curve is
    directly comparable across libraries. Falls back to no line charts
    for a language with no "scaling" data (nothing wired up yet)."""
    language = lang_data["language"]
    corpus = lang_data.get("corpus", {})
    results = lang_data["results"]
    scaling = lang_data.get("scaling", [])

    libs_seen = []
    for r in results:
        if r["library"] not in libs_seen:
            libs_seen.append(r["library"])

    # Aggregate bar chart: one dataset (bar) per library, x = operation,
    # values = throughput over the WHOLE corpus at full rep count (the
    # same numbers as the table below) -- the single-number summary,
    # shown above the size-scaling line charts rather than instead of them.
    present_ops = [op for op in OPERATIONS if any(r["operation"] == op for r in results)]
    agg_chart_id = f"chart-{language}-agg"
    agg_chart_js = ""
    if present_ops:
        agg_labels = [OP_TITLES.get(op, op) for op in present_ops]
        agg_datasets = []
        for lib in libs_seen:
            values = []
            for op in present_ops:
                match = next((r for r in results if r["library"] == lib and r["operation"] == op), None)
                values.append(round(match.get("throughput_mb_s", 0), 2) if match else None)
            agg_datasets.append({"label": lib, "data": values, "backgroundColor": _lib_color(lib, libs_seen)})
        agg_chart_js = f"""
new Chart(document.getElementById('{agg_chart_id}'), {{
  type: 'bar',
  data: {{ labels: {json.dumps(agg_labels)}, datasets: {json.dumps(agg_datasets)} }},
  options: {{
    indexAxis: 'x',
    responsive: true, maintainAspectRatio: false,
    scales: {{ y: {{ title: {{ display: true, text: 'MB/s (higher is better)' }}, beginAtZero: true }} }},
    plugins: {{ legend: {{ display: true, position: 'top' }} }}
  }}
}});
"""

    op_charts = []
    if scaling:
        # Bucket x-positions: same corpus, same split, so every library
        # lands on the same size labels -- pull them from whichever
        # library has the most points.
        sizes_by_op = {}
        for op in OPERATIONS:
            op_rows = [r for r in scaling if r["operation"] == op]
            if not op_rows:
                continue
            sizes = sorted({r["size_bytes"] for r in op_rows})
            sizes_by_op[op] = sizes

        for op in OPERATIONS:
            sizes = sizes_by_op.get(op)
            if not sizes:
                continue
            labels = [_fmt_size(s) for s in sizes]
            datasets = []
            for lib in libs_seen:
                lib_rows = {r["size_bytes"]: r for r in scaling
                            if r["library"] == lib and r["operation"] == op}
                if not lib_rows:
                    continue
                data = [round(lib_rows[s]["throughput_mb_s"], 2) if s in lib_rows else None for s in sizes]
                datasets.append({
                    "label": lib, "data": data,
                    "borderColor": _lib_color(lib, libs_seen),
                    "backgroundColor": _lib_color(lib, libs_seen),
                    "pointRadius": 4, "pointHoverRadius": 6, "tension": 0,
                })
            chart_id = f"chart-{language}-{op}"
            chart_js = f"""
new Chart(document.getElementById('{chart_id}'), {{
  type: 'line',
  data: {{ labels: {json.dumps(labels)}, datasets: {json.dumps(datasets)} }},
  options: {{
    responsive: true, maintainAspectRatio: false,
    scales: {{
      x: {{ title: {{ display: true, text: 'Input size (this bucket\\'s median file)' }} }},
      y: {{ title: {{ display: true, text: 'MB/s (higher is better)' }}, beginAtZero: true }}
    }},
    plugins: {{ legend: {{ display: true, position: 'top' }} }}
  }}
}});
"""
            op_charts.append({"key": op, "title": OP_TITLES.get(op, op),
                               "chart_id": chart_id, "chart_js": chart_js})

    # One peak-RSS number per library (see collect_memory.py -- it's a
    # whole-process high-water mark from an isolated run, not something
    # finer-grained than "per library"), plotted as its own small chart.
    # Only present at all when -DCTOON_BENCH_MEMORY=ON was used.
    mem_by_lib = {}
    for r in results:
        if r.get("peak_rss_mb") is not None:
            mem_by_lib[r["library"]] = r["peak_rss_mb"]

    memory_chart_js = ""
    if mem_by_lib:
        mem_libs = [l for l in libs_seen if l in mem_by_lib]
        mem_values = [mem_by_lib[l] for l in mem_libs]
        mem_colors = [_lib_color(l, libs_seen) for l in mem_libs]
        memory_chart_js = f"""
new Chart(document.getElementById('chart-{language}-mem'), {{
  type: 'bar',
  data: {{ labels: {json.dumps(mem_libs)}, datasets: [{{
    label: 'Peak RSS (MB)', data: {json.dumps(mem_values)},
    backgroundColor: {json.dumps(mem_colors)}
  }}] }},
  options: {{
    indexAxis: 'x',
    responsive: true, maintainAspectRatio: false,
    scales: {{ y: {{ title: {{ display: true, text: 'MB (lower is better)' }}, beginAtZero: true }} }},
    plugins: {{ legend: {{ display: false }} }}
  }}
}});
"""

    table_rows = []
    for op in OPERATIONS:
        for r in [x for x in results if x["operation"] == op]:
            table_rows.append({
                "operation": OP_TITLES.get(op, op),
                "library": r["library"],
                "throughput_mb_s": round(r.get("throughput_mb_s", 0), 2),
                "docs_per_sec": round(r.get("docs_per_sec", 0), 1),
                "success_rate": round(r.get("success_rate", 0) * 100, 1),
                "total_time_s": round(r.get("total_time_s", 0), 3),
                "peak_rss_mb": r.get("peak_rss_mb"),
            })

    has_memory = any(r.get("peak_rss_mb") is not None for r in table_rows)

    order_check = lang_data.get("order_check", [])

    return {
        "key": language,
        "title": LANG_TITLES.get(language, language),
        "corpus_files": corpus.get("files", 0),
        "corpus_bytes": corpus.get("bytes", 0),
        "agg_chart_id": agg_chart_id,
        "agg_chart_js": agg_chart_js,
        "op_charts": op_charts,
        "rows": table_rows,
        "has_memory": has_memory,
        "memory_chart_id": f"chart-{language}-mem",
        "memory_chart_js": memory_chart_js,
        "order_check": order_check,
    }


def _load_system_info(results_dir):
    path = os.path.join(results_dir, "system_info.json")
    if not os.path.exists(path):
        return None
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def build(results_dir, output_path, chartjs_path):
    langs = _load_all(results_dir)
    sections = [_build_language_section(d) for d in langs]
    system_info = _load_system_info(results_dir)

    with open(chartjs_path, "r", encoding="utf-8") as f:
        chartjs_source = f.read()

    chart_scripts = []
    for sec in sections:
        if sec["agg_chart_js"]:
            chart_scripts.append(sec["agg_chart_js"])
        for oc in sec["op_charts"]:
            chart_scripts.append(oc["chart_js"])
        if sec["has_memory"]:
            chart_scripts.append(sec["memory_chart_js"])

    embedded = {d["language"]: d for d in langs}

    env = Environment(loader=FileSystemLoader(HERE), autoescape=False)
    template = env.get_template("template.html.jinja2")

    html = template.render(
        generated_at=datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
        headline="ctoon, measured honestly",
        subhead=(
            "Every ctoon language binding &mdash; C, C++, Python, Go, Rust, Zig, "
            "MATLAB &mdash; benchmarked head-to-head against every maintained "
            "competing TOON implementation that exists for that language."
        ),
        sections=sections,
        chartjs_source=chartjs_source,
        embedded_json=json.dumps(embedded),
        chart_scripts="\n".join(chart_scripts),
        system_info=system_info,
        order_check_sample=ORDER_CHECK_SAMPLE,
    )

    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(html)

    return output_path


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("results_dir", help="directory containing <language>.json result files")
    p.add_argument("output", help="path to write the standalone report.html")
    p.add_argument("--chartjs-path", required=True, help="path to a Chart.js UMD build")
    args = p.parse_args()

    out = build(args.results_dir, args.output, args.chartjs_path)
    size_kb = os.path.getsize(out) / 1024
    print(f"generate_report: wrote {out} ({size_kb:.0f}KB, standalone)")
