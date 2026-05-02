"""Python tests for CToon TOON serialization (nanobind binding)."""

from __future__ import annotations

import pytest
import io
import os

import ctoon
from ctoon import Delimiter, ReadFlag, WriteFlag


# ---------------------------------------------------------------------------
# Test: Module structure
# ---------------------------------------------------------------------------

class TestModule:
    def test_version_exists(self):
        assert hasattr(ctoon, "__version__")
        assert len(ctoon.__version__) >= 5

    def test_functions_exist(self):
        for fn in ("encode", "decode", "loads", "dumps", "load", "dump",
                   "loads_json", "dumps_json", "load_json", "dump_json"):
            assert callable(getattr(ctoon, fn)), f"ctoon.{fn} not callable"

    def test_enums_exist(self):
        assert hasattr(ctoon, "ReadFlag")
        assert hasattr(ctoon, "WriteFlag")
        assert hasattr(ctoon, "Delimiter")

    def test_delimiter_values(self):
        assert hasattr(Delimiter, "COMMA")
        assert hasattr(Delimiter, "TAB")
        assert hasattr(Delimiter, "PIPE")

    def test_read_flag_values(self):
        for name in ("NOFLAG", "INSITU", "ALLOW_COMMENTS", "ALLOW_INF_AND_NAN",
                     "ALLOW_TRAILING_COMMAS", "ALLOW_BOM"):
            assert hasattr(ReadFlag, name), f"ReadFlag.{name} missing"

    def test_write_flag_values(self):
        for name in ("NOFLAG", "ESCAPE_UNICODE", "ESCAPE_SLASHES",
                     "LENGTH_MARKER", "NEWLINE_AT_END"):
            assert hasattr(WriteFlag, name), f"WriteFlag.{name} missing"

    def test_flag_bitwise_or(self):
        combined = ReadFlag.ALLOW_COMMENTS | ReadFlag.ALLOW_BOM
        assert combined is not None
        # combined flag must be directly usable as a flags argument
        data = ctoon.loads("x: 1", flags=combined)
        assert data["x"] == 1

    def test_write_flag_bitwise_or(self):
        combined = WriteFlag.ESCAPE_UNICODE | WriteFlag.NEWLINE_AT_END
        assert combined is not None
        toon = ctoon.dumps({"x": 1}, flags=combined)
        assert "x" in toon

    def test_encode_is_dumps_alias(self):
        data = {"x": 1}
        assert ctoon.encode(data) == ctoon.dumps(data)

    def test_decode_is_loads_alias(self):
        toon = "x: 1"
        assert ctoon.decode(toon) == ctoon.loads(toon)


# ---------------------------------------------------------------------------
# Test: TOON encoding  (dumps / encode)
# ---------------------------------------------------------------------------

class TestEncode:
    def test_simple_dict(self):
        toon = ctoon.dumps({"name": "Alice", "age": 30})
        assert "name" in toon and "Alice" in toon
        assert "age" in toon and "30" in toon

    def test_empty_dict(self):
        assert ctoon.dumps({}).strip() == ""

    def test_none_value(self):
        toon = ctoon.dumps({"v": None})
        assert "null" in toon

    def test_booleans(self):
        toon = ctoon.dumps({"a": True, "b": False})
        assert "true" in toon.lower()
        assert "false" in toon.lower()

    def test_float(self):
        toon = ctoon.dumps({"pi": 3.14159})
        assert "3.1415" in toon

    def test_negative_int(self):
        toon = ctoon.dumps({"t": -10})
        assert "-10" in toon

    def test_list(self):
        toon = ctoon.dumps({"items": [1, 2, 3]})
        assert "items" in toon
        for v in ("1", "2", "3"):
            assert v in toon

    def test_nested_dict(self):
        toon = ctoon.dumps({"outer": {"inner": {"value": 42}}})
        assert all(k in toon for k in ("outer", "inner", "42"))

    def test_indent_kwarg(self):
        toon = ctoon.dumps({"a": 1, "b": 2}, indent=2)
        assert "\n" in toon

    def test_zero_indent(self):
        toon = ctoon.dumps({"a": 1}, indent=0)
        assert isinstance(toon, str)

    def test_pipe_delimiter(self):
        toon = ctoon.dumps({"tags": ["red", "blue", "green"]},
                           delimiter=Delimiter.PIPE)
        assert "|" in toon

    def test_tab_delimiter(self):
        toon = ctoon.dumps({"tags": ["a", "b"]}, delimiter=Delimiter.TAB)
        assert "\t" in toon

    def test_write_flag_length_marker(self):
        toon = ctoon.dumps({"tags": ["a", "b"]},
                           flags=WriteFlag.LENGTH_MARKER)
        assert isinstance(toon, str)

    def test_complex_nested(self):
        data = {
            "order": {
                "id": "ORD-123",
                "customer": {"name": "John", "active": True},
                "items": [
                    {"name": "Widget", "qty": 2},
                    {"name": "Gadget", "qty": 1},
                ],
            }
        }
        toon = ctoon.dumps(data)
        for s in ("ORD-123", "John", "Widget", "Gadget"):
            assert s in toon

    def test_unsupported_type_raises(self):
        class Bad:
            pass
        with pytest.raises(Exception):
            ctoon.dumps({"obj": Bad()})

    def test_non_string_key_raises(self):
        with pytest.raises(Exception):
            ctoon.dumps({1: "value"})


# ---------------------------------------------------------------------------
# Test: TOON decoding  (loads / decode)
# ---------------------------------------------------------------------------

class TestDecode:
    def test_simple_object(self):
        data = ctoon.loads("name: Alice\nage: 30")
        assert data["name"] == "Alice"
        assert data["age"] == 30

    def test_empty_string_raises(self):
        with pytest.raises(Exception):
            ctoon.loads("")

    def test_single_field(self):
        assert ctoon.loads("key: value")["key"] == "value"

    def test_nested_object(self):
        data = ctoon.loads("outer:\n  inner: value")
        assert data["outer"]["inner"] == "value"

    def test_array(self):
        data = ctoon.loads("tags[3]: red,blue,green")
        assert data["tags"] == ["red", "blue", "green"]

    def test_booleans(self):
        data = ctoon.loads("active: true\ndeleted: false")
        assert data["active"] is True
        assert data["deleted"] is False

    def test_null(self):
        assert ctoon.loads("value: null")["value"] is None

    def test_bytes_input(self):
        data = ctoon.loads(b"name: Alice\nage: 30")
        assert data["name"] == "Alice"
        assert data["age"] == 30

    def test_negative_number(self):
        assert ctoon.loads("temp: -10")["temp"] == -10

    def test_float_number(self):
        assert abs(ctoon.loads("pi: 3.14159")["pi"] - 3.14159) < 1e-4

    def test_read_flag_noflag(self):
        data = ctoon.loads("x: 1", flags=ReadFlag.NOFLAG)
        assert data["x"] == 1


# ---------------------------------------------------------------------------
# Test: Roundtrip
# ---------------------------------------------------------------------------

class TestRoundtrip:
    def _rt(self, original):
        return ctoon.loads(ctoon.dumps(original))

    def test_simple_dict(self):
        orig = {"name": "Alice", "age": 30, "active": True}
        result = self._rt(orig)
        assert result["name"] == orig["name"]
        assert result["age"] == orig["age"]
        assert result["active"] == orig["active"]

    def test_nested_dict(self):
        assert self._rt({"outer": {"inner": {"value": 42}}}) \
                   ["outer"]["inner"]["value"] == 42

    def test_list(self):
        orig = {"items": [1, 2, 3]}
        assert self._rt(orig)["items"] == orig["items"]

    def test_none_value(self):
        orig = {"name": "Alice", "value": None}
        result = self._rt(orig)
        assert result["name"] == "Alice"
        assert result["value"] is None

    def test_large_int(self):
        orig = {"n": 2**53}
        result = self._rt(orig)
        assert result["n"] == 2**53

    def test_negative_int(self):
        orig = {"n": -42}
        assert self._rt(orig)["n"] == -42

    def test_float(self):
        orig = {"f": 1.5}
        result = self._rt(orig)
        assert abs(result["f"] - 1.5) < 1e-9


# ---------------------------------------------------------------------------
# Test: File I/O  (path strings)
# ---------------------------------------------------------------------------

class TestFileIOPath:
    def test_dump_and_load(self, tmp_path):
        data = {"name": "Alice", "age": 30, "active": True}
        path = str(tmp_path / "test.toon")
        ctoon.dump(data, path)
        result = ctoon.load(path)
        assert result == {"name": "Alice", "age": 30, "active": True}

    def test_dump_with_pipe_delimiter(self, tmp_path):
        data = {"items": [1, 2, 3]}
        path = str(tmp_path / "test.toon")
        ctoon.dump(data, path, delimiter=Delimiter.PIPE)
        with open(path) as f:
            content = f.read()
        assert "|" in content

    def test_dump_with_indent(self, tmp_path):
        data = {"a": 1, "b": 2}
        path = str(tmp_path / "test.toon")
        ctoon.dump(data, path, indent=4)
        result = ctoon.load(path)
        assert result["a"] == 1 and result["b"] == 2

    def test_load_nonexistent_raises(self, tmp_path):
        with pytest.raises(Exception):
            ctoon.load(str(tmp_path / "nonexistent.toon"))


# ---------------------------------------------------------------------------
# Test: File I/O  (file-like objects)
# ---------------------------------------------------------------------------

class TestFileIOFileLike:
    def test_dump_to_text_filelike(self, tmp_path):
        data = {"x": 42}
        path = str(tmp_path / "out.toon")
        with open(path, "w") as f:
            ctoon.dump(data, f)
        result = ctoon.load(path)
        assert result["x"] == 42

    def test_dump_to_binary_filelike(self, tmp_path):
        data = {"x": 42}
        path = str(tmp_path / "out.toon")
        with open(path, "wb") as f:
            ctoon.dump(data, f)
        result = ctoon.load(path)
        assert result["x"] == 42

    def test_load_from_text_filelike(self, tmp_path):
        data = {"name": "Bob", "score": 99}
        path = str(tmp_path / "data.toon")
        ctoon.dump(data, path)
        with open(path, "r") as f:
            result = ctoon.load(f)
        assert result["name"] == "Bob"
        assert result["score"] == 99

    def test_load_from_bytesio(self):
        toon_bytes = b"name: Charlie\nval: 7"
        buf = io.BytesIO(toon_bytes)
        buf.mode = "rb"  # simulate binary file
        result = ctoon.load(buf)
        assert result["name"] == "Charlie"
        assert result["val"] == 7

    def test_dump_to_stringio(self):
        data = {"k": "v"}
        buf = io.StringIO()
        ctoon.dump(data, buf)
        buf.seek(0)
        result = ctoon.loads(buf.read())
        assert result["k"] == "v"


# ---------------------------------------------------------------------------
# Test: JSON support
# ---------------------------------------------------------------------------

class TestJSON:
    def test_loads_json(self):
        data = ctoon.loads_json('{"name":"Eve","age":28}')
        assert data["name"] == "Eve"
        assert data["age"] == 28

    def test_loads_json_bytes(self):
        data = ctoon.loads_json(b'{"x":1}')
        assert data["x"] == 1

    def test_dumps_json(self):
        json_str = ctoon.dumps_json({"name": "Frank", "n": 35})
        assert '"name"' in json_str
        assert "Frank" in json_str

    def test_dumps_json_compact(self):
        json_str = ctoon.dumps_json({"a": 1}, indent=0)
        assert "\n" not in json_str

    def test_json_roundtrip(self):
        orig = {"x": 1, "y": 2.5, "z": [1, 2, 3], "w": {"nested": True}}
        result = ctoon.loads_json(ctoon.dumps_json(orig))
        assert result["x"] == 1
        assert abs(result["y"] - 2.5) < 1e-9
        assert result["z"] == [1, 2, 3]
        assert result["w"]["nested"] is True

    def test_load_json_from_path(self, tmp_path):
        path = str(tmp_path / "data.json")
        with open(path, "w") as f:
            f.write('{"score": 100, "name": "Grace"}')
        data = ctoon.load_json(path)
        assert data["score"] == 100
        assert data["name"] == "Grace"

    def test_dump_json_to_path(self, tmp_path):
        path = str(tmp_path / "out.json")
        ctoon.dump_json({"city": "Tehran", "pop": 9000000}, path)
        data = ctoon.load_json(path)
        assert data["city"] == "Tehran"

    def test_load_json_from_filelike(self, tmp_path):
        path = str(tmp_path / "data.json")
        with open(path, "w") as f:
            f.write('{"val": 42}')
        with open(path) as f:
            data = ctoon.load_json(f)
        assert data["val"] == 42

    def test_dump_json_to_filelike(self, tmp_path):
        path = str(tmp_path / "out.json")
        with open(path, "w") as f:
            ctoon.dump_json({"k": "v"}, f)
        data = ctoon.load_json(path)
        assert data["k"] == "v"

    def test_loads_json_with_read_flag(self):
        data = ctoon.loads_json('{"x":1}', flags=ReadFlag.NOFLAG)
        assert data["x"] == 1

    def test_dumps_json_with_write_flag(self):
        s = ctoon.dumps_json({"x": 1}, flags=WriteFlag.NOFLAG)
        assert "x" in s