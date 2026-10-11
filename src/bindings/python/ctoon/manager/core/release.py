"""Where release artifacts come from: stable tag vs the `__beta__` pre-release.

stdlib only. Everything is fetched from fixed
`.../releases/download/<tag>/<file>` URLs -- NOT the GitHub REST API, which
rate-limits unauthenticated callers (60/h per IP; CI runners and shared
networks hit that quickly).
"""

from __future__ import annotations

import contextlib
import os
import shutil
import socket
import sys
import tempfile
import urllib.error
import urllib.request

from . import config
from .errors import ManagerError

BETA_TAG = "__beta__"
LIST_FILE = "lists.txt"
TIMEOUT = 20

BETA_WARNING = (
    "warning: using the '__beta__' pre-release. This is a development build "
    "of the latest master, NOT a stable version; it can change or break at any time."
)


class ReleaseError(ManagerError):
    """Something went wrong talking to the release host."""


class NotAvailable(ReleaseError):
    """The file isn't there (HTTP 404)."""


def base_url() -> str:
    # The repo comes from config (`github-repo`), so forks only change config.
    return f"https://github.com/{config.get('github-repo')}/releases"


def add_channel_args(parser) -> None:
    """Add --beta / --tag to a command that fetches release files."""
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--beta",
        action="store_true",
        help=f"use the '{BETA_TAG}' pre-release instead of the stable release (unstable!)",
    )
    group.add_argument("--tag", metavar="TAG", help="use this release tag (e.g. v0.8.3)")


def installed_version() -> str:
    try:
        from importlib.metadata import PackageNotFoundError, version

        try:
            return version("ctoon")
        except PackageNotFoundError:
            pass
    except ImportError:  # pragma: no cover
        pass
    import ctoon  # last resort, loads the native module

    return ctoon.__version__


def resolve_tag(args) -> str:
    if getattr(args, "beta", False):
        return BETA_TAG
    if getattr(args, "tag", None):
        return args.tag
    return "v" + installed_version()


def warn_if_beta(args) -> None:
    if getattr(args, "beta", False):
        print(BETA_WARNING, file=sys.stderr)


def asset_url(tag: str, name: str) -> str:
    return f"{base_url()}/download/{tag}/{name}"


@contextlib.contextmanager
def _network_errors(url: str):
    """Turn urllib/socket failures while talking to `url` into ReleaseError."""
    try:
        yield
    except urllib.error.HTTPError as e:
        e.close()  # an HTTPError owns the response socket
        if e.code == 404:
            raise NotAvailable(url) from None
        raise ReleaseError(f"HTTP {e.code} from {url}") from None
    except urllib.error.URLError as e:
        raise ReleaseError(f"could not reach {url}: {e.reason}") from None
    except (socket.timeout, TimeoutError):
        raise ReleaseError(f"timed out fetching {url}") from None


def _request(url: str) -> urllib.request.Request:
    return urllib.request.Request(url, headers={"User-Agent": "ctoon-manager"})


def fetch_text(url: str) -> str:
    with _network_errors(url):
        with urllib.request.urlopen(_request(url), timeout=TIMEOUT) as resp:
            return resp.read().decode("utf-8")


def fetch_file(url: str, dest: str) -> None:
    """Download `url` to `dest` (written atomically; nothing is left behind on failure)."""
    folder = os.path.dirname(dest) or "."
    os.makedirs(folder, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=folder, prefix=".ctoon-", suffix=".part")
    try:
        with os.fdopen(fd, "wb") as out:
            with _network_errors(url):
                with urllib.request.urlopen(_request(url), timeout=TIMEOUT) as resp:
                    shutil.copyfileobj(resp, out)
        os.replace(tmp, dest)
    except BaseException:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise
