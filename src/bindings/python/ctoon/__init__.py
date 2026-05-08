"""CToon - A C++ TOON/JSON serialization library with Python bindings."""

from __future__ import annotations

# Import the native extension module
from .ctoon_py import *  # noqa: F401, F403
from .ctoon_py import __version__  # noqa: F401

__all__ = [
    # TOON serialisation
    "loads",
    "dumps",
    "load",
    "dump",
    # JSON interop
    "loads_json",
    "dumps_json",
    "load_json",
    "dump_json",
    # Aliases
    "encode",
    "decode",
    # Enums
    "ReadFlag",
    "WriteFlag",
    "Delimiter",
]