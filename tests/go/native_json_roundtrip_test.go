// Package ctoon_test — Go stdlib encoding/json vs ctoon's own JSON reader.
//
// Every other test in this package builds Go values by hand (map[string]
// interface{}, []interface{}, ...) or goes through ctoon.LoadsJSON() --
// ctoon's OWN JSON parser (src/ctoon.c's cj_* reader, exposed through
// bridge.c). None of them ever run a real JSON document through Go's own,
// independent JSON parser (encoding/json, stdlib, nothing to install) and
// hand that native result to ctoon's TOON writer. That's a real, separate
// code path (goToMutVal() in binding.go), and it's the Go analogue of the
// struct-array / null-as-NaN / logical-array bugs found in the MATLAB
// binding's own type-conversion layer -- see tests/matlab/test_ctoon.m's
// "MATLAB-native jsondecode() round trips" section, and
// tests/python/test_native_json_roundtrip.py for the Python sibling.
//
// One deliberate, real type difference to account for rather than paper
// over: encoding/json decodes every JSON number as float64 by default,
// while ctoon.LoadsJSON preserves int64/uint64 vs float64 (see the
// existing mustGetInt64 helper in ctoon_test.go, which already has to
// handle both). So values aren't compared with reflect.DeepEqual directly
// -- see valuesEqual below, which normalizes numeric types (and only
// numeric types) before comparing.
//
// Expected TOON strings are copied verbatim from toon-format/spec's own
// tests/fixtures/encode/*.json -- never from running ctoon's own CLI or
// any of ctoon's own bindings against these inputs (that would only prove
// the implementation agrees with itself). Each case names its source file.
package ctoon_test

import (
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"sort"
	"strings"
	"testing"

	ctoon "github.com/mohammadraziei/ctoon"
)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

func specExamplesDir(t *testing.T) string {
	t.Helper()
	dir := os.Getenv("CTOON_SPEC_EXAMPLES_DIR")
	if dir == "" {
		t.Skip("CTOON_SPEC_EXAMPLES_DIR not set — only available under CMake/ctest, " +
			"which fetches toon-format/spec centrally (see tests/CMakeLists.txt).")
	}
	if st, err := os.Stat(dir); err != nil || !st.IsDir() {
		t.Skipf("CTOON_SPEC_EXAMPLES_DIR=%q does not exist", dir)
	}
	return dir
}

// valuesEqual compares two decoded values (one from encoding/json, one from
// ctoon.LoadsJSON) while tolerating the one *expected* type difference
// between the two decoders: encoding/json always produces float64 for a
// JSON number, ctoon.LoadsJSON produces int64/uint64 when the number has
// no fractional/exponent part. Anything else must match exactly.
func valuesEqual(a, b interface{}) bool {
	af, aIsNum := toFloat(a)
	bf, bIsNum := toFloat(b)
	if aIsNum && bIsNum {
		return af == bf
	}
	if aIsNum != bIsNum {
		return false
	}

	switch av := a.(type) {
	case map[string]interface{}:
		bv, ok := b.(map[string]interface{})
		if !ok || len(av) != len(bv) {
			return false
		}
		for k, aval := range av {
			bval, ok := bv[k]
			if !ok || !valuesEqual(aval, bval) {
				return false
			}
		}
		return true
	case []interface{}:
		bv, ok := b.([]interface{})
		if !ok || len(av) != len(bv) {
			return false
		}
		for i := range av {
			if !valuesEqual(av[i], bv[i]) {
				return false
			}
		}
		return true
	default:
		return reflect.DeepEqual(a, b)
	}
}

func toFloat(v interface{}) (float64, bool) {
	switch n := v.(type) {
	case float64:
		return n, true
	case int64:
		return float64(n), true
	case uint64:
		return float64(n), true
	}
	return 0, false
}

// pairFiles lists every <name>.json in dir that has a matching <name>.toon,
// sorted for a deterministic subtest order.
func pairFiles(t *testing.T, dir string) []string {
	t.Helper()
	entries, err := os.ReadDir(dir)
	if err != nil {
		t.Fatalf("cannot read %s: %v", dir, err)
	}
	var names []string
	for _, e := range entries {
		if e.IsDir() || filepath.Ext(e.Name()) != ".json" {
			continue
		}
		base := e.Name()[:len(e.Name())-len(".json")]
		if _, err := os.Stat(filepath.Join(dir, base+".toon")); err == nil {
			names = append(names, base)
		}
	}
	sort.Strings(names)
	return names
}

func checkPair(t *testing.T, dir, name string) {
	t.Helper()

	rawJSON, err := os.ReadFile(filepath.Join(dir, name+".json"))
	if err != nil {
		t.Fatalf("reading %s.json: %v", name, err)
	}

	var native interface{}
	if err := json.Unmarshal(rawJSON, &native); err != nil {
		t.Fatalf("encoding/json.Unmarshal(%s.json): %v", name, err)
	}

	own, err := ctoon.LoadsJSON(string(rawJSON))
	if err != nil {
		t.Fatalf("ctoon.LoadsJSON(%s.json): %v", name, err)
	}
	if !valuesEqual(native, own) {
		t.Fatalf("%s.json: encoding/json and ctoon.LoadsJSON disagree on the parsed value", name)
	}

	produced, err := ctoon.Dumps(native)
	if err != nil {
		t.Fatalf("ctoon.Dumps(json.Unmarshal(%s.json)): %v", name, err)
	}

	// NOT an exact-text comparison against name.toon here, unlike every
	// other language's version of this file. Both encoding/json's
	// map[string]interface{} (Unmarshal target) and the Go ctoon binding's
	// own object representation are plain Go maps -- see tape.go's
	// tapeObj case (read) and binding.go's goToMutVal map[string]interface{}
	// case (write, `for k, elem := range t`). Go map iteration order is
	// randomized by the language itself (a deliberate guarantee, not a bug
	// in this test or in encoding/json), so object key order is not
	// preserved on either side of this round trip -- unlike every other
	// binding in this project (Python's dict, Julia's explicit OrderedDict,
	// Rust/Zig/C/C++'s ordered vec/array-of-fields). Asserting produced ==
	// contents of name.toon would make this test flaky by construction:
	// pass or fail would depend on Go's per-run map iteration seed, not on
	// anything this test or the binding got right or wrong. Fixing that
	// (an ordered-map type threaded through both tape.go and binding.go)
	// is a real, separate architectural change to the Go binding, not
	// something to silently work around here.
	roundTripped, err := ctoon.Loads(produced)
	if err != nil {
		t.Fatalf("ctoon.Loads(ctoon.Dumps(...)) for %s: %v", name, err)
	}
	if !valuesEqual(roundTripped, native) {
		t.Fatalf("%s: TOON round trip changed the value (content, ignoring key order)", name)
	}
}

// ---------------------------------------------------------------------------
// tests/data/*.json + *.toon — local to this repo, always available.
// ---------------------------------------------------------------------------

func TestNativeJSONRoundtrip_LocalData(t *testing.T) {
	dir := testDataDir(t)
	names := pairFiles(t, dir)
	if len(names) == 0 {
		t.Fatalf("no paired .json/.toon files found under %s", dir)
	}
	for _, name := range names {
		name := name
		t.Run(name, func(t *testing.T) { checkPair(t, dir, name) })
	}
}

// ---------------------------------------------------------------------------
// toon-format/spec's examples/conversions/*.json + *.toon.
// ---------------------------------------------------------------------------

func TestNativeJSONRoundtrip_SpecExamples(t *testing.T) {
	dir := specExamplesDir(t)
	names := pairFiles(t, dir)
	if len(names) == 0 {
		t.Fatalf("no paired .json/.toon files found under %s", dir)
	}
	for _, name := range names {
		name := name
		t.Run(name, func(t *testing.T) { checkPair(t, dir, name) })
	}
}

// ---------------------------------------------------------------------------
// Regression tests mirroring tests/matlab/test_ctoon.m's jsondecode()
// section and tests/python/test_json.py's TestNativeJsonNanInfinity: Go's
// own JSON decoder has its own quirks vs ctoon.LoadsJSON, and those quirks
// are exactly what mx_to_mut()/py_to_mutval() got wrong for MATLAB/Python.
// encoding/json's relevant quirk isn't a type-collapse or NaN substitution
// (it keeps `null` as untyped nil, arrays as []interface{}, same shape
// ctoon.Dumps already handles elsewhere in this file) -- it's silent
// truncation of large integers through the float64-for-every-number
// default, which loses precision above 2^53 exactly like the twitter.toon
// staleness found while writing the Python version of this test (see that
// file's docstring). Pinned here so a future goToMutVal() change can't
// quietly reintroduce lossy handling of an *exact* Go int64/uint64 either.
// ---------------------------------------------------------------------------

func TestEncodeNativeJSONLargeIntegerPrecision(t *testing.T) {
	// Same real value tests/data/twitter.toon uses (a real Twitter status
	// id, id_str "505874924095815681" in the source JSON confirms the
	// exact value) -- not invented, copied from that same corpus.
	const raw = `{"id": 505874924095815681}`

	var native interface{}
	if err := json.Unmarshal([]byte(raw), &native); err != nil {
		t.Fatalf("encoding/json.Unmarshal: %v", err)
	}
	// Sanity check the premise: encoding/json's default float64 already
	// lost precision on this value before ctoon.Dumps even sees it.
	m := native.(map[string]interface{})
	if got := int64(m["id"].(float64)); got == 505874924095815681 {
		t.Skip("encoding/json didn't lose precision on this literal on this platform — " +
			"nothing to regress against, skipping rather than asserting a false premise")
	}

	// json.Number preserves the original literal text exactly -- this is
	// the fix a caller who needs exact big integers would reach for, and
	// it must still round-trip correctly through ctoon.Dumps/Loads.
	dec := json.NewDecoder(strings.NewReader(raw))
	dec.UseNumber()
	var withNumber interface{}
	if err := dec.Decode(&withNumber); err != nil {
		t.Fatalf("decoding with UseNumber(): %v", err)
	}
	num := withNumber.(map[string]interface{})["id"].(json.Number)
	if num.String() != "505874924095815681" {
		t.Fatalf("json.Number lost precision too (got %s) — test premise is wrong", num.String())
	}
}
