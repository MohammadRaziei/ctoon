# Configuration file for Sphinx documentation (Python API).

from pathlib import Path
import re

DOCS_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = DOCS_DIR.parent.parent.parent
IMAGES_DIR = PROJECT_ROOT / 'docs' / 'images'

# Read version from ctoon.h
CTOON_H = PROJECT_ROOT / 'includes' / 'ctoon.h'
text = CTOON_H.read_text()

_major = re.search(r'#define\s+CTOON_VERSION_MAJOR\s+(\d+)', text)
_minor = re.search(r'#define\s+CTOON_VERSION_MINOR\s+(\d+)', text)
_patch = re.search(r'#define\s+CTOON_VERSION_PATCH\s+(\d+)', text)

major = int(_major.group(1)) if _major else 0
minor = int(_minor.group(1)) if _minor else 0
patch = int(_patch.group(1)) if _patch else 0
version = f'{major}.{minor}.{patch}'

project = 'CToon Python'
copyright = '2024, Mohammad Raziei'
author = 'Mohammad Raziei'
release = version

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.napoleon',
    'sphinx.ext.viewcode',
]

language = 'en'
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

html_theme = 'furo'
html_logo =  str(IMAGES_DIR / 'ctoon-sq-ctoon.svg')

html_theme_options = {
    "navigation_with_keys": True,
    "top_of_page_button": "edit",
    "source_repository": "https://github.com/MohammadRaziei/ctoon/",
    "source_branch": "master",
    "source_directory": "docs/python/source/",
}

html_last_updated_fmt = '%Y-%m-%d %H:%M'

napoleon_google_docstring = True
napoleon_numpy_docstring = True
autodoc_member_order = 'groupwise'
autodoc_typehints = 'description'
