#!/usr/bin/env python3
"""
extract_matlab_docs.py — Extract MATLAB docstrings and generate RST files.

Parses standard MATLAB help-text format (H1 line + body comment block)
from .m source files and writes one RST file per input file plus an
api.rst index that references them all.

Usage:
    python3 extract_matlab_docs.py
        --pkg-sources     <file1.m> [<file2.m> ...]   # ctoon/ package functions
        --helper-sources  <file1.m> [<file2.m> ...]   # ctoon_*.m top-level scripts
        --out-dir         <output_directory>
        --version         <version_string>

Output (in --out-dir):
    api.rst               top-level API reference with two sections:
                            - Package functions  (ctoon.encode, ctoon.decode, ...)
                            - Helper scripts     (ctoon_build, ctoon_install, ...)
    encode.rst
    decode.rst
    ctoon_build.rst
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
        name        str   function name  (from H1 line, e.g. ENCODE)
        h1          str   one-line summary
        syntax      list  lines of the syntax block
        sections    list  of (heading: str, lines: list[str]) tuples
        see_also    list  of function name strings
        raw_body    str   full docstring body (for fallback)
    """
    text = path.read_text(encoding='utf-8')
    lines = text.splitlines()

    comment_lines = []
    in_block = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('function '):
            in_block = True
            continue
        if in_block:
            if stripped.startswith('%'):
                comment_lines.append(stripped[1:])
            else:
                break

    if not comment_lines:
        return {'name': path.stem, 'h1': '', 'syntax': [],
                'sections': [], 'see_also': [], 'raw_body': ''}

    h1_raw = comment_lines[0].strip()
    h1_match = re.match(r'^([A-Z0-9_]+)\s{2,}(.+)$', h1_raw)
    if h1_match:
        func_name = h1_match.group(1)
        h1_summary = h1_match.group(2).strip()
    else:
        func_name = path.stem.upper()
        h1_summary = h1_raw

    body_lines = [ln.rstrip() for ln in comment_lines[1:]]
    while body_lines and body_lines[0].strip() == '':
        body_lines.pop(0)

    section_re  = re.compile(r'^\s{0,4}([A-Z][A-Za-z0-9 _-]+):\s*$')
    see_also_re = re.compile(r'^\s*See also:\s*(.+)$', re.IGNORECASE)

    sections       = []
    syntax_lines   = []
    see_also       = []
    current_heading = None
    current_lines   = []
    pre_section     = True

    for ln in body_lines:
        sa_match = see_also_re.match(ln)
        if sa_match:
            if current_heading is not None:
                sections.append((current_heading, current_lines))
            raw = sa_match.group(1)
            see_also = [n.strip().rstrip('.') for n in re.split(r'[,\s]+', raw) if n.strip()]
            current_heading = None
            current_lines   = []
            pre_section     = False
            continue

        sec_match = section_re.match(ln)
        if sec_match:
            if pre_section:
                syntax_lines = current_lines[:]
                pre_section  = False
            elif current_heading is not None:
                sections.append((current_heading, current_lines))
            current_heading = sec_match.group(1)
            current_lines   = []
            continue

        current_lines.append(ln)

    if current_heading is not None and current_lines:
        sections.append((current_heading, current_lines))
    elif pre_section:
        syntax_lines = current_lines[:]

    syntax_lines = [ln[3:] if ln.startswith('   ') else ln for ln in syntax_lines]
    while syntax_lines and syntax_lines[0].strip() == '':
        syntax_lines.pop(0)
    while syntax_lines and syntax_lines[-1].strip() == '':
        syntax_lines.pop()

    return {
        'name':     func_name,
        'h1':       h1_summary,
        'syntax':   syntax_lines,
        'sections': sections,
        'see_also': see_also,
        'raw_body': '\n'.join(body_lines),
    }


# ---------------------------------------------------------------------------
# RST renderer
# ---------------------------------------------------------------------------

def _section_body_to_rst(heading: str, lines: list) -> list:
    out = []
    in_code = False
    for ln in lines:
        content = ln[3:] if ln.startswith('   ') else ln
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


def doc_to_rst(doc: dict, stem: str, prefix: str = '') -> str:
    """Render a parsed docstring dict to RST.

    prefix: 'ctoon.' for package functions, '' for top-level scripts.
    """
    lines = []

    label = f'``{prefix}{stem}``'
    lines.append(label)
    lines.append('-' * len(label))
    lines.append('')
    lines.append(doc['h1'])
    lines.append('')

    if doc['syntax']:
        lines.append('.. code-block:: matlab')
        lines.append('')
        for ln in doc['syntax']:
            lines.append('   ' + ln if ln.strip() else '')
        lines.append('')

    for heading, body_lines in doc['sections']:
        lines.append(heading)
        lines.append('~' * len(heading))
        lines.append('')
        lines.extend(_section_body_to_rst(heading, body_lines))
        lines.append('')

    if doc['see_also']:
        lines.append('See also')
        lines.append('~~~~~~~~')
        lines.append('')
        for name in doc['see_also']:
            lines.append(f':func:`{name.lower()}`')
        lines.append('')

    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# api.rst builder — two sections
# ---------------------------------------------------------------------------

def build_api_rst(pkg_docs: list, helper_docs: list, version: str) -> str:
    """Build api.rst with two sections: package functions and helper scripts."""
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

    # ---- Package functions (ctoon.encode, ctoon.decode, ...) ---------------
    if pkg_docs:
        lines.append('Package functions')
        lines.append('-----------------')
        lines.append('')
        lines.append('Call these via the ``ctoon`` package namespace, e.g. ``ctoon.encode(v)``.')
        lines.append('')
        for doc, stem in pkg_docs:
            lines.append('')
            lines.append('----')
            lines.append('')
            lines.append(doc_to_rst(doc, stem, prefix='ctoon.'))

    # ---- Helper scripts (ctoon_build, ctoon_install, ctoon_clean) ----------
    if helper_docs:
        lines.append('')
        lines.append('Helper scripts')
        lines.append('--------------')
        lines.append('')
        lines.append('Run these directly from the ``src/bindings/matlab`` directory.')
        lines.append('')
        for doc, stem in helper_docs:
            lines.append('')
            lines.append('----')
            lines.append('')
            lines.append(doc_to_rst(doc, stem, prefix=''))

    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Extract MATLAB docstrings → RST files')
    parser.add_argument('--pkg-sources',    nargs='*', default=[],
                        help='.m files inside ctoon/ package folder')
    parser.add_argument('--helper-sources', nargs='*', default=[],
                        help='top-level ctoon_*.m helper scripts')
    parser.add_argument('--out-dir',  required=True,
                        help='Output directory for generated .rst files')
    parser.add_argument('--version',  default='0.0.0',
                        help='Project version string')
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    def process_group(sources):
        result = []
        for src in sources:
            path = Path(src)
            if not path.exists():
                print(f'[matlab-docs] Warning: not found: {src}', file=sys.stderr)
                continue
            doc  = parse_m_file(path)
            stem = path.stem
            rst  = doc_to_rst(doc, stem)
            out_path = out_dir / f'{stem}.rst'
            out_path.write_text(rst, encoding='utf-8')
            print(f'[matlab-docs] wrote {out_path}')
            result.append((doc, stem))
        return result

    pkg_docs    = process_group(args.pkg_sources)
    helper_docs = process_group(args.helper_sources)

    api_rst  = build_api_rst(pkg_docs, helper_docs, args.version)
    api_path = out_dir / 'api.rst'
    api_path.write_text(api_rst, encoding='utf-8')
    print(f'[matlab-docs] wrote {api_path}')


if __name__ == '__main__':
    main()