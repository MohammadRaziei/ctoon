"""GNU Octave toolchain support for ctoon.manager.

Octave has no prebuilt ctoon binary: `ctoon.manager octave install` downloads
the MATLAB *source* distribution (ctoon-matlab-<ver>-src.zip) of a release and
builds it locally with `mkoctfile`. Only the build needs a compiler.
"""

from __future__ import annotations

from .cli import add_parser
from .probe import MIN_VERSION, detect

NAME = "octave"

__all__ = ["NAME", "MIN_VERSION", "add_parser", "detect"]
