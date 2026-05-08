# Configuration file for Sphinx documentation (Python API).

from pathlib import Path
import re
import sys
import os

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
_install_dir = os.environ.get('CTOON_PYTHON_INSTALL_DIR', '')
if _install_dir:
    sys.path.insert(0, _install_dir)

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