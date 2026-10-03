"""Read / atomically write a small JSON object file. stdlib only."""

from __future__ import annotations

import json
import os
import tempfile


def read(path: str) -> dict:
    """Return the JSON object in `path`; {} if the file doesn't exist.

    Raises ValueError if the file exists but isn't a JSON object.
    """
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except (FileNotFoundError, NotADirectoryError):
        return {}
    except json.JSONDecodeError as e:
        raise ValueError(f"{path} is not valid JSON ({e})") from None
    if not isinstance(data, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return data


def write_atomic(path: str, data: dict) -> None:
    folder = os.path.dirname(path) or "."
    os.makedirs(folder, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=folder, prefix=".ctoon-", suffix=".tmp")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, sort_keys=True)
            f.write("\n")
        os.replace(tmp, path)
    except BaseException:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise
