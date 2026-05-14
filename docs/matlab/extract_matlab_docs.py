#!/usr/bin/env python3
"""
extract_matlab_docs.py — Extract MATLAB docstrings and generate RST files.

Parses standard MATLAB help-text format (H1 line + body comment block)
from .m source files and writes one RST file per input file plus an
api.rst index that references them all.

Usage:
    python3 extract_matlab_docs.py
        --sources  <file1.m> [<file2.m> ...]
        --out-dir  <output_directory>
        --version  <version_string>

Output (in --out-dir):
    api.rst               top-level API reference (includes all functions)
    ctoon_encode.rst
    ctoon_decode.rst
    ...
"""

import argparse
import re
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# MATLAB docstring parser
# ---------------------------------------------------------------------------

def parse_m_file(path: Path) -> dict:
    """
    Parse a MATLAB .m file and extract docstring components.

    Returns a dict with keys:
        name        str   function name  (from H1 line, e.g. CTOON_ENCODE)
        h1          str   one-line summary (rest of H1 after the name)
        syntax      list  lines of the syntax block (e.g. STR = CTOON_ENCODE(VALUE))
        sections    list  of (heading: str, lines: list[str]) tuples
        see_also    list  of function name strings
        raw_body    str   full docstring body (for fallback)
    """
    text = path.read_text(encoding='utf-8')
    lines = text.splitlines()

    # ---- collect comment block (starts after function declaration) ----------
    comment_lines = []
    in_block = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('function '):
            in_block = True
            continue
        if in_block:
            if stripped.startswith('%'):
                comment_lines.append(stripped[1:])  # strip leading %
            else:
                break  # first non-comment line ends the block

    if not comment_lines:
        return {'name': path.stem, 'h1': '', 'syntax': [],
                'sections': [], 'see_also': [], 'raw_body': ''}

    # ---- H1 line ------------------------------------------------------------
    h1_raw = comment_lines[0].strip()
    # H1 format: "FUNCNAME  Brief description"  (two or more spaces separate)
    h1_match = re.match(r'^([A-Z0-9_]+)\s{2,}(.+)$', h1_raw)
    if h1_match:
        func_name = h1_match.group(1)
        h1_summary = h1_match.group(2).strip()
    else:
        func_name = path.stem.upper()
        h1_summary = h1_raw

    # ---- body lines (strip common leading whitespace) -----------------------
    body_lines = [ln.rstrip() for ln in comment_lines[1:]]

    # Remove leading blank line after H1
    while body_lines and body_lines[0].strip() == '':
        body_lines.pop(0)

    # ---- split into labelled sections ---------------------------------------
    # Recognise lines like "   Syntax:", "   Example:", "   Type mapping:", etc.
    # and "   See also: ..." as section starts.
    section_re = re.compile(r'^\s{0,4}([A-Z][A-Za-z0-9 _-]+):\s*$')
    see_also_re = re.compile(r'^\s*See also:\s*(.+)$', re.IGNORECASE)

    sections = []       # [(heading, [lines])]
    syntax_lines = []   # lines before the first labelled section
    see_also = []

    current_heading = None
    current_lines = []
    pre_section = True   # True until we hit the first labelled section

    for ln in body_lines:
        sa_match = see_also_re.match(ln)
        if sa_match:
            if current_heading is not None:
                sections.append((current_heading, current_lines))
            # parse comma/space separated names; strip formatting
            raw = sa_match.group(1)
            see_also = [n.strip().rstrip('.') for n in re.split(r'[,\s]+', raw) if n.strip()]
            current_heading = None
            current_lines = []
            pre_section = False
            continue

        sec_match = section_re.match(ln)
        if sec_match:
            if pre_section:
                syntax_lines = current_lines[:]
                pre_section = False
            elif current_heading is not None:
                sections.append((current_heading, current_lines))
            current_heading = sec_match.group(1)
            current_lines = []
            continue

        current_lines.append(ln)

    # flush last section
    if current_heading is not None and current_lines:
        sections.append((current_heading, current_lines))
    elif pre_section:
        syntax_lines = current_lines[:]

    # tidy syntax_lines: remove blank lines at edges, strip 3-space indent
    syntax_lines = [ln[3:] if ln.startswith('   ') else ln for ln in syntax_lines]
    while syntax_lines and syntax_lines[0].strip() == '':
        syntax_lines.pop(0)
    while syntax_lines and syntax_lines[-1].strip() == '':
        syntax_lines.pop()

    raw_body = '\n'.join(body_lines)

    return {
        'name':     func_name,
        'h1':       h1_summary,
        'syntax':   syntax_lines,
        'sections': sections,
        'see_also': see_also,
        'raw_body': raw_body,
    }


# ---------------------------------------------------------------------------
# RST renderer
# ---------------------------------------------------------------------------

def _indent(lines: list, n: int = 3) -> list:
    pad = ' ' * n
    return [pad + ln if ln.strip() else '' for ln in lines]


def _section_body_to_rst(heading: str, lines: list) -> list:
    """Convert a docstring section body to RST lines."""
    out = []
    # Detect code blocks: lines where every non-blank line starts with spaces
    # (4+ spaces relative to section indent level)
    in_code = False

    for ln in lines:
        # strip up to 3 leading spaces (section indent)
        if ln.startswith('   '):
            content = ln[3:]
        else:
            content = ln

        # Heuristic: lines starting with more spaces = code/example
        if content.startswith('  ') and content.strip():
            if not in_code:
                out.append('')
                out.append('.. code-block:: matlab')
                out.append('')
                in_code = True
            out.append('   ' + content.lstrip())
        else:
            if in_code and content.strip():
                in_code = False
                out.append('')
            out.append(content)

    return out


def doc_to_rst(doc: dict, stem: str) -> str:
    """Render a parsed docstring dict to RST."""
    lines = []

    # ---- Title --------------------------------------------------------------
    title = f'``{stem}``'
    lines.append(title)
    lines.append('-' * len(title))
    lines.append('')

    # ---- H1 summary ---------------------------------------------------------
    lines.append(doc['h1'])
    lines.append('')

    # ---- Syntax block -------------------------------------------------------
    if doc['syntax']:
        lines.append('.. code-block:: matlab')
        lines.append('')
        for ln in doc['syntax']:
            lines.append('   ' + ln if ln.strip() else '')
        lines.append('')

    # ---- Sections -----------------------------------------------------------
    for heading, body_lines in doc['sections']:
        lines.append(heading)
        lines.append('~' * len(heading))
        lines.append('')
        rst_body = _section_body_to_rst(heading, body_lines)
        lines.extend(rst_body)
        lines.append('')

    # ---- See also -----------------------------------------------------------
    if doc['see_also']:
        lines.append('See also')
        lines.append('~~~~~~~~')
        lines.append('')
        for name in doc['see_also']:
            lower = name.lower()
            lines.append(f':func:`{lower}`')
        lines.append('')

    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Top-level api.rst builder
# ---------------------------------------------------------------------------

def build_api_rst(docs: list, version: str) -> str:
    """Build the top-level api.rst — inlines all function docs directly.

    Uses inline content instead of ``.. include::`` directives so that
    docutils can render the file standalone without resolving external files.
    """
    lines = []
    lines.append('API Reference')
    lines.append('=============')
    lines.append('')
    lines.append(f'CToon MATLAB binding v{version}.')
    lines.append('')
    lines.append('.. contents:: Functions')
    lines.append('   :local:')
    lines.append('   :depth: 1')
    lines.append('')

    for doc, stem in docs:
        lines.append('')
        lines.append('----')
        lines.append('')
        # Inline the RST content directly
        rst = doc_to_rst(doc, stem)
        lines.append(rst)

    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Extract MATLAB docstrings → RST files')
    parser.add_argument('--sources', nargs='+', required=True,
                        help='.m source files to process')
    parser.add_argument('--out-dir', required=True,
                        help='Output directory for generated .rst files')
    parser.add_argument('--version', default='0.0.0',
                        help='Project version string')
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    docs = []
    for src in args.sources:
        path = Path(src)
        if not path.exists():
            print(f'[matlab-docs] Warning: not found: {src}', file=sys.stderr)
            continue

        doc = parse_m_file(path)
        stem = path.stem          # e.g. ctoon_encode
        rst = doc_to_rst(doc, stem)
        out_path = out_dir / f'{stem}.rst'
        out_path.write_text(rst, encoding='utf-8')
        print(f'[matlab-docs] wrote {out_path}')
        docs.append((doc, stem))

    # write api.rst
    api_rst = build_api_rst(docs, args.version)
    api_path = out_dir / 'api.rst'
    api_path.write_text(api_rst, encoding='utf-8')
    print(f'[matlab-docs] wrote {api_path}')


if __name__ == '__main__':
    main()
