"""MATLAB detection -- no CLI, no printing, just facts.

Detection itself is the gist's tested code (_matlab_version.py), used as is:
find the MATLAB binary, then read its release from VersionInfo.xml without
launching MATLAB. This module only adds what the gist doesn't return: the
install root and the `mex` compiler driver, which lives next to the matlab
binary.
"""

from __future__ import annotations

import os
import platform
import re

from . import _matlab_version as _gist

# Oldest release the MATLAB binding supports (CMake: MATLAB >= 8.4 == R2014b).
# Not what CI covers (setup-matlab starts at R2021a) -- see matlab.yml.
MIN_RELEASE = "R2014b"

_RELEASE_RE = re.compile(r"^R\d{4}[ab]$")


def find_mex(bin_dir: str) -> str | None:
    names = ("mex.bat", "mex.exe", "mex") if platform.system() == "Windows" else ("mex",)
    for name in names:
        path = os.path.join(bin_dir, name)
        if os.path.exists(path):
            return path
    return None


def meets(release: str | None, minimum: str) -> bool | None:
    # "R2021a" < "R2021b" < "R2022a" ...: plain string order is release order.
    if not release or not _RELEASE_RE.match(release):
        return None
    return release >= minimum


def detect() -> dict:
    """Return everything we can learn about the local MATLAB, without running it."""
    info: dict = {
        "found": False,
        "executable": None,
        "root": None,
        "release": None,
        "mex": None,
        "min_release": MIN_RELEASE,
        "meets_minimum": None,
        "error": None,
    }
    try:
        exe = _gist.find_matlab_executable()
    except FileNotFoundError as e:
        info["error"] = str(e)
        return info

    info["found"] = True
    info["executable"] = exe
    info["root"] = os.path.dirname(os.path.dirname(exe))
    info["mex"] = find_mex(os.path.dirname(exe))
    try:
        info["release"] = _gist.get_matlab_release()
        info["meets_minimum"] = meets(info["release"], MIN_RELEASE)
    except (FileNotFoundError, RuntimeError) as e:
        info["error"] = str(e)
    return info
