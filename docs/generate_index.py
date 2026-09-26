#!/usr/bin/env python3
"""Generate docs/index.html from docs/index.html.in.

Reads docs/supported_langs.json (the language registry) plus, per language,
  <folder>/index-docs.json     structured API-reference card fields
  <folder>/index-install.txt   raw HTML for the install panel
  <folder>/index-example.txt   raw HTML for the quick-example panel
and substitutes the @DOCS_TABS@ / @DOCS_CONTENT@ / @INSTALL_TABS@ /
@INSTALL_CONTENT@ / @EXAMPLE_TABS@ / @EXAMPLE_CONTENT@ placeholders (plus
@PROJECT_VERSION@ / @LOGO_CONTENT@ / @FAVICON@) in index.html.in.

Invoked by docs/CreateDocs.cmake (which also copies ctoon-docs.css and the
logo/favicon into place) — see that file for the exact command line. Can
also be run directly for local iteration, e.g.:

    python3 generate_index.py \
        --index-in index.html.in --output /tmp/preview/index.html \
        --langs-file supported_langs.json --langs-dir . \
        --project-version 0.7.0 --logo-svg images/ctoon-sq.svg
"""
import argparse
import json
import sys
from pathlib import Path

PANELS = ("docs", "install", "example")

# Named SVG glyphs for languages without a suitable Font Awesome icon
# (see supported_langs.json's icon.kind == "svg").
ICON_SVG = {
    "layers": (
        '<svg class="tab-svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" '
        'stroke-width="2" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg>'
    ),
}

DOC_CARD_TEMPLATE = """<div class="doc-card-inner">
    <div class="doc-card-icon">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg>
    </div>
    <div class="doc-card-body">
        <div class="doc-card-title">{title} <span class="card-badge">{badge}</span></div>
        <p class="doc-card-desc">{description}</p>
        <div class="doc-card-meta"><span class="doc-meta-item"><i class="{meta_icon}"></i> {meta_label}</span></div>
        <a href="{link_href}" class="doc-card-btn">{link_label} <i class="fa-solid fa-arrow-right"></i></a>
    </div>
</div>
"""

DOC_FIELDS = ("title", "badge", "description", "meta_icon", "meta_label", "link_href", "link_label")


def icon_html(icon: dict, folder: str) -> str:
    kind, value = icon.get("kind"), icon.get("value")
    if kind == "fa":
        return f'<i class="{value}"></i>'
    if kind == "svg":
        try:
            return ICON_SVG[value]
        except KeyError:
            sys.exit(f"generate_index.py: unknown svg icon {value!r} for language {folder!r}")
    sys.exit(f"generate_index.py: unknown icon kind {kind!r} for language {folder!r} (expected 'fa' or 'svg')")


def tab_button(panel: str, folder: str, name: str, icon: dict, active: bool) -> str:
    cls = "tab-btn active" if active else "tab-btn"
    return (
        f'            <button class="{cls}" data-tab="{panel}-{folder}" '
        f"onclick=\"switchTab('{panel}','{folder}')\">\n"
        f"                {icon_html(icon, folder)} {name}\n"
        f"            </button>\n"
    )


def tab_content(panel: str, folder: str, inner_html: str, active: bool) -> str:
    cls = "tab-content active" if active else "tab-content"
    return f'        <div class="{cls}" id="{panel}-{folder}">\n{inner_html}        </div>\n\n'


def load_docs_card(lang_dir: Path, folder: str) -> str:
    path = lang_dir / "index-docs.json"
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        sys.exit(f"generate_index.py: failed to read {path}: {exc}")
    missing = [f for f in DOC_FIELDS if f not in data]
    if missing:
        sys.exit(f"generate_index.py: {path} is missing field(s): {', '.join(missing)}")
    return DOC_CARD_TEMPLATE.format(**{f: data[f] for f in DOC_FIELDS})


def load_raw_fragment(lang_dir: Path, panel: str) -> str:
    path = lang_dir / f"index-{panel}.txt"
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        sys.exit(f"generate_index.py: failed to read {path}: {exc}")


def build(args: argparse.Namespace) -> str:
    langs_file = Path(args.langs_file)
    langs_dir = Path(args.langs_dir)
    try:
        registry = json.loads(langs_file.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        sys.exit(f"generate_index.py: failed to read {langs_file}: {exc}")

    languages = registry["languages"]
    tabs = {panel: [] for panel in PANELS}
    content = {panel: [] for panel in PANELS}

    for i, lang in enumerate(languages):
        folder, name, icon = lang["folder"], lang["name"], lang["icon"]
        lang_dir = langs_dir / folder
        active = i == 0  # CLI, first in the registry, opens active

        for panel in PANELS:
            tabs[panel].append(tab_button(panel, folder, name, icon, active))

        docs_html = load_docs_card(lang_dir, folder)
        content["docs"].append(tab_content("docs", folder, docs_html, active))
        for panel in ("install", "example"):
            frag = load_raw_fragment(lang_dir, panel)
            content[panel].append(tab_content(panel, folder, frag, active))

    index_content = Path(args.index_in).read_text(encoding="utf-8")
    for panel in PANELS:
        index_content = index_content.replace(f"@{panel.upper()}_TABS@", "".join(tabs[panel]))
        index_content = index_content.replace(f"@{panel.upper()}_CONTENT@", "".join(content[panel]))

    index_content = index_content.replace("@PROJECT_VERSION@", args.project_version)
    logo_content = Path(args.logo_svg).read_text(encoding="utf-8") if args.logo_svg else ""
    index_content = index_content.replace("@LOGO_CONTENT@", logo_content)
    index_content = index_content.replace("@FAVICON@", args.favicon)
    return index_content


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--index-in", required=True, help="path to index.html.in")
    ap.add_argument("--output", required=True, help="path to write the generated index.html")
    ap.add_argument("--langs-file", required=True, help="path to supported_langs.json")
    ap.add_argument("--langs-dir", required=True, help="directory containing each <folder>/ subdirectory")
    ap.add_argument("--project-version", required=True)
    ap.add_argument("--logo-svg", default="", help="path to the inline logo SVG (embedded verbatim)")
    ap.add_argument("--favicon", default="ctoon-sq.svg")
    args = ap.parse_args()

    output = build(args)
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(output, encoding="utf-8")
    print(f"-- Route index created at: {out_path}")


if __name__ == "__main__":
    main()
