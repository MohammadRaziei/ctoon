"""CToon - A C++ TOON/JSON serialization library with Python bindings."""

from __future__ import annotations

# Import the native extension module
from .ctoon_py import *  # noqa: F401, F403
from .ctoon_py import __version__  # noqa: F401

__all__ = [
    "loads_json",
    "dumps_json",
    "load_json",
    "dump_json",
    "encode",
    "decode",
    "encode_to_file",
    "decode_from_file",
    "loads_toon",
    "dumps_toon",
    "loads",
    "dumps",
    "Delimiter",
    "EncodeOptions",
    "DecodeOptions",
    "Type",
]
