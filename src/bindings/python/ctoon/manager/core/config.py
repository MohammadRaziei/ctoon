"""User configuration (`ctoon.manager config ...`). stdlib only.

A small JSON file holding ONLY what the user changed; everything else comes
from DEFAULTS below. Default location is per environment (sys.prefix), next
to state.json; CTOON_CONFIG overrides it.

Why `github-repo` exists: contributors who fork ctoon point the manager at
their fork's releases (`ctoon.manager config set github-repo me/ctoon`)
instead of editing code.
"""

from __future__ import annotations

import os
import re
import sys

from . import jsonfile
from .errors import ManagerError

SCHEMA = 1

_REPO_RE = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")


def _check_repo(value: str) -> None:
    if not _REPO_RE.match(value):
        raise ManagerError(f"github-repo must look like 'owner/name', got {value!r}")


# key -> (default, validator)
KEYS = {
    "github-repo": ("MohammadRaziei/ctoon", _check_repo),
}


class ConfigError(ManagerError):
    pass


def path() -> str:
    env = os.environ.get("CTOON_CONFIG")
    if env:
        return env
    return os.path.join(sys.prefix, "etc", "ctoon", "config.json")


def _load() -> dict:
    try:
        return jsonfile.read(path())
    except ValueError as e:
        raise ConfigError(f"{e}. Fix or delete it (see: ctoon.manager config path).") from None
    except OSError as e:
        raise ConfigError(f"could not read {path()}: {e}") from None


def _known(key: str) -> None:
    if key not in KEYS:
        raise ConfigError(f"unknown key {key!r}; known keys: {', '.join(sorted(KEYS))}")


def get(key: str) -> str:
    _known(key)
    value = _load().get(key)
    return value if isinstance(value, str) and value else KEYS[key][0]


def effective() -> dict:
    """key -> (value, 'default' | <config file path>)"""
    data = _load()
    out = {}
    for key, (default, _check) in KEYS.items():
        value = data.get(key)
        if isinstance(value, str) and value:
            out[key] = (value, path())
        else:
            out[key] = (default, "default")
    return out


def _write(data: dict) -> None:
    data["schema"] = SCHEMA
    try:
        jsonfile.write_atomic(path(), data)
    except OSError as e:
        raise ConfigError(f"could not write {path()}: {e}") from None


def set(key: str, value: str) -> None:  # noqa: A001 (mirrors dict-style API)
    _known(key)
    KEYS[key][1](value)
    data = _load()
    data[key] = value
    _write(data)


def unset(key: str) -> None:
    _known(key)
    data = _load()
    if key in data:
        del data[key]
        _write(data)
