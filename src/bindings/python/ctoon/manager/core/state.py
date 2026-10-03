"""Tiny JSON state file: what the manager detected / downloaded / installed.

Machine-written (contrast config.py, which the user edits). Default location
is per environment (sys.prefix), so each venv keeps its own record;
CTOON_STATE overrides it.
"""

from __future__ import annotations

import datetime
import os
import sys

from . import jsonfile

SCHEMA = 1


def path() -> str:
    env = os.environ.get("CTOON_STATE")
    if env:
        return env
    return os.path.join(sys.prefix, "etc", "ctoon", "state.json")


def load() -> dict:
    try:
        return jsonfile.read(path())
    except ValueError:
        return {}  # unreadable state is just rebuilt on the next detect


def update(section: str, value: dict) -> str:
    """Set state[section] = value (+ timestamp) and write atomically."""
    state = load()
    state["schema"] = SCHEMA
    state[section] = dict(
        value,
        detected_at=datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
    )
    target = path()
    jsonfile.write_atomic(target, state)
    return target
