"""Shared pytest fixtures for CToon's Python tests."""

from __future__ import annotations

import os
from pathlib import Path

import pytest


@pytest.fixture(scope="session")
def spec_fixtures_dir() -> Path:
    """Path to a local checkout of toon-format/spec's tests/fixtures dir.

    toon-format/spec isn't part of this project, so it's never vendored
    or cloned by the Python tests themselves. When run under CMake/ctest,
    CTOON_SPEC_FIXTURES_DIR points at the checkout the C++ conformance
    suite's FetchContent already made (see tests/cpp/CMakeLists.txt).
    If that env var isn't set — e.g. running `pytest` directly, outside
    CMake — the spec-conformance tests are skipped rather than fetching
    their own copy.
    """
    env_dir = os.environ.get("CTOON_SPEC_FIXTURES_DIR")
    if not env_dir or not Path(env_dir).is_dir():
        pytest.skip(
            "CTOON_SPEC_FIXTURES_DIR not set — spec-conformance tests only "
            "run under CMake/ctest, which fetches toon-format/spec fixtures "
            "for this suite to use."
        )
    return Path(env_dir)
