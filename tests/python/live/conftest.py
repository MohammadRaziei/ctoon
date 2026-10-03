"""Live tests hit the real GitHub release. Skipped unless selected with -m live."""

from __future__ import annotations

import pytest


def pytest_configure(config):
    config.addinivalue_line(
        "markers", "live: talks to the real GitHub release (run: pytest -m live)"
    )


def pytest_collection_modifyitems(config, items):
    if "live" in (config.getoption("markexpr") or ""):
        return
    skip = pytest.mark.skip(reason="live release test: run with `pytest -m live` after a deploy")
    for item in items:
        if item.get_closest_marker("live"):
            item.add_marker(skip)
