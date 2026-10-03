"""`ctoon.manager matlab <action>` -- argparse wiring and output."""

from __future__ import annotations

import json
import sys

from ...core import state
from .probe import MIN_RELEASE, detect


def _print_human(info: dict, saved_to: str | None) -> None:
    if not info["found"]:
        print("matlab : not found")
        print(f"         {info['error']}")
        print("         (put matlab on PATH, or install under the default location)")
        return
    print(f"matlab : {info['executable']}")
    print(f"root   : {info['root']}")
    note = ""
    if info["meets_minimum"] is True:
        note = f"  (>= {MIN_RELEASE}, ok)"
    elif info["meets_minimum"] is False:
        note = f"  (< {MIN_RELEASE}, too old)"
    print(f"release: {info['release'] or 'unknown'}{note}")
    print(f"mex    : {info['mex'] or 'not found next to the matlab binary'}")
    if info["error"]:
        print(f"warning: {info['error']}")
    if saved_to:
        print(f"saved  : {saved_to}")


def run_detect(args) -> int:
    info = detect()

    saved_to = None
    if not args.no_save:
        try:
            saved_to = state.update("matlab", info)
        except OSError as e:
            print(f"warning: could not save state: {e}", file=sys.stderr)

    if args.json:
        print(json.dumps(info, indent=2, sort_keys=True))
    else:
        _print_human(info, saved_to)

    usable = info["found"] and info["mex"] is not None and info["release"] is not None
    return 0 if usable and info["meets_minimum"] is not False else 1


def add_parser(subparsers) -> None:
    p = subparsers.add_parser("matlab", help="MATLAB toolchain")
    sub = p.add_subparsers(dest="action", metavar="<action>", required=True)

    d = sub.add_parser("detect", help="find matlab, its release and mex")
    d.add_argument("--json", action="store_true", help="machine-readable output")
    d.add_argument("--no-save", action="store_true", help="don't record the result in the state file")
    d.set_defaults(func=run_detect)
