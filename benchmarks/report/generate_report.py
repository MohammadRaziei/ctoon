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
    "rust": "Rust", "zig": "Zig", "matlab": "MATLAB",
    "node": "Node.js", "julia": "Julia",
}
PALETTE = ["#5b8cff", "#ff6b81", "#35d0ba", "#ffb454", "#b98bff", "#8b93a7", "#4dd0e1"]


def _load_all(results_dir):
    """Every *.json in results_dir except the corpus manifest is one
    language's result file (see benchmarks/CMakeLists.txt)."""
    langs = []
    for path in sorted(glob.glob(os.path.join(results_dir, "*.json"))):
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        if "language" in data and "results" in data:
            langs.append(data)
    langs.sort(key=lambda d: d["language"])
    return langs


def _lib_color(lib, lib_order):
    idx = lib_order.index(lib) if lib in lib_order else 0
    return PALETTE[idx % len(PALETTE)]


def _build_language_section(lang_data):
    """One chart per language: x-axis = operation (json_to_toon,
    toon_to_json, roundtrip), one dataset (bar series) per library, so
    all three operations sit side by side and every library --
    including ctoon itself -- is just one more bar, not singled out."""
    language = lang_data["language"]
    corpus = lang_data.get("corpus", {})
    results = lang_data["results"]

    libs_seen = []
    for r in results:
        if r["library"] not in libs_seen:
            libs_seen.append(r["library"])

    present_ops = [op for op in OPERATIONS if any(r["operation"] == op for r in results)]
    op_labels = [OP_TITLES.get(op, op) for op in present_ops]

    datasets = []
    for lib in libs_seen:
        values = []
        for op in present_ops:
            match = next((r for r in results if r["library"] == lib and r["operation"] == op), None)
            values.append(round(match.get("throughput_mb_s", 0), 2) if match else None)
        datasets.append({"label": lib, "data": values, "backgroundColor": _lib_color(lib, libs_seen)})

    chart_js = f"""
new Chart(document.getElementById('chart-{language}'), {{
  type: 'bar',
  data: {{ labels: {json.dumps(op_labels)}, datasets: {json.dumps(datasets)} }},
  options: {{
    indexAxis: 'x',
    responsive: true, maintainAspectRatio: false,
    scales: {{ y: {{ title: {{ display: true, text: 'MB/s (higher is better)' }}, beginAtZero: true }} }},
    plugins: {{ legend: {{ display: true, position: 'top' }} }}
  }}
}});
"""

    table_rows = []
    for op in present_ops:
        for r in [x for x in results if x["operation"] == op]:
            table_rows.append({
                "operation": OP_TITLES.get(op, op),
                "library": r["library"],
                "throughput_mb_s": round(r.get("throughput_mb_s", 0), 2),
                "docs_per_sec": round(r.get("docs_per_sec", 0), 1),
                "success_rate": round(r.get("success_rate", 0) * 100, 1),
                "total_time_s": round(r.get("total_time_s", 0), 3),
            })

    return {
        "key": language,
        "title": LANG_TITLES.get(language, language),
        "corpus_files": corpus.get("files", 0),
        "corpus_bytes": corpus.get("bytes", 0),
        "chart_id": f"chart-{language}",
        "chart_js": chart_js,
        "rows": table_rows,
    }


def build(results_dir, output_path, chartjs_path):
    langs = _load_all(results_dir)
    sections = [_build_language_section(d) for d in langs]

    with open(chartjs_path, "r", encoding="utf-8") as f:
        chartjs_source = f.read()

    chart_scripts = []
    for sec in sections:
        chart_scripts.append(sec["chart_js"])

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
