#!/usr/bin/env python3
"""
CToon Version & Tag Manager
Supports:
  - Showing current header version
  - Reading latest git tag
  - Setting version from tag
  - Creating releases (commit + tag)
  - Semantic version bumping
Repository:
    https://github.com/mohammadraziei/ctoon
Author:
    Mohammad Raziei
License:
    MIT
    SPDX-License-Identifier: MIT
Run:
    python version.py --help
"""
import re
import sys
import argparse
import subprocess
from pathlib import Path

HEADER_PATH = "includes/ctoon/ctoon.h"
PREFIX = "CTOON_"
HEADER_ABS_PATH = Path(__file__).parent / HEADER_PATH
VERSION_PATTERN = f"#define {PREFIX}VERSION_(MAJOR|MINOR|PATCH) (\\d+)"

# ============================================================
# Utilities
# ============================================================

def run(cmd):
    subprocess.run(cmd, check=True)

def git_last_tag():
    try:
        result = subprocess.run(
            ["git", "describe", "--tags", "--abbrev=0"],
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        return None

def read_version():
    if not HEADER_ABS_PATH.exists():
        sys.exit(f"{HEADER_PATH} not found")
    text = HEADER_ABS_PATH.read_text()
    matches = re.findall(VERSION_PATTERN, text)
    version = {name: int(value) for name, value in matches}
    return version, text

def write_version(version, text):
    def repl(match):
        name = match.group(1)
        return f"#define {PREFIX}VERSION_{name} {version[name]}"
    new_text = re.sub(VERSION_PATTERN, repl, text)
    HEADER_ABS_PATH.write_text(new_text)

def version_str(v):
    return f"{v['MAJOR']}.{v['MINOR']}.{v['PATCH']}"

def bump(version, part):
    if part == "major":
        version["MAJOR"] += 1
        version["MINOR"] = 0
        version["PATCH"] = 0
    elif part == "minor":
        version["MINOR"] += 1
        version["PATCH"] = 0
    elif part == "patch":
        version["PATCH"] += 1

# ============================================================
# Commands
# ============================================================

def cmd_show(args):
    version, _ = read_version()
    part = getattr(args, 'part', None)
    
    if part:
        print(version[part.upper()])
    else:
        print(version_str(version))

def cmd_bump_or_set(args):
    version, text = read_version()
    part = args.part.upper()  # MAJOR, MINOR, or PATCH
    
    if args.value:
        val_str = args.value
        
        # Handle other offsets (+1, -5, etc)
        if val_str.startswith(('+', '-')):
            try:
                if len(val_str) == 1:
                    val_str += "1"
                offset = int(val_str)
                version[part] += offset
                # Ensure version doesn't go below 0
                if version[part] < 0:
                    version[part] = 0
            except ValueError:
                sys.exit(f"Invalid offset value: {val_str}")
        else:
            # It is a direct set value (e.g., 4)
            try:
                version[part] = int(val_str)
            except ValueError:
                sys.exit(f"Invalid value for {part}: {val_str}")
        
        write_version(version, text)
        print(f"Version updated to {version_str(version)}")
    else:
        # If no value provided, just show the current part
        print(version[part])

def cmd_tag(args):
    # --------------------------------------------------------
    # python version.py tag
    # --------------------------------------------------------
    if args.subcommand is None:
        tag = git_last_tag()
        print(tag if tag else "No git tags found")
        return

    # --------------------------------------------------------
    # python version.py tag set
    # --------------------------------------------------------
    if args.subcommand == "set":
        tag = git_last_tag()
        if not tag:
            sys.exit("No git tags found")
        value = tag.lstrip("v")
        parts = value.split(".")
        if len(parts) != 3 or not all(p.isdigit() for p in parts):
            sys.exit("Invalid tag format")
        version, text = read_version()
        version["MAJOR"], version["MINOR"], version["PATCH"] = map(int, parts)
        write_version(version, text)
        print(f"Version set from tag → {value}")
        return

    # --------------------------------------------------------
    # python version.py tag create [...]
    # --------------------------------------------------------
    if args.subcommand == "create":
        version, text = read_version()
        if args.value is None:
            # use current header version
            pass
        elif args.value in ("major", "minor", "patch"):
            bump(version, args.value)
        else:
            value = args.value.lstrip("v")
            parts = value.split(".")
            if len(parts) != 3 or not all(p.isdigit() for p in parts):
                sys.exit("Invalid version format")
            version["MAJOR"], version["MINOR"], version["PATCH"] = map(int, parts)
        
        write_version(version, text)
        new_version = version_str(version)
        tag_name = f"v{new_version}"
        run(["git", "add", str(HEADER_ABS_PATH)])
        run(["git", "commit", "-m", f"Release {new_version}"])
        run(["git", "tag", tag_name])
        print(f"Committed and tagged {tag_name}")
        return

# ============================================================
# CLI
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        prog="version.py",
        description="dynamic version and git tag manager",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog="""
Examples:
  python version.py
      Show header version
  python version.py minor
      Show minor version
  python version.py major +
      Increase major version by 1
  python version.py patch +2
      Increase patch version by 2
  python version.py minor -5
      Decrease minor version by 5
  python version.py patch 4
      Set patch version to 4
  python version.py tag
      Show latest git tag
  python version.py tag set
      Set header version from latest tag
  python version.py tag create
      Commit and tag using current header version
  python version.py tag create minor
      Bump minor, commit and tag
  python version.py tag create v1.2.3
      Set version, commit and tag
"""
    )
    
    subparsers = parser.add_subparsers(dest="command")
    
    # Default show (if no command provided)
    parser.set_defaults(func=cmd_show)

    # Version parts commands (major, minor, patch)
    for part in ["major", "minor", "patch"]:
        part_parser = subparsers.add_parser(part, help=f"Manage {part} version")
        part_parser.add_argument(
            "value",
            nargs="?",
            help="Value to set (e.g., 4), or offset (e.g., +, -, +1, -5)"
        )
        part_parser.set_defaults(func=cmd_bump_or_set, part=part)

    # tag command
    tag_parser = subparsers.add_parser("tag", help="Manage git tags")
    tag_sub = tag_parser.add_subparsers(dest="subcommand")
    
    tag_parser.set_defaults(func=cmd_tag)
    
    tag_sub.add_parser("set", help="Set version from latest tag")
    
    create_parser = tag_sub.add_parser("create", help="Create release tag")
    create_parser.add_argument(
        "value",
        nargs="?",
        help="major | minor | patch | X.Y.Z"
    )
    
    args = parser.parse_args()
    if hasattr(args, "func"):
        args.func(args)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
