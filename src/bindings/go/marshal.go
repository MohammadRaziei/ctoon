package binding

// Marshal/Unmarshal/JSONMarshal/JSONUnmarshal — aliases matching the
// encoding/json package's exact function signatures
// (func([]byte)/([]byte, error)/error, not Loads/Dumps' string-based
// ones), for anyone reaching for the stdlib-idiomatic names out of habit.
//
// Loads/Dumps/LoadsJSON/DumpsJSON remain the primary, documented API —
// kept consistent with the naming used across every other ctoon binding
// (Python's loads/dumps, C's ctoon_read/ctoon_write, ...). These are
// thin wrappers around them, not a separate implementation.
//
// Unlike encoding/json's Marshal, these do not use struct reflection or
// `toon:"..."` struct tags to serialise arbitrary struct types — they
// support exactly the same Go value shapes Dumps/Loads do: nil, bool,
// the numeric kinds, string, []interface{}, and map[string]interface{}.
// Unmarshal only fills pointers to those same shapes (or to numeric/
// string/bool types convertible from them); it returns an error for an
// arbitrary struct target rather than silently leaving it unfilled.

import (
	"fmt"
	"reflect"
)

// Marshal serialises v to TOON-formatted bytes. Signature-compatible with
// encoding/json.Marshal. Equivalent to Dumps(v) with the result converted
// to a []byte.
func Marshal(v interface{}) ([]byte, error) {
	s, err := Dumps(v)
	if err != nil {
		return nil, err
	}
	return []byte(s), nil
}

// Unmarshal parses TOON-formatted bytes and stores the result in the
// value pointed to by v. Signature-compatible with encoding/json.Unmarshal.
// v must be a non-nil pointer; see the package doc comment above for which
// target types are supported.
func Unmarshal(data []byte, v interface{}) error {
	decoded, err := Loads(string(data))
	if err != nil {
		return err
	}
	return assignDecoded(decoded, v)
}

// JSONMarshal serialises v to JSON-formatted bytes using ctoon's own JSON
// writer. Signature-compatible with encoding/json.Marshal. Equivalent to
// DumpsJSON(v, 0) (compact output) with the result converted to a []byte
// — call DumpsJSON directly for a specific indent width.
func JSONMarshal(v interface{}) ([]byte, error) {
	s, err := DumpsJSON(v, 0)
	if err != nil {
		return nil, err
	}
	return []byte(s), nil
}

// JSONUnmarshal parses JSON-formatted bytes using ctoon's own JSON reader
// and stores the result in the value pointed to by v.
// Signature-compatible with encoding/json.Unmarshal; same target-type
// support as Unmarshal.
func JSONUnmarshal(data []byte, v interface{}) error {
	decoded, err := LoadsJSON(string(data))
	if err != nil {
		return err
	}
	return assignDecoded(decoded, v)
}

// assignDecoded stores decoded (whatever Loads/LoadsJSON returned) into
// the value v points to, via reflection.
func assignDecoded(decoded interface{}, v interface{}) error {
	rv := reflect.ValueOf(v)
	if rv.Kind() != reflect.Ptr || rv.IsNil() {
		return fmt.Errorf("ctoon: Unmarshal target must be a non-nil pointer, got %T", v)
	}
	elem := rv.Elem()
	if !elem.CanSet() {
		return fmt.Errorf("ctoon: Unmarshal target is not settable")
	}

	if decoded == nil {
		elem.Set(reflect.Zero(elem.Type()))
		return nil
	}

	dv := reflect.ValueOf(decoded)

	if dv.Type().AssignableTo(elem.Type()) {
		elem.Set(dv)
		return nil
	}

	if dv.Type().ConvertibleTo(elem.Type()) {
		switch elem.Kind() {
		case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64,
			reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64,
			reflect.Float32, reflect.Float64, reflect.String, reflect.Bool:
			elem.Set(dv.Convert(elem.Type()))
			return nil
		}
	}

	return fmt.Errorf(
		"ctoon: cannot unmarshal decoded %T into target of type %s "+
			"(struct targets with field tags aren't supported — decode into "+
			"interface{}/map[string]interface{} instead, or use Loads directly)",
		decoded, elem.Type())
}
