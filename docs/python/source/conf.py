# Configuration file for Sphinx documentation (Python API).

from pathlib import Path
import re
import sys

DOCS_DIR     = Path(__file__).resolve().parent
PROJECT_ROOT = DOCS_DIR.parent.parent.parent
IMAGES_DIR   = PROJECT_ROOT / 'docs' / 'images'

# ── Version ───────────────────────────────────────────────────────────────────
CTOON_H = PROJECT_ROOT / 'include' / 'ctoon.h'
version = '0.0.0'

if CTOON_H.exists():
    text   = CTOON_H.read_text()
    _major = re.search(r'#define\s+CTOON_VERSION_MAJOR\s+(\d+)', text)
    _minor = re.search(r'#define\s+CTOON_VERSION_MINOR\s+(\d+)', text)
    _patch = re.search(r'#define\s+CTOON_VERSION_PATCH\s+(\d+)', text)
    major  = int(_major.group(1)) if _major else 0
    minor  = int(_minor.group(1)) if _minor else 0
    patch  = int(_patch.group(1)) if _patch else 0
    version = f'{major}.{minor}.{patch}'

# ── Import ctoon from build/python/install ────────────────────────────────────
sys.path.insert(0, '.')

import ctoon  # noqa: E402

# ── Project info ──────────────────────────────────────────────────────────────
project   = 'CToon Python'
copyright = '2025, Mohammad Raziei'
author    = 'Mohammad Raziei'
release   = version

# ── Extensions ────────────────────────────────────────────────────────────────
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.napoleon',
    'sphinx.ext.viewcode',
    'sphinx.ext.autosummary',
]

autodoc_member_order   = 'groupwise'
autodoc_typehints      = 'description'
autosummary_generate   = True

# ── Hide internal ctoon_py module — rewrite to ctoon ─────────────────────────
add_module_names       = False
modindex_common_prefix = ['ctoon.']

def _fix_ctoon_py(app, what, name, obj, options, lines):
    """Rewrite ctoon_py → ctoon in docstring lines."""
    for i, line in enumerate(lines):
        lines[i] = line.replace('ctoon_py.', 'ctoon.').replace('ctoon_py::', 'ctoon::')

def _fix_ctoon_py_sig(app, what, name, obj, options, signature, return_annotation):
    """Rewrite ctoon_py → ctoon in signatures."""
    if signature:
        signature = signature.replace('ctoon_py.', 'ctoon.')
    if return_annotation:
        return_annotation = return_annotation.replace('ctoon_py.', 'ctoon.')
    return signature, return_annotation

def setup(app):
    app.connect('autodoc-process-docstring', _fix_ctoon_py)
    app.connect('autodoc-process-signature', _fix_ctoon_py_sig)
    # Patch __module__ on all exported symbols so sphinx shows 'ctoon' not 'ctoon.ctoon_py'
    import ctoon as _ctoon
    import ctoon.ctoon_py as _ctoon_py
    for _name in dir(_ctoon_py):
        _obj = getattr(_ctoon_py, _name, None)
        if _obj is not None and hasattr(_obj, '__module__') and _obj.__module__ == 'ctoon.ctoon_py':
            try:
                _obj.__module__ = 'ctoon'
            except (AttributeError, TypeError):
                pass

napoleon_google_docstring = True
napoleon_numpy_docstring  = True

# ── HTML output ───────────────────────────────────────────────────────────────
language         = 'en'
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']
html_theme       = 'furo'

_logo = IMAGES_DIR / 'ctoon-sq-ctoon.svg'
if _logo.exists():
    html_logo = str(_logo)

html_theme_options = {
    'navigation_with_keys': True,
    'top_of_page_button':   'edit',
    'source_repository':    'https://github.com/MohammadRaziei/ctoon/',
    'source_branch':        'master',
    'source_directory':     'docs/python/source/',
}

html_last_updated_fmt = '%Y-%m-%d %H:%M'