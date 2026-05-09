#!/usr/bin/env python3
"""
convert_page.py — CToon Markdown → HTML page converter.

Single page mode:
    python3 convert_page.py
        --input     <source.md>
        --output    <out/index.html>
        --title     "Page Title"
        --nav-label "Short Label"
        --template  <page.html.in>
        --css       <ctoon-docs.css>
        [--logo     <ctoon-sq.svg>]
        --version   "1.0.0"

Section mode (multi-page):
    python3 convert_page.py --section
        --section-title  "CLI Reference"
        --section-dir    <out/cli/>
        --section-pages  "index.md:Overview,installation.md:Installation,..."
        --template       <section.html.in>
        --css            <ctoon-docs.css>
        [--logo          <ctoon-sq.svg>]
        --version        "1.0.0"
        --source-dir     <docs/>     # base dir for resolving md paths
"""

import argparse
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
        from mdit_py_plugins.front_matter import front_matter_plugin
        from mdit_py_plugins.admon import admon_plugin
        md = MarkdownIt("commonmark").enable("table").enable("strikethrough")
        try:
            md = md.use(front_matter_plugin)
        except Exception:
            pass
        try:
            md = md.use(admon_plugin)
        except Exception:
            pass
    except ImportError:
        md = MarkdownIt("commonmark").enable("table")

    raw_html = md.render(md_text)

    # Post-process: wrap every <pre><code class="language-X">...</code></pre>
    # with our custom code-wrap chrome (dots + lang label + copy button)
    raw_html = _wrap_code_blocks(raw_html)
    return raw_html


def _wrap_code_blocks(html: str) -> str:
    """Replace bare <pre><code ...> blocks with styled code-wrap divs."""
    import re

    def replacer(m):
        attrs      = m.group(1) or ''
        code_body  = m.group(2)

        # Extract language name from class e.g. class="language-bash" → bash
        lang_match = re.search(r'language-([a-zA-Z0-9_+-]+)', attrs)
        lang = lang_match.group(1) if lang_match else 'plaintext'

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
            f'<pre><code class="language-{lang}">{code_body}</code></pre>'
            f'</div>'
        )

    # Match <pre><code class="...">...</code></pre>  (non-greedy, DOTALL)
    pattern = re.compile(
        r'<pre><code((?:\s+[^>]*)?)?>(.*?)</code></pre>',
        re.DOTALL
    )
    return pattern.sub(replacer, html)


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


# ── Read logo ────────────────────────────────────────────────────────────────
def read_logo(logo_path: str) -> str:
    if not logo_path:
        return ''
    p = Path(logo_path)
    if p.exists():
        return p.read_text(encoding='utf-8').strip()
    print(f"[ctoon] Warning: logo file not found: {logo_path}", file=sys.stderr)
    return ''


# ── Single page ───────────────────────────────────────────────────────────────
def build_page(args):
    md_text      = Path(args.input).read_text(encoding='utf-8')
    template     = Path(args.template).read_text(encoding='utf-8')
    css          = Path(args.css).read_text(encoding='utf-8')
    logo_content = read_logo(args.logo)

    body_html   = render_markdown(md_text)
    breadcrumbs = make_breadcrumbs(args.nav_label)

    html = template
    html = html.replace('@PAGE_TITLE@',      args.title)
    html = html.replace('@NAV_LABEL@',       args.nav_label)
    html = html.replace('@PROJECT_VERSION@', args.version)
    html = html.replace('@LOGO_CONTENT@',    logo_content)
    html = html.replace('@CSS_CONTENT@',     css)
    html = html.replace('@BREADCRUMBS@',     breadcrumbs)
    html = html.replace('@BODY_CONTENT@',    body_html)
    html = html.replace('@FAVICON@',         '../ctoon-sq.svg')

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(html, encoding='utf-8')
    print(f"[ctoon] Page built: {out_path}")


# ── Section mode ──────────────────────────────────────────────────────────────
def build_section(args):
    """Build a multi-page section with sidebar nav and prev/next buttons."""
    template     = Path(args.template).read_text(encoding='utf-8')
    css          = Path(args.css).read_text(encoding='utf-8')
    logo_content = read_logo(args.logo)
    section_dir  = Path(args.section_dir)
    source_dir   = Path(args.source_dir) if args.source_dir else Path('.')
    section_dir.mkdir(parents=True, exist_ok=True)

    # Parse page list: "file.md:Title,file2.md:Title 2,..."
    pages = []  # list of (md_path, title, output_filename)
    for entry in args.section_pages.split(','):
        entry = entry.strip()
        if ':' in entry:
            md_file, title = entry.split(':', 1)
        else:
            md_file, title = entry, Path(entry).stem.replace('-', ' ').title()
        md_path = source_dir / md_file.strip()
        # index.md → index.html, installation.md → installation.html
        out_name = Path(md_file.strip()).stem + '.html'
        pages.append((md_path, title.strip(), out_name))

    # Build each page
    for i, (md_path, title, out_name) in enumerate(pages):
        md_text  = md_path.read_text(encoding='utf-8')
        body_html = render_markdown(md_text)

        # ── Sidebar nav ──────────────────────────────────────────────────────
        nav_items = []
        for j, (_, nav_title, nav_out) in enumerate(pages):
            active = 'active' if j == i else ''
            nav_items.append(
                f'<li><a href="{nav_out}" class="{active}">'
                f'<span class="nav-index">{j + 1:02d}</span>'
                f'{nav_title}</a></li>'
            )
        section_nav = '\n'.join(nav_items)

        # ── Breadcrumb ───────────────────────────────────────────────────────
        breadcrumbs = (
            f'<nav class="page-breadcrumb">'
            f'<a href="../index.html">Docs</a>'
            f'<span class="page-breadcrumb-sep">›</span>'
            f'<a href="index.html">{args.section_title}</a>'
            f'<span class="page-breadcrumb-sep">›</span>'
            f'<span>{title}</span>'
            f'</nav>'
        )

        # ── Prev / Next buttons ──────────────────────────────────────────────
        def nav_btn(direction: str, idx: int) -> str:
            if idx < 0 or idx >= len(pages):
                return f'<div class="page-nav-btn page-nav-empty"></div>'
            _, btn_title, btn_out = pages[idx]
            icon_prev = '<i class="fa-solid fa-arrow-left"></i>'
            icon_next = '<i class="fa-solid fa-arrow-right"></i>'
            if direction == 'prev':
                return (
                    f'<a href="{btn_out}" class="page-nav-btn page-nav-prev">'
                    f'<span class="page-nav-direction">{icon_prev} Previous</span>'
                    f'<span class="page-nav-label">{btn_title}</span>'
                    f'</a>'
                )
            else:
                return (
                    f'<a href="{btn_out}" class="page-nav-btn page-nav-next">'
                    f'<span class="page-nav-direction">Next {icon_next}</span>'
                    f'<span class="page-nav-label">{btn_title}</span>'
                    f'</a>'
                )

        prev_btn = nav_btn('prev', i - 1)
        next_btn = nav_btn('next', i + 1)

        # ── Render ───────────────────────────────────────────────────────────
        html = template
        html = html.replace('@PAGE_TITLE@',      title)
        html = html.replace('@SECTION_TITLE@',   args.section_title)
        html = html.replace('@PROJECT_VERSION@', args.version)
        html = html.replace('@LOGO_CONTENT@',    logo_content)
        html = html.replace('@CSS_CONTENT@',     css)
        html = html.replace('@BREADCRUMBS@',     breadcrumbs)
        html = html.replace('@SECTION_NAV@',     section_nav)
        html = html.replace('@BODY_CONTENT@',    body_html)
        html = html.replace('@PREV_BTN@',        prev_btn)
        html = html.replace('@NEXT_BTN@',        next_btn)
        html = html.replace('@FAVICON@',         '../ctoon-sq.svg')

        out_path = section_dir / out_name
        out_path.write_text(html, encoding='utf-8')
        print(f"[ctoon] Section page built: {out_path}")


# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="CToon Markdown → HTML converter")

    # Mode
    parser.add_argument('--section', action='store_true', help='Build a multi-page section')

    # Single page args
    parser.add_argument('--input',     help='Input .md file (single page mode)')
    parser.add_argument('--output',    help='Output HTML path (single page mode)')
    parser.add_argument('--title',     help='Page title')
    parser.add_argument('--nav-label', help='Short nav label (single page mode)')

    # Section args
    parser.add_argument('--section-title', help='Section title shown in sidebar and topbar')
    parser.add_argument('--section-dir',   help='Output directory for the section')
    parser.add_argument('--section-pages', help='Comma-separated list of "file.md:Title" entries')
    parser.add_argument('--source-dir',    help='Base directory for resolving md paths')

    # Shared args
    parser.add_argument('--template', required=True, help='HTML template (.html.in)')
    parser.add_argument('--css',      required=True, help='ctoon-docs.css path')
    parser.add_argument('--logo',     required=False, default='', help='SVG logo path (optional)')
    parser.add_argument('--version',  required=True, help='Project version')

    args = parser.parse_args()

    if args.section:
        for req in ('section_title', 'section_dir', 'section_pages'):
            if not getattr(args, req):
                parser.error(f'--{req.replace("_", "-")} is required in section mode')
        build_section(args)
    else:
        for req in ('input', 'output', 'title', 'nav_label'):
            if not getattr(args, req):
                parser.error(f'--{req.replace("_", "-")} is required in single page mode')
        build_page(args)


if __name__ == '__main__':
    main()