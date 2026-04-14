"""CToon command-line interface."""

from __future__ import annotations

from . import native_cli

def main() -> None:
    """Entry point for ctoon script."""
    native_cli.run()

if __name__ == "__main__":
    main()
