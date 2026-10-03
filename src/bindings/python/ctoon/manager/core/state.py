"""Tiny JSON state file: what the manager detected / downloaded / installed.

stdlib only. Default location is per environment (sys.prefix), so each venv
keeps its own record; CTOON_STATE overrides it.
"""

from __future__ import annotations

import datetime
import json
import os
import sys
import tempfile

SCHEMA = 1


def path() -> str:
    env = os.environ.get("CTOON_STATE")
    if env:
        return env
    return os.path.join(sys.prefix, "etc", "ctoon", "state.json")


def load() -> dict:
    try:
        with open(path(), "r", encoding="utf-8") as f:
            data = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {"schema": SCHEMA}
    return data if isinstance(data, dict) else {"schema": SCHEMA}


def update(section: str, value: dict) -> str:
    """Set state[section] = value (+ timestamp) and write atomically."""
    state = load()
    state["schema"] = SCHEMA
    state[section] = dict(
        value,
        detected_at=datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
    )
    target = path()
    folder = os.path.dirname(target) or "."
    os.makedirs(folder, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=folder, prefix=".state-", suffix=".tmp")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(state, f, indent=2, sort_keys=True)
            f.write("\n")
        os.replace(tmp, target)
    except BaseException:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise
    return target
