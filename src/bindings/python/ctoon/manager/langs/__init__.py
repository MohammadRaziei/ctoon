"""One subpackage per language.

A language package exposes `NAME` and `add_parser(subparsers)`; add it to
LANGS below and it shows up as `ctoon.manager <NAME> ...`.
"""

from __future__ import annotations

from . import matlab

LANGS = (matlab,)
