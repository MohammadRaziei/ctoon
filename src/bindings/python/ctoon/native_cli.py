"""Native CLI wrapper for CToon command-line tool."""

from __future__ import annotations

import os
import shutil
import stat
import subprocess
import sys
from pathlib import Path
from typing import Optional, List

import ctoon


def ctoon_bin_path() -> Path | None:
    """Find the native ctoon CLI binary (OS-independent)."""
    # 1. Check bin/ next to the ctoon package (e.g., site-packages/bin/ctoon)
    pkg_dir = Path(ctoon.__file__).parent       # .../site-packages/ctoon/

    if pkg_dir and pkg_dir.is_dir():
        exe_name = "ctoon.exe" if sys.platform == "win32" else "ctoon"
        cli_path = pkg_dir / "bin" / exe_name
        if cli_path.exists():
            return _ensure_executable(cli_path)

    return None


def _ensure_executable(path: Path) -> None:
    """On Unix-like systems, ensure the file has the executable bit."""
    if sys.platform == "win32":
        return path  # Windows doesn't use executable bits, so we skip this step
    try:
        mode = path.stat().st_mode
        if not (mode & stat.S_IXUSR):
            path.chmod(mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    except OSError:
        pass  # Best-effort - caller will fail with a clear error if unusable
    return path

def run(args: Optional[List[str]] = None) -> None:
    """Run the native CToon CLI."""
    if args is None:
        args = sys.argv[1:]
        
    cli_path = ctoon_bin_path()
    if cli_path is None:
        print("Error: ctoon CLI not found. Install with: pip install ctoon", file=sys.stderr)
        sys.exit(1)

    result = subprocess.run([str(cli_path), *args], check=False)
    sys.exit(result.returncode)
