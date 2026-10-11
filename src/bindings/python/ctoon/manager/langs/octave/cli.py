"""`ctoon.manager octave <action>` -- argparse wiring and output.

Actions:
  detect    find octave-cli, its version, mkoctfile and the compiler behind it
  install   build the binding from the release's MATLAB source distribution
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

from ...commands import listing
from ...core import release, state
from ...core.errors import ManagerError
from .probe import MIN_VERSION, detect

# The MATLAB source dist is the one artifact Octave builds from; there is no
# Octave-specific binary (a MEX file is tied to the Octave it was built with).
_SRC_ZIP_RE = re.compile(r"^ctoon-matlab-.+-src\.zip$")

BUILD_TIMEOUT = 600
VERIFY_TIMEOUT = 120


# ---------------------------------------------------------------------------
# detect
# ---------------------------------------------------------------------------

def _print_human(info: dict, saved_to: str | None) -> None:
    if not info["found"]:
        print("octave : not found")
        print(f"         {info['error']}")
        print("         (install GNU Octave and put octave-cli on PATH)")
        return
    print(f"octave : {info['executable']}")
    print(f"root   : {info['root']}")
    note = ""
    if info["meets_minimum"] is True:
        note = f"  (>= {MIN_VERSION}, ok)"
    elif info["meets_minimum"] is False:
        note = f"  (< {MIN_VERSION}, too old)"
    print(f"version: {info['version'] or 'unknown'}{note}")
    print(f"mkoctfile: {info['mkoctfile'] or 'not found (needed to BUILD only; Debian/Ubuntu: apt install octave-dev)'}")
    if info["mkoctfile"]:
        print(f"compiler : {info['compiler'] or 'not found (needed to BUILD only)'}")
    if info["error"]:
        print(f"warning: {info['error']}")
    if saved_to:
        print(f"saved  : {saved_to}")


def run_detect(args) -> int:
    info = detect()

    saved_to = None
    if not args.no_save:
        try:
            saved_to = state.update("octave", info)
        except OSError as e:
            print(f"warning: could not save state: {e}", file=sys.stderr)

    if args.json:
        print(json.dumps(info, indent=2, sort_keys=True))
    else:
        _print_human(info, saved_to)

    usable = info["found"] and info["version"] is not None
    return 0 if usable and info["meets_minimum"] is not False else 1


# ---------------------------------------------------------------------------
# install
# ---------------------------------------------------------------------------

def default_prefix() -> str:
    # Per environment, like the manager's config/state files.
    return os.path.join(sys.prefix, "share", "ctoon", "octave")


def _q(path: str) -> str:
    """Quote `path` for an Octave single-quoted string."""
    return "'" + path.replace("'", "''") + "'"


def _extract(zip_path: str, dest: str) -> str:
    """Unpack the source dist into `dest`; return the folder holding ctoon_build.m."""
    root = os.path.realpath(dest)
    try:
        with zipfile.ZipFile(zip_path) as z:
            for member in z.namelist():
                target = os.path.realpath(os.path.join(root, member))
                if target != root and not target.startswith(root + os.sep):
                    raise ManagerError(f"unsafe path in {os.path.basename(zip_path)}: {member}")
            z.extractall(root)
    except zipfile.BadZipFile:
        raise ManagerError(f"{zip_path} is not a valid zip file") from None

    for folder, _dirs, files in os.walk(root):
        if "ctoon_build.m" in files and "ctoon_mex.c" in files:
            return folder
    raise ManagerError(f"{os.path.basename(zip_path)} is not a CToon MATLAB source distribution "
                       "(no ctoon_build.m / ctoon_mex.c inside)")


def _find_source_name(tag: str) -> str:
    """The `ctoon-matlab-<ver>-src.zip` a release publishes (looked up in lists.txt)."""
    url = release.asset_url(tag, release.LIST_FILE)
    try:
        names = listing.parse(release.fetch_text(url))
    except release.NotAvailable:
        raise ManagerError(f"release {tag} has no {release.LIST_FILE} ({url} not found)") from None
    matches = [n for n in names if _SRC_ZIP_RE.match(n)]
    if not matches:
        raise ManagerError(f"release {tag} does not offer a MATLAB source distribution "
                           "(ctoon-matlab-*-src.zip), which Octave builds from")
    return sorted(matches)[-1]


def _octave(exe: str, expr: str, timeout: int) -> subprocess.CompletedProcess:
    return subprocess.run(
        [exe, "--no-gui", "--norc", "--quiet", "--eval", expr],
        capture_output=True, text=True, timeout=timeout,
    )


def _check_toolchain(info: dict) -> None:
    if not info["found"]:
        raise ManagerError(f"{info['error']}. Install GNU Octave {MIN_VERSION}+ first.")
    if info["meets_minimum"] is False:
        raise ManagerError(f"Octave {info['version']} is too old; {MIN_VERSION}+ is required.")
    if not info["mkoctfile"]:
        raise ManagerError("mkoctfile not found: building needs Octave's development files "
                           "(Debian/Ubuntu: apt install octave-dev). Only the build needs it.")
    if not info["compiler"]:
        raise ManagerError("no C compiler found for mkoctfile (Debian/Ubuntu: apt install build-essential; "
                           "macOS: xcode-select --install). Only the build needs one.")


def run_install(args) -> int:
    info = detect()
    _check_toolchain(info)

    dest = os.path.abspath(args.prefix or default_prefix())

    with tempfile.TemporaryDirectory(prefix="ctoon-octave-") as tmp:
        if args.zip:
            zip_path = os.path.abspath(args.zip)
            if not os.path.isfile(zip_path):
                raise ManagerError(f"no such file: {args.zip}")
            origin = zip_path
        else:
            tag = release.resolve_tag(args)
            release.warn_if_beta(args)
            name = _find_source_name(tag)
            url = release.asset_url(tag, name)
            zip_path = os.path.join(tmp, name)
            print(f"downloading {url}")
            try:
                release.fetch_file(url, zip_path)
            except release.NotAvailable:
                raise ManagerError(f"{url} not found") from None
            origin = url

        src = _extract(zip_path, os.path.join(tmp, "src"))

        print(f"building with Octave {info['version']} (mkoctfile) -> {dest}")
        if os.path.isdir(dest):
            # A stale gateway from another Octave must not survive a reinstall.
            shutil.rmtree(dest)
        expr = f"addpath({_q(src)}); ctoon_build({_q(dest)}, true);"
        try:
            proc = _octave(info["executable"], expr, BUILD_TIMEOUT)
        except subprocess.TimeoutExpired:
            raise ManagerError(f"build did not finish within {BUILD_TIMEOUT}s") from None
        if proc.returncode != 0:
            sys.stderr.write(proc.stdout)
            sys.stderr.write(proc.stderr)
            raise ManagerError("building the MEX gateway failed (output above)")

    # Verify the installed copy on its own: the source tree is already gone.
    check = (f"addpath({_q(dest)}); warning('off', 'Octave:shadowed-function'); "
             "v = ctoon.decode(ctoon.encode(struct('x', 1.5))); "
             "assert(isstruct(v) && abs(v.x - 1.5) < 1e-10); disp(ctoon.version());")
    try:
        proc = _octave(info["executable"], check, VERIFY_TIMEOUT)
    except subprocess.TimeoutExpired:
        raise ManagerError("the installed binding did not answer (verification timed out)") from None
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout)
        sys.stderr.write(proc.stderr)
        raise ManagerError("the installed binding failed its self-check (output above)")

    try:
        state.update("octave", dict(info, install_dir=dest, installed_from=origin))
    except OSError as e:
        print(f"warning: could not save state: {e}", file=sys.stderr)

    print(f"installed: {dest}")
    print("use it in Octave with:")
    print(f"    addpath({_q(dest)})")
    return 0


# ---------------------------------------------------------------------------
# parser
# ---------------------------------------------------------------------------

def add_parser(subparsers) -> None:
    p = subparsers.add_parser("octave", help="GNU Octave toolchain and binding")
    sub = p.add_subparsers(dest="action", metavar="<action>", required=True)

    d = sub.add_parser("detect", help="find octave-cli, its version and mkoctfile")
    d.add_argument("--json", action="store_true", help="machine-readable output")
    d.add_argument("--no-save", action="store_true", help="don't record the result in the state file")
    d.set_defaults(func=run_detect)

    i = sub.add_parser(
        "install",
        help="build the binding from the release's MATLAB source distribution",
        description="Download the MATLAB source distribution of a release and build it with "
                    "mkoctfile. There is no prebuilt Octave binary; the build needs a C compiler, "
                    "using the result does not.",
    )
    release.add_channel_args(i)
    i.add_argument("--prefix", metavar="DIR",
                   help=f"where to install (default: {default_prefix()})")
    i.add_argument("--zip", metavar="FILE",
                   help="use this local ctoon-matlab-*-src.zip instead of downloading one")
    i.set_defaults(func=run_install)
