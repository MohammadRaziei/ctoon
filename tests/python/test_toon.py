"""Python tests for CToon TOON serialization."""

from __future__ import annotations

import pytest

import ctoon
from ctoon import Delimiter, EncodeOptions


class TestToonEncode:
    """Test TOON encoding."""

    def test_encode_simple_object(self):
        data = {"name": "Alice", "age": 30}
        toon = ctoon.encode(data)
        assert "name: Alice" in toon
        assert "age: 30" in toon

    def test_encode_with_delimiter(self):
        data = {"tags": ["red", "blue"]}
        options = EncodeOptions().set_delimiter(Delimiter.Pipe)
        toon = ctoon.encode(data, options)
        assert "red|blue" in toon

    def test_encode_with_indent(self):
        data = {"outer": {"items": [1, 2]}}
        options = EncodeOptions().set_indent(4)
        toon = ctoon.encode(data, options)
        assert "    " in toon

    def test_encode_nested_object(self):
        data = {
            "order": {
                "id": "ORD-123",
                "customer": {"name": "John"},
            }
        }
        toon = ctoon.encode(data)
        assert "id: ORD-123" in toon
        assert "name: John" in toon

    def test_encode_boolean_null(self):
        data = {"active": True, "value": None}
        toon = ctoon.encode(data)
        assert "active: true" in toon
        assert "value: null" in toon


class TestToonDecode:
    """Test TOON decoding."""

    def test_decode_simple_object(self):
        toon = "name: Alice\nage: 30"
        data = ctoon.decode(toon)
        assert data["name"] == "Alice"
        assert data["age"] == 30

    def test_decode_nested(self):
        toon = "order:\n  id: ORD-123\n  status: completed"
        data = ctoon.decode(toon)
        assert data["order"]["id"] == "ORD-123"
        assert data["order"]["status"] == "completed"

    def test_decode_array(self):
        toon = "tags[3]: red,blue,green"
        data = ctoon.decode(toon)
        assert len(data["tags"]) == 3
        assert data["tags"][0] == "red"


class TestToonRoundtrip:
    """Test TOON encode/decode roundtrip."""

    def test_roundtrip_simple(self):
        original = {"name": "Alice", "age": 30, "active": True}
        toon = ctoon.encode(original)
        result = ctoon.decode(toon)
        assert result["name"] == original["name"]
        assert result["age"] == original["age"]
        assert result["active"] == original["active"]

    def test_roundtrip_with_array(self):
        original = {"items": ["apple", "banana", "cherry"]}
        toon = ctoon.encode(original)
        result = ctoon.decode(toon)
        assert result["items"] == original["items"]
