"""Top-level argument parser for `ctoon.manager`."""

from __future__ import annotations

import argparse

from .commands import COMMANDS
from .langs import LANGS


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ctoon.manager",
        description="CToon serialization manager",
    )
    parser.add_argument("-v", "--version", action="store_true", help="show version and exit")
    subparsers = parser.add_subparsers(dest="command", metavar="<command|lang>")
    for mod in (*COMMANDS, *LANGS):
        mod.add_parser(subparsers)
    return parser


def main(argv=None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.version:
        import ctoon  # lazy: only --version needs the native module

        print(f"ctoon.manager {ctoon.__version__}")
        return 0
    if not getattr(args, "func", None):
        parser.print_help()
        return 0
    return args.func(args)
