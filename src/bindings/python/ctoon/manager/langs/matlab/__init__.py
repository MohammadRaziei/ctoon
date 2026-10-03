"""MATLAB toolchain support for ctoon.manager."""

from __future__ import annotations

from .cli import add_parser
from .probe import MIN_RELEASE, detect

NAME = "matlab"

__all__ = ["NAME", "MIN_RELEASE", "add_parser", "detect"]
