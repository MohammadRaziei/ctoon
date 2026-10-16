"""CToon - A C++ TOON/JSON serialization library with Python bindings."""

from __future__ import annotations

# Import the native extension module
from .ctoon_py import *  # noqa: F401, F403
from .ctoon_py import __version__  # noqa: F401
from .ctoon_py import (  # noqa: F401
    __toon_spec__,
    __toon_spec_tag__,
    __toon_spec_date__,
)

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
    # Direct text -> text conversion (no Python object in between)
    "json_to_toon",
    "toon_to_json",
    "json_to_toon_file",
    "toon_to_json_file",
    # Aliases
    "encode",
    "decode",
    # Enums
    "ReadFlag",
    "WriteFlag",
    "Delimiter",
]