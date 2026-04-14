"""CToon Manager - CLI entry point."""

from __future__ import annotations

import argparse
import sys

import ctoon


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="ctoon.manager",
        description="CToon serialization manager",
    )
    parser.add_argument(
        "-v", "--version",
        action="version",
        version=f"%(prog)s {ctoon.__version__}",
    )
    parser.parse_args()

