"""Python tests for CToon JSON serialization."""

from __future__ import annotations

import pytest

import ctoon


class TestJsonBasic:
    """Test basic JSON serialization."""

    def test_loads_json_object(self):
        data = ctoon.loads_json('{"name": "Alice", "age": 30}')
        assert data["name"] == "Alice"
        assert data["age"] == 30

    def test_loads_json_array(self):
        data = ctoon.loads_json('[1, 2, 3]')
        assert data == [1, 2, 3]

    def test_loads_json_primitives(self):
        assert ctoon.loads_json('"hello"') == "hello"
        assert ctoon.loads_json("42") == 42
        assert ctoon.loads_json("3.14") == pytest.approx(3.14)
        assert ctoon.loads_json("true") is True
        assert ctoon.loads_json("false") is False
        assert ctoon.loads_json("null") is None

    def test_dumps_json_object(self):
        data = {"name": "Bob", "age": 25}
        json_str = ctoon.dumps_json(data)
        parsed = ctoon.loads_json(json_str)
        assert parsed["name"] == "Bob"
        assert parsed["age"] == 25

    def test_dumps_json_array(self):
        data = [1, 2, 3, "four"]
        json_str = ctoon.dumps_json(data)
        parsed = ctoon.loads_json(json_str)
        assert parsed == data

    def test_dumps_json_indent(self):
        data = {"name": "Alice"}
        json_str = ctoon.dumps_json(data, indent=4)
        assert "    " in json_str

    def test_roundtrip_object(self):
        original = {"user": "admin", "active": True, "score": 95.5}
        json_str = ctoon.dumps_json(original)
        result = ctoon.loads_json(json_str)
        assert result == original

    def test_roundtrip_nested(self):
        original = {
            "users": [
                {"name": "Alice", "role": "admin"},
                {"name": "Bob", "role": "user"},
            ]
        }
        json_str = ctoon.dumps_json(original)
        result = ctoon.loads_json(json_str)
        assert result["users"][0]["name"] == "Alice"
        assert result["users"][1]["role"] == "user"
