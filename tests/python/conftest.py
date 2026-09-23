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
    CTOON_SPEC_FIXTURES_DIR points at the checkout tests/CMakeLists.txt's
    central FetchContent already made (fetched once there, not per
    language -- see that file's comment on why).
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


@pytest.fixture(scope="session")
def spec_examples_dir() -> Path:
    """Path to toon-format/spec's examples/conversions/ dir: real, paired
    <name>.json / <name>.toon documents (not test manifests -- contrast
    spec_fixtures_dir above, whose files are {"tests":[...]} wrappers).

    Same env-var/skip convention as spec_fixtures_dir; set alongside it
    by the same central FetchContent in tests/CMakeLists.txt.
    """
    env_dir = os.environ.get("CTOON_SPEC_EXAMPLES_DIR")
    if not env_dir or not Path(env_dir).is_dir():
        pytest.skip(
            "CTOON_SPEC_EXAMPLES_DIR not set — only available under "
            "CMake/ctest, same as spec_fixtures_dir."
        )
    return Path(env_dir)
