#!/usr/bin/env python3
"""
convert_page.py — CToon Markdown → HTML page converter.

Usage:
    python3 convert_page.py
        --input     <source.md>
        --output    <out/index.html>
        --title     "Page Title"
        --nav-label "Short Label"
        --template  <page.html.in>
        --css       <ctoon-docs.css>
        --logo      <ctoon-sq.svg>
        --version   "1.0.0"
"""

import argparse
import os
import re
import sys
from pathlib import Path

# ── Dependency check ──────────────────────────────────────────────────────────
try:
    from markdown_it import MarkdownIt
    from markdown_it.rules_block import fence
except ImportError:
    print("ERROR: markdown-it-py is not installed.\n"
          "Run: pip install markdown-it-py", file=sys.stderr)
    sys.exit(1)


# ── Markdown → HTML ───────────────────────────────────────────────────────────
def render_markdown(md_text: str) -> str:
    """Convert Markdown to HTML using markdown-it-py with common plugins."""
    try:
        # Try to load extras (tables, strikethrough, etc.)
        from mdit_py_plugins.front_matter import front_matter_plugin
        from mdit_py_plugins.admon import admon_plugin
        md = (
            MarkdownIt("commonmark", {"highlight": highlight_code})
            .enable("table")
            .enable("strikethrough")
        )
        try:
            md = md.use(front_matter_plugin)
        except Exception:
            pass
        try:
            md = md.use(admon_plugin)
        except Exception:
            pass
    except ImportError:
        md = MarkdownIt("commonmark", {"highlight": highlight_code}).enable("table")

    return md.render(md_text)


def highlight_code(code: str, lang: str, attrs: str) -> str:
    """Wrap code blocks so highlight.js picks them up."""
    lang = lang.strip() if lang else "plaintext"
    escaped = (code
               .replace("&", "&amp;")
               .replace("<", "&lt;")
               .replace(">", "&gt;"))
    return (
        f'<div class="code-wrap">'
        f'<div class="code-label">'
        f'<div class="code-lang-row">'
        f'<div class="code-dots"><span></span><span></span><span></span></div>'
        f'<span class="code-lang">{lang}</span>'
        f'</div>'
        f'<button class="copy-btn" onclick="copyCode(this)">'
        f'<i class="fa-regular fa-copy"></i> copy</button>'
        f'</div>'
        f'<pre><code class="language-{lang}">{escaped}</code></pre>'
        f'</div>'
    )


# ── Extract title from first H1 (optional) ───────────────────────────────────
def extract_h1(md_text: str) -> str | None:
    m = re.search(r'^#\s+(.+)$', md_text, re.MULTILINE)
    return m.group(1).strip() if m else None


# ── Generate breadcrumb path ─────────────────────────────────────────────────
def make_breadcrumbs(nav_label: str) -> str:
    return (
        f'<nav class="page-breadcrumb">'
        f'<a href="../index.html">Docs</a>'
        f'<span class="page-breadcrumb-sep">›</span>'
        f'<span>{nav_label}</span>'
        f'</nav>'
    )


# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="CToon Markdown → HTML converter")
    parser.add_argument("--input",     required=True, help="Input .md file")
    parser.add_argument("--output",    required=True, help="Output index.html path")
    parser.add_argument("--title",     required=True, help="Page title")
    parser.add_argument("--nav-label", required=True, help="Short nav label")
    parser.add_argument("--template",  required=True, help="page.html.in template")
    parser.add_argument("--css",       required=True, help="ctoon-docs.css path")
    parser.add_argument("--logo",      required=False, default="", help="SVG logo path (optional)")
    parser.add_argument("--version",   required=True, help="Project version")
    args = parser.parse_args()

    # Read inputs
    md_text      = Path(args.input).read_text(encoding="utf-8")
    template     = Path(args.template).read_text(encoding="utf-8")
    css          = Path(args.css).read_text(encoding="utf-8")
    # Read and embed SVG logo (optional)
    logo_content = ""
    if args.logo:
        logo_path = Path(args.logo)
        if logo_path.exists():
            logo_content = logo_path.read_text(encoding="utf-8").strip()
        else:
            print(f"[ctoon] Warning: logo file not found: {args.logo}", file=sys.stderr)

    # Render Markdown
    body_html = render_markdown(md_text)

    # Build breadcrumbs
    breadcrumbs = make_breadcrumbs(args.nav_label)

    # Substitute template placeholders
    html = template
    html = html.replace("@PAGE_TITLE@",       args.title)
    html = html.replace("@NAV_LABEL@",        args.nav_label)
    html = html.replace("@PROJECT_VERSION@",  args.version)
    html = html.replace("@LOGO_CONTENT@",     logo_content)
    html = html.replace("@CSS_CONTENT@",      css)
    html = html.replace("@BREADCRUMBS@",      breadcrumbs)
    html = html.replace("@BODY_CONTENT@",     body_html)

    # Write output
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(html, encoding="utf-8")

    print(f"[ctoon] Page built: {out_path}")


if __name__ == "__main__":
    main()