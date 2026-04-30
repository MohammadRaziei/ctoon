// Package ctoon_test contains integration tests for the ctoon Go binding.
// Run directly:  go test ./tests/go/... -v
// Or via CMake:  cmake --build build --target ctoon_test_go
package ctoon_test

import (
	"math"
	"os"
	"path/filepath"
	"strings"
	"testing"

	ctoon "github.com/mohammadraziei/ctoon"
)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

func testDataDir(t *testing.T) string {
	t.Helper()
	dir, err := filepath.Abs(filepath.Join("..", "data"))
	if err != nil {
		t.Fatalf("cannot resolve test data dir: %v", err)
	}
	return dir
}

func mustGetString(t *testing.T, v interface{}, key string) string {
	t.Helper()
	m, ok := v.(map[string]interface{})
	if !ok {
		t.Fatalf("expected map, got %T", v)
	}
	s, ok := m[key].(string)
	if !ok {
		t.Fatalf("key %q: expected string, got %T", key, m[key])
	}
	return s
}

func mustGetInt64(t *testing.T, v interface{}, key string) int64 {
	t.Helper()
	m := v.(map[string]interface{})
	switch n := m[key].(type) {
	case int64:
		return n
	case uint64:
		return int64(n)
	default:
		t.Fatalf("key %q: expected int64/uint64, got %T (%v)", key, m[key], m[key])
	}
	return 0
}

func mustGetBool(t *testing.T, v interface{}, key string) bool {
	t.Helper()
	m := v.(map[string]interface{})
	b, ok := m[key].(bool)
	if !ok {
		t.Fatalf("key %q: expected bool, got %T", key, m[key])
	}
	return b
}

// ---------------------------------------------------------------------------
// Section 1 – API surface
// ---------------------------------------------------------------------------

func TestAPIExists(t *testing.T) {
	t.Run("Encode", func(t *testing.T) {
		if _, err := ctoon.Encode(map[string]interface{}{"k": "v"}, ctoon.DefaultEncodeOptions()); err != nil {
			t.Fatalf("Encode error: %v", err)
		}
	})
	t.Run("Decode", func(t *testing.T) {
		if _, err := ctoon.Decode("k: v", ctoon.DefaultDecodeOptions()); err != nil {
			t.Fatalf("Decode error: %v", err)
		}
	})
	t.Run("Dumps", func(t *testing.T) {
		if _, err := ctoon.Dumps(map[string]interface{}{"k": "v"}); err != nil {
			t.Fatalf("Dumps error: %v", err)
		}
	})
	t.Run("Loads", func(t *testing.T) {
		if _, err := ctoon.Loads("k: v"); err != nil {
			t.Fatalf("Loads error: %v", err)
		}
	})
	t.Run("DumpsJSON", func(t *testing.T) {
		if _, err := ctoon.DumpsJSON(map[string]interface{}{"k": "v"}, 2); err != nil {
			t.Fatalf("DumpsJSON error: %v", err)
		}
	})
	t.Run("LoadsJSON", func(t *testing.T) {
		if _, err := ctoon.LoadsJSON(`{"k":"v"}`); err != nil {
			t.Fatalf("LoadsJSON error: %v", err)
		}
	})
}

func TestDelimiterConstants(t *testing.T) {
	if ctoon.DelimiterComma == ctoon.DelimiterTab {
		t.Error("DelimiterComma == DelimiterTab")
	}
	if ctoon.DelimiterTab == ctoon.DelimiterPipe {
		t.Error("DelimiterTab == DelimiterPipe")
	}
}

func TestReadFlagConstants(t *testing.T) {
	if ctoon.ReadNoFlag != 0 {
		t.Errorf("ReadNoFlag should be 0, got %d", ctoon.ReadNoFlag)
	}
	if ctoon.ReadToon5 == 0 {
		t.Error("ReadToon5 should be non-zero")
	}
}

// ---------------------------------------------------------------------------
// Section 2 – Encoding
// ---------------------------------------------------------------------------

func TestEncodeSimpleObject(t *testing.T) {
	toon, err := ctoon.Dumps(map[string]interface{}{"name": "Alice", "age": int64(30)})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	for _, kw := range []string{"name", "Alice", "30"} {
		if !strings.Contains(toon, kw) {
			t.Errorf("missing %q in output: %s", kw, toon)
		}
	}
}

func TestEncodeNilValue(t *testing.T) {
	toon, err := ctoon.Dumps(map[string]interface{}{"key": nil})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !strings.Contains(toon, "null") {
		t.Errorf("expected 'null' in output: %s", toon)
	}
}

func TestEncodeBooleans(t *testing.T) {
	toon, err := ctoon.Dumps(map[string]interface{}{"active": true, "deleted": false})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	lower := strings.ToLower(toon)
	if !strings.Contains(lower, "true") || !strings.Contains(lower, "false") {
		t.Errorf("expected true/false in output: %s", toon)
	}
}

func TestEncodeFloat(t *testing.T) {
	toon, err := ctoon.Dumps(map[string]interface{}{"pi": 3.14159})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !strings.Contains(toon, "3.1415") {
		t.Errorf("expected '3.1415' in output: %s", toon)
	}
}

func TestEncodeNegativeInt(t *testing.T) {
	toon, err := ctoon.Dumps(map[string]interface{}{"temp": int64(-10)})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !strings.Contains(toon, "-10") {
		t.Errorf("expected '-10' in output: %s", toon)
	}
}

func TestEncodeArray(t *testing.T) {
	toon, err := ctoon.Dumps(map[string]interface{}{
		"items": []interface{}{int64(1), int64(2), int64(3)},
	})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	for _, kw := range []string{"items", "1", "2", "3"} {
		if !strings.Contains(toon, kw) {
			t.Errorf("missing %q in output: %s", kw, toon)
		}
	}
}

func TestEncodeNested(t *testing.T) {
	toon, err := ctoon.Dumps(map[string]interface{}{
		"outer": map[string]interface{}{
			"inner": map[string]interface{}{"value": int64(42)},
		},
	})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	for _, kw := range []string{"outer", "inner", "42"} {
		if !strings.Contains(toon, kw) {
			t.Errorf("missing %q in output: %s", kw, toon)
		}
	}
}

func TestEncodeWithPipeDelimiter(t *testing.T) {
	opts := ctoon.DefaultEncodeOptions()
	opts.Delimiter = ctoon.DelimiterPipe
	toon, err := ctoon.Encode(map[string]interface{}{
		"tags": []interface{}{"red", "blue", "green"},
	}, opts)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !strings.Contains(toon, "|") {
		t.Errorf("expected pipe delimiter in output: %s", toon)
	}
}

func TestEncodeWithTabDelimiter(t *testing.T) {
	opts := ctoon.DefaultEncodeOptions()
	opts.Delimiter = ctoon.DelimiterTab
	toon, err := ctoon.Encode(map[string]interface{}{
		"nums": []interface{}{int64(10), int64(20), int64(30)},
	}, opts)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !strings.Contains(toon, "\t") {
		t.Errorf("expected tab delimiter in output: %s", toon)
	}
}

func TestEncodeAllIntegerTypes(t *testing.T) {
	data := map[string]interface{}{
		"i": int(-1), "i8": int8(-2), "i16": int16(-3),
		"i32": int32(-4), "i64": int64(-5),
		"u": uint(6), "u8": uint8(7), "u16": uint16(8),
		"u32": uint32(9), "u64": uint64(10),
	}
	if _, err := ctoon.Dumps(data); err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
}

func TestEncodeFloat32(t *testing.T) {
	if _, err := ctoon.Dumps(map[string]interface{}{"v": float32(1.5)}); err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
}

func TestEncodeUnsupportedTypeReturnsError(t *testing.T) {
	type custom struct{ X int }
	if _, err := ctoon.Dumps(map[string]interface{}{"obj": custom{X: 1}}); err == nil {
		t.Error("expected error for unsupported type, got nil")
	}
}

// ---------------------------------------------------------------------------
// Section 3 – Decoding
// ---------------------------------------------------------------------------

func TestDecodeSimpleObject(t *testing.T) {
	val, err := ctoon.Loads("name: Alice\nage: 30")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if mustGetString(t, val, "name") != "Alice" {
		t.Error("name mismatch")
	}
	if mustGetInt64(t, val, "age") != 30 {
		t.Error("age mismatch")
	}
}

func TestDecodeBooleans(t *testing.T) {
	val, err := ctoon.Loads("active: true\ndeleted: false")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !mustGetBool(t, val, "active") {
		t.Error("active should be true")
	}
	if mustGetBool(t, val, "deleted") {
		t.Error("deleted should be false")
	}
}

func TestDecodeNull(t *testing.T) {
	val, err := ctoon.Loads("value: null")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if val.(map[string]interface{})["value"] != nil {
		t.Errorf("expected nil, got %v", val.(map[string]interface{})["value"])
	}
}

func TestDecodeArray(t *testing.T) {
	val, err := ctoon.Loads("tags[3]: red,blue,green")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	arr, ok := val.(map[string]interface{})["tags"].([]interface{})
	if !ok || len(arr) != 3 {
		t.Fatalf("expected 3-element slice, got %T / len=%d", val.(map[string]interface{})["tags"], len(arr))
	}
	if arr[0].(string) != "red" {
		t.Errorf("arr[0]: expected 'red', got %v", arr[0])
	}
}

func TestDecodeNested(t *testing.T) {
	val, err := ctoon.Loads("outer:\n  inner: value")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	outer := val.(map[string]interface{})["outer"].(map[string]interface{})
	if outer["inner"].(string) != "value" {
		t.Error("inner value mismatch")
	}
}

func TestDecodeNegativeNumber(t *testing.T) {
	val, err := ctoon.Loads("temp: -10")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if mustGetInt64(t, val, "temp") != -10 {
		t.Error("temp mismatch")
	}
}

func TestDecodeFloat(t *testing.T) {
	val, err := ctoon.Loads("pi: 3.14159")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	f, ok := val.(map[string]interface{})["pi"].(float64)
	if !ok {
		t.Fatalf("expected float64, got %T", val.(map[string]interface{})["pi"])
	}
	if math.Abs(f-3.14159) > 1e-4 {
		t.Errorf("pi: expected ~3.14159, got %f", f)
	}
}

func TestDecodeEmptyStringReturnsError(t *testing.T) {
	if _, err := ctoon.Loads(""); err == nil {
		t.Error("expected error for empty input, got nil")
	}
}

// ---------------------------------------------------------------------------
// Section 4 – Round-trip
// ---------------------------------------------------------------------------

func TestRoundtripSimple(t *testing.T) {
	original := map[string]interface{}{
		"name": "Alice", "age": int64(30), "active": true,
	}
	toon, err := ctoon.Dumps(original)
	if err != nil {
		t.Fatalf("encode error: %v", err)
	}
	result, err := ctoon.Loads(toon)
	if err != nil {
		t.Fatalf("decode error: %v", err)
	}
	if mustGetString(t, result, "name") != "Alice" {
		t.Error("name mismatch after roundtrip")
	}
	if mustGetInt64(t, result, "age") != 30 {
		t.Error("age mismatch after roundtrip")
	}
	if !mustGetBool(t, result, "active") {
		t.Error("active mismatch after roundtrip")
	}
}

func TestRoundtripNested(t *testing.T) {
	original := map[string]interface{}{
		"outer": map[string]interface{}{
			"inner": map[string]interface{}{"value": int64(42)},
		},
	}
	toon, err := ctoon.Dumps(original)
	if err != nil {
		t.Fatalf("encode error: %v", err)
	}
	result, err := ctoon.Loads(toon)
	if err != nil {
		t.Fatalf("decode error: %v", err)
	}
	outer := result.(map[string]interface{})["outer"].(map[string]interface{})
	inner := outer["inner"].(map[string]interface{})
	var got int64
	switch n := inner["value"].(type) {
	case int64:
		got = n
	case uint64:
		got = int64(n)
	default:
		t.Fatalf("unexpected type %T for value", inner["value"])
	}
	if got != 42 {
		t.Errorf("value: expected 42, got %d", got)
	}
}

func TestRoundtripWithNull(t *testing.T) {
	original := map[string]interface{}{"name": "Alice", "value": nil}
	toon, err := ctoon.Dumps(original)
	if err != nil {
		t.Fatalf("encode error: %v", err)
	}
	result, err := ctoon.Loads(toon)
	if err != nil {
		t.Fatalf("decode error: %v", err)
	}
	if result.(map[string]interface{})["value"] != nil {
		t.Errorf("expected nil, got %v", result.(map[string]interface{})["value"])
	}
}

func TestRoundtripWithArray(t *testing.T) {
	original := map[string]interface{}{
		"items": []interface{}{int64(1), int64(2), int64(3)},
	}
	toon, err := ctoon.Dumps(original)
	if err != nil {
		t.Fatalf("encode error: %v", err)
	}
	result, err := ctoon.Loads(toon)
	if err != nil {
		t.Fatalf("decode error: %v", err)
	}
	arr, ok := result.(map[string]interface{})["items"].([]interface{})
	if !ok || len(arr) != 3 {
		t.Errorf("expected 3-element slice, got %T / len=%d", result.(map[string]interface{})["items"], len(arr))
	}
}

func TestRoundtripComplex(t *testing.T) {
	original := map[string]interface{}{
		"order": map[string]interface{}{
			"id": "ORD-123",
			"customer": map[string]interface{}{
				"name": "John", "active": true,
			},
			"items": []interface{}{
				map[string]interface{}{"product": "Widget", "qty": int64(2)},
				map[string]interface{}{"product": "Gadget", "qty": int64(1)},
			},
		},
	}
	toon, err := ctoon.Dumps(original)
	if err != nil {
		t.Fatalf("encode error: %v", err)
	}
	for _, kw := range []string{"ORD-123", "John", "Widget", "Gadget"} {
		if !strings.Contains(toon, kw) {
			t.Errorf("missing %q in encoded output", kw)
		}
	}
	if _, err = ctoon.Loads(toon); err != nil {
		t.Fatalf("decode error: %v", err)
	}
}

// ---------------------------------------------------------------------------
// Section 5 – File I/O
// ---------------------------------------------------------------------------

func TestEncodeDecodeFile(t *testing.T) {
	path := filepath.Join(t.TempDir(), "test.toon")
	data := map[string]interface{}{"name": "Alice", "age": int64(30), "active": true}

	if err := ctoon.EncodeToFile(data, path, ctoon.DefaultEncodeOptions()); err != nil {
		t.Fatalf("EncodeToFile error: %v", err)
	}
	if _, err := os.Stat(path); os.IsNotExist(err) {
		t.Fatal("output file was not created")
	}
	result, err := ctoon.DecodeFromFile(path, ctoon.DefaultDecodeOptions())
	if err != nil {
		t.Fatalf("DecodeFromFile error: %v", err)
	}
	if mustGetString(t, result, "name") != "Alice" {
		t.Error("name mismatch after file roundtrip")
	}
	if mustGetInt64(t, result, "age") != 30 {
		t.Error("age mismatch after file roundtrip")
	}
}

func TestDecodeFromFile_NotFound(t *testing.T) {
	if _, err := ctoon.DecodeFromFile("/nonexistent/path/file.toon", ctoon.DefaultDecodeOptions()); err == nil {
		t.Error("expected error for missing file, got nil")
	}
}

// ---------------------------------------------------------------------------
// Section 6 – Sample data files (shared with C / C++ / Python tests)
// ---------------------------------------------------------------------------

func TestSampleFile_User(t *testing.T) {
	path := filepath.Join(testDataDir(t), "sample1_user.toon")
	val, err := ctoon.DecodeFromFile(path, ctoon.DefaultDecodeOptions())
	if err != nil {
		t.Fatalf("DecodeFromFile(%q): %v", path, err)
	}
	m := val.(map[string]interface{})
	if name, _ := m["name"].(string); name != "Alice" {
		t.Errorf("name: expected 'Alice', got %q", name)
	}
	if mustGetInt64(t, val, "age") != 30 {
		t.Error("age mismatch")
	}
	if active, _ := m["active"].(bool); !active {
		t.Error("active should be true")
	}
	tags, ok := m["tags"].([]interface{})
	if !ok || len(tags) != 3 {
		t.Errorf("tags: expected 3-element slice, got %T len=%d", m["tags"], len(tags))
	}
}

func TestSampleFile_NestedOrder(t *testing.T) {
	path := filepath.Join(testDataDir(t), "sample3_nested.toon")
	val, err := ctoon.DecodeFromFile(path, ctoon.DefaultDecodeOptions())
	if err != nil {
		t.Fatalf("DecodeFromFile(%q): %v", path, err)
	}
	order, ok := val.(map[string]interface{})["order"].(map[string]interface{})
	if !ok {
		t.Fatalf("expected 'order' map, got %T", val.(map[string]interface{})["order"])
	}
	if id, _ := order["id"].(string); id != "ORD-12345" {
		t.Errorf("order.id: expected 'ORD-12345', got %q", id)
	}
}

// ---------------------------------------------------------------------------
// Section 7 – JSON helpers
// ---------------------------------------------------------------------------

func TestDumpsLoadsJSON(t *testing.T) {
	data := map[string]interface{}{"name": "Alice", "age": int64(30)}
	json, err := ctoon.DumpsJSON(data, 2)
	if err != nil {
		t.Fatalf("DumpsJSON error: %v", err)
	}
	for _, kw := range []string{"Alice", "30"} {
		if !strings.Contains(json, kw) {
			t.Errorf("missing %q in JSON: %s", kw, json)
		}
	}
	back, err := ctoon.LoadsJSON(json)
	if err != nil {
		t.Fatalf("LoadsJSON error: %v", err)
	}
	if mustGetString(t, back, "name") != "Alice" {
		t.Error("name mismatch after JSON roundtrip")
	}
}

func TestDumpsJSONCompact(t *testing.T) {
	json, err := ctoon.DumpsJSON(map[string]interface{}{"x": int64(1)}, 0)
	if err != nil {
		t.Fatalf("DumpsJSON error: %v", err)
	}
	if strings.Contains(json, "\n") {
		t.Errorf("compact JSON should have no newlines, got: %s", json)
	}
}

func TestLoadJSON_File(t *testing.T) {
	path := filepath.Join(testDataDir(t), "sample1_user.json")
	val, err := ctoon.LoadJSON(path)
	if err != nil {
		t.Fatalf("LoadJSON(%q): %v", path, err)
	}
	if mustGetString(t, val, "name") != "Alice" {
		t.Error("name mismatch from JSON file")
	}
}

func TestDumpJSON_File(t *testing.T) {
	path := filepath.Join(t.TempDir(), "out.json")
	if err := ctoon.DumpJSON(map[string]interface{}{"hello": "world"}, path, 2); err != nil {
		t.Fatalf("DumpJSON error: %v", err)
	}
	if _, err := os.Stat(path); os.IsNotExist(err) {
		t.Fatal("JSON output file was not created")
	}
	back, err := ctoon.LoadJSON(path)
	if err != nil {
		t.Fatalf("LoadJSON after DumpJSON error: %v", err)
	}
	if mustGetString(t, back, "hello") != "world" {
		t.Error("value mismatch after JSON file roundtrip")
	}
}

func TestLoadJSON_NotFound(t *testing.T) {
	if _, err := ctoon.LoadJSON("/no/such/file.json"); err == nil {
		t.Error("expected error for missing file, got nil")
	}
}

// ---------------------------------------------------------------------------
// Section 8 – Aliases
// ---------------------------------------------------------------------------

func TestAliases(t *testing.T) {
	toon, err := ctoon.DumpsToon(map[string]interface{}{"k": "v"})
	if err != nil {
		t.Fatalf("DumpsToon: %v", err)
	}
	back, err := ctoon.LoadsToon(toon)
	if err != nil {
		t.Fatalf("LoadsToon: %v", err)
	}
	if mustGetString(t, back, "k") != "v" {
		t.Error("value mismatch via toon aliases")
	}
}

// ---------------------------------------------------------------------------
// Section 9 – Options
// ---------------------------------------------------------------------------

func TestDefaultEncodeOptions(t *testing.T) {
	opts := ctoon.DefaultEncodeOptions()
	if opts.Flag != ctoon.WriteNoFlag {
		t.Errorf("expected WriteNoFlag, got %d", opts.Flag)
	}
	if opts.Delimiter != ctoon.DelimiterComma {
		t.Errorf("expected DelimiterComma, got %d", opts.Delimiter)
	}
	if opts.Indent != 0 {
		t.Errorf("expected Indent=0, got %d", opts.Indent)
	}
}

func TestDefaultDecodeOptions(t *testing.T) {
	if opts := ctoon.DefaultDecodeOptions(); opts.Flag != ctoon.ReadToon5 {
		t.Errorf("expected ReadToon5, got %d", opts.Flag)
	}
}

func TestCustomWriteFlag_NewlineAtEnd(t *testing.T) {
	opts := ctoon.DefaultEncodeOptions()
	opts.Flag = ctoon.WriteNewlineAtEnd
	toon, err := ctoon.Encode(map[string]interface{}{"k": "v"}, opts)
	if err != nil {
		t.Fatalf("Encode with WriteNewlineAtEnd: %v", err)
	}
	if !strings.HasSuffix(toon, "\n") {
		t.Errorf("expected trailing newline, got: %q", toon)
	}
}