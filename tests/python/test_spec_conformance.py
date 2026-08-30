"""Spec conformance tests for the Python bindings.

Runs ctoon against the official fixture suite published in
https://github.com/toon-format/spec (tests/fixtures/{encode,decode}/*.json),
fetched on demand by the `spec_fixtures_dir` fixture (see conftest.py) —
not vendored into this repo.

Each fixture file is itself JSON, shaped like:
    {"tests": [{"name", "input", "expected", "options"?, "shouldError"?}]}

- encode/*.json: take "input" (a Python value), encode it to TOON with the
  requested options, compare the text to "expected".
- decode/*.json: parse "input" (TOON text) and compare the resulting value
  to "expected", or expect a decode failure when "shouldError" is set.

This mirrors tests/cpp/test_spec_conformance.cpp; keep the two in sync.
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

import ctoon
from ctoon import Delimiter, ReadFlag

ENCODE_FIXTURES = [
    "primitives.json", "objects.json", "objects-keyed.json",
    "arrays-primitive.json", "arrays-nested.json", "arrays-tabular.json",
    "arrays-objects.json", "delimiters.json", "whitespace.json",
]

DECODE_FIXTURES = [
    "primitives.json", "objects.json", "objects-keyed.json",
    "arrays-primitive.json", "arrays-nested.json", "arrays-tabular.json",
    "numbers.json", "delimiters.json", "whitespace.json", "blank-lines.json",
    "comments.json", "root-form.json", "validation-errors.json",
    "indentation-errors.json",
]


def _load_fixture(spec_fixtures_dir: Path, subdir: str, filename: str) -> dict:
    path = spec_fixtures_dir / subdir / filename
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _delimiter(options: dict | None) -> Delimiter:
    d = (options or {}).get("delimiter")
    if d == "\t":
        return Delimiter.TAB
    if d == "|":
        return Delimiter.PIPE
    return Delimiter.COMMA


def _indent(options: dict | None, fallback: int = 2) -> int:
    return (options or {}).get("indentSize", fallback)


def _strict(options: dict | None, fallback: bool = True) -> bool:
    return (options or {}).get("strict", fallback)


@pytest.mark.parametrize("filename", ENCODE_FIXTURES)
def test_encode_fixture(spec_fixtures_dir, filename):
    fixture = _load_fixture(spec_fixtures_dir, "encode", filename)
    failures = []
    for case in fixture["tests"]:
        options = case.get("options")
        try:
            got = ctoon.dumps(
                case["input"],
                indent=_indent(options),
                delimiter=_delimiter(options),
            )
        except Exception as e:  # noqa: BLE001 - report, don't crash the loop
            failures.append(f"{case['name']!r}: unexpected exception: {e!r}")
            continue
        want = case["expected"]
        # dumps() doesn't add a trailing newline; fixture text shouldn't
        # need one either, but strip defensively for a clean compare.
        if got.rstrip("\n") != want.rstrip("\n"):
            failures.append(
                f"{case['name']!r}: expected {want!r}, got {got!r}"
            )
    assert not failures, "\n" + "\n".join(failures)


@pytest.mark.parametrize("filename", DECODE_FIXTURES)
def test_decode_fixture(spec_fixtures_dir, filename):
    fixture = _load_fixture(spec_fixtures_dir, "decode", filename)
    failures = []
    for case in fixture["tests"]:
        options = case.get("options")
        expect_error = bool(case.get("shouldError"))
        strict = _strict(options)
        flags = ReadFlag.NOFLAG if strict else ReadFlag.ALLOW_INF_AND_NAN
        indent = _indent(options)

        try:
            got = ctoon.loads(case["input"], flags=flags, indent=indent)
            threw = False
        except Exception:
            threw = True
            got = None

        if expect_error:
            if not threw:
                failures.append(
                    f"{case['name']!r}: expected a decode error, got {got!r}"
                )
            continue

        if threw:
            failures.append(f"{case['name']!r}: unexpected decode failure")
            continue

        want = case["expected"]
        if got != want:
            failures.append(
                f"{case['name']!r}: expected {want!r}, got {got!r}"
            )
    assert not failures, "\n" + "\n".join(failures)
