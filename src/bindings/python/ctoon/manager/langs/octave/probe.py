"""Octave detection -- no CLI, no printing, just facts.

Finds `octave-cli` (PATH first, then the usual install locations), asks it for
its version, and locates `mkoctfile` next to it. `mkoctfile` is only needed to
BUILD the MEX gateway; whether the compiler it drives exists is reported too,
since that is what actually fails on a machine with Octave but no toolchain.
"""

from __future__ import annotations

import glob
import os
import platform
import re
import shutil
import subprocess

# Oldest Octave the binding is verified against (keep in sync with
# CTOON_OCTAVE_MIN_VERSION in cmake/CtoonOctave.cmake).
MIN_VERSION = "8.1"

_VERSION_RE = re.compile(r"^\d+\.\d+(\.\d+)?$")
TIMEOUT = 30


def _version_tuple(version: str) -> tuple[int, ...]:
    return tuple(int(p) for p in version.split("."))


def meets(version: str | None, minimum: str) -> bool | None:
    """True/False against `minimum`; None when `version` isn't a plain x.y[.z]."""
    if not version or not _VERSION_RE.match(version):
        return None
    return _version_tuple(version) >= _version_tuple(minimum)


def _exe_names() -> tuple[str, ...]:
    return ("octave-cli.exe", "octave-cli.bat") if platform.system() == "Windows" else ("octave-cli",)


def _default_dirs() -> list[str]:
    system = platform.system()
    if system == "Windows":
        # C:\Program Files\GNU Octave\Octave-9.2.0\mingw64\bin  (newest first)
        roots = [os.environ.get("ProgramFiles", r"C:\Program Files"),
                 os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")]
        found: list[str] = []
        for root in roots:
            found += glob.glob(os.path.join(root, "GNU Octave", "Octave-*", "mingw64", "bin"))
        return sorted(found, reverse=True)
    if system == "Darwin":
        return ["/opt/homebrew/bin", "/usr/local/bin", "/Applications/Octave.app/Contents/Resources/usr/bin"]
    return ["/usr/local/bin", "/usr/bin", "/snap/bin"]


def find_octave() -> str | None:
    on_path = shutil.which("octave-cli")
    if on_path:
        return os.path.realpath(on_path)
    for directory in _default_dirs():
        for name in _exe_names():
            candidate = os.path.join(directory, name)
            if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
                return os.path.realpath(candidate)
    return None


def find_mkoctfile(bin_dir: str) -> str | None:
    names = ("mkoctfile.exe", "mkoctfile.bat", "mkoctfile") if platform.system() == "Windows" else ("mkoctfile",)
    for name in names:
        path = os.path.join(bin_dir, name)
        if os.path.exists(path):
            return path
    return shutil.which("mkoctfile")


def _run(cmd: list[str]) -> str | None:
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT)
    except (OSError, subprocess.SubprocessError):
        return None
    return proc.stdout.strip() if proc.returncode == 0 else None


def octave_version(exe: str) -> str | None:
    out = _run([exe, "--norc", "--quiet", "--eval", "disp(OCTAVE_VERSION)"])
    return out.splitlines()[-1].strip() if out else None


def compiler_of(mkoctfile: str) -> str | None:
    """The C compiler mkoctfile drives, if it can be found (None otherwise)."""
    cc = _run([mkoctfile, "-p", "CC"])
    if not cc:
        return None
    first = cc.split()[0]  # e.g. "gcc -std=gnu11" -> "gcc"
    return shutil.which(first)


def detect() -> dict:
    """Return everything we can learn about the local Octave."""
    info: dict = {
        "found": False,
        "executable": None,
        "root": None,
        "version": None,
        "mkoctfile": None,
        "compiler": None,
        "min_version": MIN_VERSION,
        "meets_minimum": None,
        "error": None,
    }
    exe = find_octave()
    if exe is None:
        info["error"] = "octave-cli not found on PATH or in the default install locations"
        return info

    info["found"] = True
    info["executable"] = exe
    bin_dir = os.path.dirname(exe)
    info["root"] = os.path.dirname(bin_dir)
    info["mkoctfile"] = find_mkoctfile(bin_dir)
    if info["mkoctfile"]:
        info["compiler"] = compiler_of(info["mkoctfile"])

    info["version"] = octave_version(exe)
    if info["version"] is None:
        info["error"] = f"could not run {exe} to read its version"
    else:
        info["meets_minimum"] = meets(info["version"], MIN_VERSION)
    return info
