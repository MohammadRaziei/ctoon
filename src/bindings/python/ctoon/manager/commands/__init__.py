"""Language-independent commands (`ctoon.manager <command>`).

A command module exposes `add_parser(subparsers)`; add it to COMMANDS.
"""

from __future__ import annotations

from . import configuration, listing

COMMANDS = (configuration, listing)
