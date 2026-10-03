"""`ctoon.manager list` -- what files does a release offer? (language independent)

Reads the release's `lists.txt` (one published file name per line). CI
uploads it LAST, after every other artifact, so its presence also means the
release is complete; on `__beta__` all assets (lists.txt included) are
deleted at the start of each run, so a missing file means "not ready yet".
"""

from __future__ import annotations

import sys

from ..core import release


def parse(text: str) -> list[str]:
    names = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if "/" in line or "\\" in line or line in (".", ".."):
            continue  # plain file names only
        names.append(line)
    return names


def run(args) -> int:
    tag = release.resolve_tag(args)
    release.warn_if_beta(args)
    url = release.asset_url(tag, release.LIST_FILE)

    try:
        names = parse(release.fetch_text(url))
    except release.NotAvailable:
        if args.beta:
            print("error: the beta version is not available yet.", file=sys.stderr)
        else:
            print(f"error: release {tag} has no {release.LIST_FILE}.", file=sys.stderr)
        print(f"       ({url} not found)", file=sys.stderr)
        return 1
    except release.ReleaseError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    if not names:
        print(f"error: {release.LIST_FILE} of {tag} is empty.", file=sys.stderr)
        return 1

    for name in names:
        print(release.asset_url(tag, name) if args.urls else name)
    return 0


def add_parser(subparsers) -> None:
    p = subparsers.add_parser("list", help="list the files of a release")
    release.add_channel_args(p)
    p.add_argument("--urls", action="store_true", help="print full download URLs")
    p.set_defaults(func=run)
