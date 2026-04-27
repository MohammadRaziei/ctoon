"""Python tests for CToon JSON serialization."""

from __future__ import annotations

import pytest

import ctoon


class TestJsonBasic:
    """Test basic JSON serialization."""

    def test_loads_json_primitives(self):
        """Test loading JSON primitives (strings, numbers, booleans, null)."""
        assert ctoon.loads_json('"hello"') == "hello"
        assert ctoon.loads_json("42") == 42
        assert ctoon.loads_json("3.14") == pytest.approx(3.14)
        assert ctoon.loads_json("true") is True
        assert ctoon.loads_json("false") is False
        assert ctoon.loads_json("null") is None

    def test_loads_json_string(self):
        """Test loading a simple JSON string value."""
        data = ctoon.loads_json('"test"')
        assert data == "test"

    def test_loads_json_number(self):
        """Test loading a simple JSON number."""
        data = ctoon.loads_json("123")
        assert data == 123

    def test_dumps_json_object(self):
        """Test serializing a dict to JSON string."""
        data = {"name": "Bob", "age": 25}
        json_str = ctoon.dumps_json(data)
        assert "name" in json_str
        assert "Bob" in json_str
        assert "age" in json_str
        assert "25" in json_str

    def test_dumps_json_array(self):
        """Test serializing a list to JSON string."""
        data = [1, 2, 3, "four"]
        json_str = ctoon.dumps_json(data)
        assert "1" in json_str
        assert "2" in json_str
        assert "3" in json_str
        assert "four" in json_str

    def test_dumps_json_indent(self):
        """Test JSON serialization with custom indent."""
        data = {"name": "Alice"}
        json_str = ctoon.dumps_json(data, indent=4)
        assert "    " in json_str

    def test_dumps_json_boolean(self):
        """Test JSON serialization with boolean values."""
        data = {"active": True, "deleted": False}
        json_str = ctoon.dumps_json(data)
        assert "true" in json_str.lower()
        assert "false" in json_str.lower()

    def test_dumps_json_null(self):
        """Test JSON serialization with None value."""
        data = {"value": None}
        json_str = ctoon.dumps_json(data)
        assert "null" in json_str

    def test_dumps_json_nested(self):
        """Test JSON serialization with nested structures."""
        data = {
            "users": [
                {"name": "Alice", "role": "admin"},
                {"name": "Bob", "role": "user"},
            ]
        }
        json_str = ctoon.dumps_json(data)
        assert "Alice" in json_str
        assert "Bob" in json_str
        assert "admin" in json_str
        assert "user" in json_str

    def test_dumps_json_complex(self):
        """Test JSON serialization with complex nested data."""
        data = {
            "order": {
                "id": "ORD-123",
                "customer": {
                    "name": "John",
                    "active": True
                },
                "items": [
                    {"name": "Widget", "qty": 2},
                    {"name": "Gadget", "qty": 1}
                ]
            }
        }
        json_str = ctoon.dumps_json(data)
        assert "ORD-123" in json_str
        assert "John" in json_str
        assert "Widget" in json_str
        assert "Gadget" in json_str


class TestJsonFileIO:
    """Test JSON file I/O."""

    def test_dump_json_file(self, tmp_path):
        """Test dumping data to a JSON file."""
        data = {"name": "Alice", "age": 30}
        filepath = str(tmp_path / "test.json")

        ctoon.dump_json(data, filepath)

        # Verify file was created
        assert tmp_path.joinpath("test.json").exists()

    def test_load_json_file(self, tmp_path):
        """Test loading data from a JSON file."""
        data = {"name": "Alice", "age": 30, "active": True}
        filepath = str(tmp_path / "test.json")

        ctoon.dump_json(data, filepath)
        result = ctoon.load_json(filepath)

        assert isinstance(result, (dict, str))  # May return string or dict


class TestJsonEdgeCases:
    """Test edge cases for JSON serialization."""

    def test_empty_dict(self):
        """Test serializing an empty dict."""
        data = {}
        json_str = ctoon.dumps_json(data)
        assert isinstance(json_str, str)

    def test_empty_list(self):
        """Test serializing an empty list."""
        data = []
        json_str = ctoon.dumps_json(data)
        assert isinstance(json_str, str)

    def test_unicode_string(self):
        """Test serializing unicode strings."""
        data = {"text": "سلام"}
        json_str = ctoon.dumps_json(data)
        assert isinstance(json_str, str)

    def test_negative_numbers(self):
        """Test serializing negative numbers."""
        data = {"temp": -10, "balance": -999.99}
        json_str = ctoon.dumps_json(data)
        assert "-" in json_str

    def test_mixed_types_in_list(self):
        """Test serializing a list with mixed types."""
        data = [1, "two", 3.0, True, None]
        json_str = ctoon.dumps_json(data)
        assert isinstance(json_str, str)