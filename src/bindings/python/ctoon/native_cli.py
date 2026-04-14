"""Native CLI wrapper for CToon command-line tool."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def _find_cli() -> Path | None:
    """Find the native ctoon CLI binary."""
    # Check if installed package has a bundled CLI
    try:
        import ctoon
        pkg_dir = Path(ctoon.__file__).parent
        cli_path = pkg_dir / "bin" / "ctoon"
        if cli_path.exists():
            return cli_path
    except Exception:
        pass

    # Fallback: check PATH
    import shutil
    path = shutil.which("ctoon")
    if path:
        return Path(path)

    return None


def run() -> None:
    """Run the native CToon CLI."""
    cli = _find_cli()
    if cli is None:
        print("Error: ctoon CLI not found. Install with: pip install ctoon", file=sys.stderr)
        sys.exit(1)

    result = subprocess.run([str(cli), *sys.argv[1:]])
    sys.exit(result.returncode)
