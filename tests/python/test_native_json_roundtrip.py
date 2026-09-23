"""ctoon's own JSON reader/writer vs Python's stdlib ``json`` module.

Every other Python test in this suite either builds Python values by hand
(dicts/lists/scalars) or goes through ctoon.loads_json() -- ctoon's *own*
JSON parser (src/ctoon.c's cj_* reader). None of them ever run a real JSON
document through Python's *own*, independent JSON parser (the stdlib
``json`` module -- builtin, nothing to install) and then hand that native
result to ctoon's TOON writer. That's a real, separate code path
(py_to_mutval() in binding.cpp), and it's exactly the path that had the
struct-array / null-as-NaN / logical-array bugs in the MATLAB binding
(different type-conversion layer, same class of gap) -- see
tests/matlab/test_ctoon.m's "MATLAB-native jsondecode() round trips"
section for the sibling tests over there.

Two corpora, both real (not hand-crafted) documents:

  - tests/data/*.json + *.toon -- local to this repo, always available,
    no network/env needed.
  - toon-format/spec's examples/conversions/*.json + *.toon -- fetched
    centrally by tests/CMakeLists.txt; skipped (spec_examples_dir fixture)
    when CTOON_SPEC_EXAMPLES_DIR isn't set.

For each paired file, three independent things are checked:
  1. Python's json.load() and ctoon.loads_json() agree on the parsed value
     (two independent JSON parsers, same document).
  2. ctoon.dumps() of the natively-parsed value matches the paired .toon
     file byte-for-byte.
  3. ctoon.loads(ctoon.dumps(native_value)) == native_value (full round
     trip through TOON and back).

Finding while writing this: tests/data/twitter.toon (a 469KB corpus with
100 real tweets) failed check 2 on two independent counts, both traceable
to the *fixture* being stale, not the code -- confirmed against
toon-format/spec's own arrays-primitive.json fixture ("encodes empty
arrays" -> {"items": []} must encode as `items: []`, spec section 9.1;
twitter.toon instead had the older `urls[0]:` form for the same case) and
against the JSON source's own id_str field (exact-precision int64 handling
-- twitter.toon had lossy double-rounded id values, e.g. ...815700 where
the source, and id_str, say ...815681). Regenerated from the current,
spec-verified writer.
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

import ctoon

LOCAL_DATA_DIR = Path(__file__).parent.parent / "data"


def _pairs(directory: Path) -> list[tuple[Path, Path]]:
    """Every <name>.json in `directory` that has a matching <name>.toon."""
    out = []
    for jf in sorted(directory.glob("*.json")):
        tf = jf.with_suffix(".toon")
        if tf.is_file():
            out.append((jf, tf))
    return out


def _check_pair(json_path: Path, toon_path: Path) -> None:
    raw_json = json_path.read_text(encoding="utf-8")

    native = json.loads(raw_json)
    own = ctoon.loads_json(raw_json)
    assert native == own, (
        f"{json_path.name}: stdlib json and ctoon.loads_json() disagree "
        f"on the parsed value"
    )

    produced = ctoon.dumps(native)
    expected = toon_path.read_text(encoding="utf-8").rstrip("\n")
    assert produced.rstrip("\n") == expected, (
        f"{json_path.name}: ctoon.dumps(json.loads(...)) doesn't match "
        f"the paired {toon_path.name}"
    )

    round_tripped = ctoon.loads(produced)
    assert round_tripped == native, (
        f"{json_path.name}: TOON round trip changed the value"
    )


class TestLocalDataCorpus:
    """tests/data/*.json + *.toon -- always available, no skip needed."""

    @pytest.mark.parametrize(
        "json_path,toon_path", _pairs(LOCAL_DATA_DIR), ids=lambda p: p.name if isinstance(p, Path) else None
    )
    def test_pair(self, json_path: Path, toon_path: Path) -> None:
        _check_pair(json_path, toon_path)


class TestSpecExamplesCorpus:
    """toon-format/spec's examples/conversions/*.json + *.toon."""

    def test_all_pairs(self, spec_examples_dir: Path) -> None:
        pairs = _pairs(spec_examples_dir)
        assert pairs, f"no paired .json/.toon files found under {spec_examples_dir}"
        failures = []
        for json_path, toon_path in pairs:
            try:
                _check_pair(json_path, toon_path)
            except AssertionError as e:
                failures.append(str(e))
        assert not failures, "\n".join(failures)
