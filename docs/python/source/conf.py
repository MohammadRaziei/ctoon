# Configuration file for Sphinx documentation (Python API).

from pathlib import Path

DOCS_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = DOCS_DIR.parent.parent.parent
IMAGES_DIR = PROJECT_ROOT / 'docs' / 'images'

# Read version from pyproject.toml
import tomllib
with open(PROJECT_ROOT / 'pyproject.toml', 'rb') as f:
    pyproject_data = tomllib.load(f)
version = pyproject_data['project'].get('version', '0.1.0')

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
html_logo = str(IMAGES_DIR / 'ctoon.svg')
# html_favicon = str(IMAGES_DIR / 'ctoon.svg')

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
