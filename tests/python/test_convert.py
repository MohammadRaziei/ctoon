"""ctoon.json_to_toon / toon_to_json (+ _file): direct text -> text conversion.

Expected values come only from independent sources: hand-written strings,
the paired .toon fixture files, and Python's stdlib ``json`` (never from
the ctoon CLI or from ctoon's own loads/dumps).
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

import ctoon

LOCAL_DATA_DIR = Path(__file__).parent.parent / "data"


def _pairs(directory: Path):
    return [(jf, jf.with_suffix(".toon")) for jf in sorted(directory.glob("*.json"))
            if jf.with_suffix(".toon").is_file()]


def _check_pair(json_path: Path, toon_path: Path) -> None:
    raw_json = json_path.read_text(encoding="utf-8")
    expected_toon = toon_path.read_text(encoding="utf-8").rstrip("\n")

    assert ctoon.json_to_toon(raw_json).rstrip("\n") == expected_toon, json_path.name
    # bytes input takes the same path
    assert ctoon.json_to_toon(raw_json.encode()).rstrip("\n") == expected_toon

    back = ctoon.toon_to_json(expected_toon)
    assert json.loads(back) == json.loads(raw_json), json_path.name


class TestBasics:
    def test_json_to_toon(self):
        assert ctoon.json_to_toon('{"name":"Alice","age":30,"tags":["a","b"]}') == \
            "name: Alice\nage: 30\ntags[2]: a,b"

    def test_tabular(self):
        assert ctoon.json_to_toon('{"u":[{"id":1,"n":"x"},{"id":2,"n":"y"}]}') == \
            "u[2]{id,n}:\n  1,x\n  2,y"

    def test_toon_to_json_compact_and_indented(self):
        assert ctoon.toon_to_json("a: 1") == '{"a":1}'
        assert ctoon.toon_to_json("a: 1", indent=2) == '{\n  "a": 1\n}'

    def test_options(self):
        assert ctoon.json_to_toon('{"t":[1,2,3]}', delimiter=ctoon.Delimiter.PIPE) == "t[3|]: 1|2|3"

    def test_unicode_roundtrip(self):
        s = '{"k":"héllo ✓ 日本"}'
        assert json.loads(ctoon.toon_to_json(ctoon.json_to_toon(s))) == json.loads(s)

    def test_invalid_input_raises(self):
        with pytest.raises(RuntimeError):
            ctoon.json_to_toon('{"a":')
        with pytest.raises(RuntimeError):
            ctoon.toon_to_json("a[2]: 1")  # declared length mismatch (strict)

    def test_wrong_type_raises(self):
        with pytest.raises(TypeError):
            ctoon.json_to_toon(123)


class TestFiles:
    def test_files(self, tmp_path: Path):
        j = tmp_path / "in.json"
        t = tmp_path / "out.toon"
        b = tmp_path / "back.json"
        j.write_text('{"k":[1,2],"s":"hi"}')
        ctoon.json_to_toon_file(str(j), str(t))
        assert t.read_text() == "k[2]: 1,2\ns: hi"
        ctoon.toon_to_json_file(str(t), str(b))
        assert b.read_text() == '{"k":[1,2],"s":"hi"}'

    def test_failed_conversion_does_not_create_output(self, tmp_path: Path):
        j = tmp_path / "bad.json"
        j.write_text('{"a":')
        t = tmp_path / "never.toon"
        with pytest.raises(RuntimeError):
            ctoon.json_to_toon_file(str(j), str(t))
        assert not t.exists()
        with pytest.raises(RuntimeError):
            ctoon.json_to_toon_file(str(tmp_path / "missing.json"), str(t))


class TestLocalDataCorpus:
    @pytest.mark.parametrize("json_path,toon_path", _pairs(LOCAL_DATA_DIR),
                             ids=lambda p: p.name if isinstance(p, Path) else None)
    def test_pair(self, json_path: Path, toon_path: Path):
        _check_pair(json_path, toon_path)


class TestSpecExamplesCorpus:
    def test_all_pairs(self, spec_examples_dir: Path):
        pairs = _pairs(spec_examples_dir)
        assert pairs
        failures = []
        for jp, tp in pairs:
            try:
                _check_pair(jp, tp)
            except AssertionError as e:
                failures.append(str(e))
        assert not failures, "\n".join(failures)
