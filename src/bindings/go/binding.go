// Package binding implements Go bindings for the CToon library via CGo.
//
// This file is the implementation layer. Users should import the top-level
// "ctoon" package (ctoon.go in the module root) rather than this package
// directly.
//
// Supported types for encoding/decoding:
//   - nil             ↔  TOON null
//   - bool            ↔  TOON true/false
//   - int … int64     ↔  TOON signed integer
//   - uint … uint64   ↔  TOON unsigned integer
//   - float32/float64 ↔  TOON real
//   - string          ↔  TOON string
//   - []interface{}   ↔  TOON array
//   - map[string]interface{} ↔  TOON object
package binding

/*
#cgo CFLAGS: -I../../../include -I../../../src -DCTOON_ENABLE_JSON=1
#cgo LDFLAGS: -lm

#include "ctoon.c"
#include <stdlib.h>
#include <string.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// ---------------------------------------------------------------------------
// Public option types
// ---------------------------------------------------------------------------

// ReadFlag controls TOON parsing behaviour (bit flags).
// We use plain uint32 instead of aliasing C.ctoon_read_flag so that the
// values can be declared as Go const (CGo static-const variables are not
// Go compile-time constants).
type ReadFlag uint32

const (
	ReadNoFlag              ReadFlag = 0
	ReadAllowTrailingCommas ReadFlag = 1 << 2
	ReadAllowComments       ReadFlag = 1 << 3
	ReadAllowInfAndNaN      ReadFlag = 1 << 4
	ReadAllowInvalidUnicode ReadFlag = 1 << 6
	// ReadToon5 enables the full TOON5 extension set:
	// trailing commas, comments, inf/nan, extended numbers/escapes/whitespace,
	// single-quoted strings, unquoted keys.
	ReadToon5 ReadFlag = (1 << 2) | (1 << 3) | (1 << 4) |
		(1 << 9) | (1 << 10) | (1 << 11) | (1 << 12) | (1 << 13)
)

// WriteFlag controls TOON serialisation behaviour (bit flags).
type WriteFlag uint32

const (
	WriteNoFlag              WriteFlag = 0
	WriteEscapeUnicode       WriteFlag = 1 << 1
	WriteEscapeSlashes       WriteFlag = 1 << 2
	WriteAllowInfAndNaN      WriteFlag = 1 << 3
	WriteInfAndNaNAsNull     WriteFlag = 1 << 4
	WriteAllowInvalidUnicode WriteFlag = 1 << 5
	WriteLengthMarker        WriteFlag = 1 << 6
	WriteNewlineAtEnd        WriteFlag = 1 << 7
)

// Delimiter selects the array-value separator used when writing TOON.
type Delimiter uint32

const (
	DelimiterComma Delimiter = 0 // , (default)
	DelimiterTab   Delimiter = 1 // \t
	DelimiterPipe  Delimiter = 2 // |
)

// EncodeOptions controls TOON serialisation behaviour.
type EncodeOptions struct {
	Flag      WriteFlag
	Delimiter Delimiter
	// Indent sets the indentation level (0 = compact / minified).
	Indent int
}

// DecodeOptions controls TOON parsing behaviour.
type DecodeOptions struct {
	Flag ReadFlag
}

// DefaultEncodeOptions returns compact encoding with no special flags.
func DefaultEncodeOptions() EncodeOptions {
	return EncodeOptions{
		Flag:      WriteNoFlag,
		Delimiter: DelimiterComma,
		Indent:    0,
	}
}

// DefaultDecodeOptions returns lenient TOON5 parsing.
func DefaultDecodeOptions() DecodeOptions {
	return DecodeOptions{Flag: ReadToon5}
}

// ---------------------------------------------------------------------------
// Encode  (Go value → TOON string)
// ---------------------------------------------------------------------------

// Encode serialises a Go value to a TOON-formatted string.
func Encode(v interface{}, opts EncodeOptions) (string, error) {
	doc := C.ctoon_mut_doc_new(nil)
	if doc == nil {
		return "", fmt.Errorf("ctoon: failed to create mutable document")
	}
	defer C.ctoon_mut_doc_free(doc)

	root, err := goToMutVal(doc, v)
	if err != nil {
		return "", err
	}
	C.ctoon_mut_doc_set_root(doc, root)

	wopts := C.ctoon_write_options{
		flag:      C.ctoon_write_flag(opts.Flag),
		delimiter: C.ctoon_delimiter(opts.Delimiter),
	}
	var outLen C.size_t
	var werr C.ctoon_write_err
	// signature: (doc, opts, alc, len*, err*) → char*
	raw := C.ctoon_mut_write_opts(doc, &wopts, nil, &outLen, &werr)
	if raw == nil {
		msg := "ctoon: write error"
		if werr.msg != nil {
			msg = C.GoString(werr.msg)
		}
		return "", fmt.Errorf("%s (code %d)", msg, werr.code)
	}
	defer C.free(unsafe.Pointer(raw))

	return C.GoStringN(raw, C.int(outLen)), nil
}

// Decode parses a TOON string and returns the equivalent Go value.
// emptyCStrByte backs a non-NULL, zero-length C string pointer for empty
// Go strings. ctoon's C API treats a NULL `str` pointer as an error
// regardless of `len` (see ctoon_mut_val_one_str in ctoon.h: it checks
// `doc && str`, not the length) — the same convention ctoon_read_opts and
// friends follow for their `dat` parameter. A genuine NULL here would be
// silently rejected even though the length is correctly 0, so this must
// never be nil.
var emptyCStrByte [1]C.char

// readOnlyCStr returns a pointer to a Go string's own backing bytes for
// passing into a CGo call — no C.CString malloc+copy needed, since the
// callee only reads len(s) bytes and never requires a null terminator.
// Safe as long as the C side treats it as read-only for the call's
// duration, which is why the caller must ensure CTOON_READ_INSITU is
// stripped first (a Go string's bytes can't safely be mutated in place —
// same reasoning ctoon_read()'s own C wrapper uses to justify clearing
// that bit for callers passing a `const char *`).
func readOnlyCStr(s string) *C.char {
	if len(s) == 0 {
		return (*C.char)(unsafe.Pointer(&emptyCStrByte[0]))
	}
	return (*C.char)(unsafe.Pointer(unsafe.StringData(s)))
}

// insituBit is CTOON_READ_INSITU — not exposed as a ReadFlag constant in
// this package's public API, but stripped defensively in case a caller
// constructs a raw ReadFlag value that happens to set it.
// insituBit mirrors CTOON_READ_INSITU from ctoon.h (= 1 << 0). Hardcoded
// rather than referenced via cgo: it's a #define macro with no linkable
// storage, which cgo can't reliably export as a constant across packages.
const insituBit C.ctoon_read_flag = 1 << 0

func Decode(s string, opts DecodeOptions) (interface{}, error) {
	var rerr C.ctoon_read_err
	flag := C.ctoon_read_flag(opts.Flag) &^ insituBit
	// signature: (dat, len, flag, alc*, err*) → doc*
	doc := C.ctoon_read_opts(readOnlyCStr(s), C.size_t(len(s)), flag, nil, &rerr)
	if doc == nil {
		msg := "ctoon: parse error"
		if rerr.msg != nil {
			msg = C.GoString(rerr.msg)
		}
		return nil, fmt.Errorf("%s (pos %d, code %d)", msg, rerr.pos, rerr.code)
	}
	defer C.ctoon_doc_free(doc)

	return valToGo(C.ctoon_doc_get_root(doc)), nil
}

// EncodeToFile serialises v and writes the TOON output to path.
func EncodeToFile(v interface{}, path string, opts EncodeOptions) error {
	doc := C.ctoon_mut_doc_new(nil)
	if doc == nil {
		return fmt.Errorf("ctoon: failed to create mutable document")
	}
	defer C.ctoon_mut_doc_free(doc)

	root, err := goToMutVal(doc, v)
	if err != nil {
		return err
	}
	C.ctoon_mut_doc_set_root(doc, root)

	cp := C.CString(path)
	defer C.free(unsafe.Pointer(cp))

	wopts := C.ctoon_write_options{
		flag:      C.ctoon_write_flag(opts.Flag),
		delimiter: C.ctoon_delimiter(opts.Delimiter),
	}
	var werr C.ctoon_write_err
	// signature: (path, doc, opts, alc, err*) → bool
	ok := C.ctoon_mut_write_file(cp, doc, &wopts, nil, &werr)
	if !ok {
		msg := "ctoon: write file error"
		if werr.msg != nil {
			msg = C.GoString(werr.msg)
		}
		return fmt.Errorf("%s (code %d)", msg, werr.code)
	}
	return nil
}

// DecodeFromFile reads and parses a TOON file at path.
func DecodeFromFile(path string, opts DecodeOptions) (interface{}, error) {
	cp := C.CString(path)
	defer C.free(unsafe.Pointer(cp))

	var rerr C.ctoon_read_err
	doc := C.ctoon_read_file(cp, C.ctoon_read_flag(opts.Flag), nil, &rerr)
	if doc == nil {
		msg := "ctoon: read file error"
		if rerr.msg != nil {
			msg = C.GoString(rerr.msg)
		}
		return nil, fmt.Errorf("%s (pos %d, code %d)", msg, rerr.pos, rerr.code)
	}
	defer C.ctoon_doc_free(doc)

	return valToGo(C.ctoon_doc_get_root(doc)), nil
}

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------

// LoadsJSON parses a JSON string into a Go value (uses ctoon's JSON reader).
func LoadsJSON(json string) (interface{}, error) {
	var rerr C.ctoon_read_err
	doc := C.ctoon_read_json(readOnlyCStr(json), C.size_t(len(json)), C.ctoon_read_flag(ReadNoFlag), nil, &rerr)
	if doc == nil {
		msg := "ctoon: JSON parse error"
		if rerr.msg != nil {
			msg = C.GoString(rerr.msg)
		}
		return nil, fmt.Errorf("%s (pos %d, code %d)", msg, rerr.pos, rerr.code)
	}
	defer C.ctoon_doc_free(doc)

	return valToGo(C.ctoon_doc_get_root(doc)), nil
}

// DumpsJSON serialises a Go value to a JSON string.
func DumpsJSON(v interface{}, indent int) (string, error) {
	doc := C.ctoon_mut_doc_new(nil)
	if doc == nil {
		return "", fmt.Errorf("ctoon: failed to create mutable document")
	}
	defer C.ctoon_mut_doc_free(doc)

	root, err := goToMutVal(doc, v)
	if err != nil {
		return "", err
	}
	C.ctoon_mut_doc_set_root(doc, root)

	var outLen C.size_t
	var werr C.ctoon_write_err
	// signature: (doc, indent, flags, alc, len*, err*) → char*
	raw := C.ctoon_mut_doc_to_json(doc, C.int(indent), C.ctoon_write_flag(WriteNoFlag), nil, &outLen, &werr)
	if raw == nil {
		msg := "ctoon: JSON write error"
		if werr.msg != nil {
			msg = C.GoString(werr.msg)
		}
		return "", fmt.Errorf("%s (code %d)", msg, werr.code)
	}
	defer C.free(unsafe.Pointer(raw))

	return C.GoStringN(raw, C.int(outLen)), nil
}

// LoadJSON reads a JSON file and returns the Go value.
func LoadJSON(path string) (interface{}, error) {
	cp := C.CString(path)
	defer C.free(unsafe.Pointer(cp))

	var rerr C.ctoon_read_err
	doc := C.ctoon_read_json_file(cp, C.ctoon_read_flag(ReadNoFlag), nil, &rerr)
	if doc == nil {
		msg := "ctoon: JSON file read error"
		if rerr.msg != nil {
			msg = C.GoString(rerr.msg)
		}
		return nil, fmt.Errorf("%s (code %d)", msg, rerr.code)
	}
	defer C.ctoon_doc_free(doc)

	return valToGo(C.ctoon_doc_get_root(doc)), nil
}

// DumpJSON serialises v and writes JSON to path.
func DumpJSON(v interface{}, path string, indent int) error {
	doc := C.ctoon_mut_doc_new(nil)
	if doc == nil {
		return fmt.Errorf("ctoon: failed to create mutable document")
	}
	defer C.ctoon_mut_doc_free(doc)

	root, err := goToMutVal(doc, v)
	if err != nil {
		return err
	}
	C.ctoon_mut_doc_set_root(doc, root)

	cp := C.CString(path)
	defer C.free(unsafe.Pointer(cp))

	var outLen C.size_t
	var werr C.ctoon_write_err
	// signature: (doc, indent, flag, alc, len*, err*) → char*
	// We get the JSON string first, then write ourselves via write_json_mut if available,
	// or fall back to manual file write.
	raw := C.ctoon_write_json_mut(doc, C.int(indent), C.ctoon_write_flag(WriteNoFlag), nil, &outLen, &werr)
	if raw == nil {
		msg := "ctoon: JSON serialisation error"
		if werr.msg != nil {
			msg = C.GoString(werr.msg)
		}
		return fmt.Errorf("%s (code %d)", msg, werr.code)
	}
	defer C.free(unsafe.Pointer(raw))

	// Write the raw bytes to file via C stdio.
	fp := C.fopen(cp, C.CString("wb"))
	if fp == nil {
		return fmt.Errorf("ctoon: cannot open file %q for writing", path)
	}
	C.fwrite(unsafe.Pointer(raw), 1, outLen, fp)
	C.fclose(fp)
	return nil
}

// ---------------------------------------------------------------------------
// Convenience aliases  (mirror Python's loads/dumps naming)
// ---------------------------------------------------------------------------

// Loads is an alias for Decode with default options.
func Loads(s string) (interface{}, error) {
	if s == "" {
		return nil, fmt.Errorf("ctoon: cannot decode empty input")
	}
	return Decode(s, DefaultDecodeOptions())
}

// Dumps is an alias for Encode with default options.
func Dumps(v interface{}) (string, error) {
	return Encode(v, DefaultEncodeOptions())
}

// LoadsToon is an alias for Loads (explicit TOON variant).
func LoadsToon(s string) (interface{}, error) { return Loads(s) }

// DumpsToon is an alias for Dumps (explicit TOON variant).
func DumpsToon(v interface{}) (string, error) { return Dumps(v) }

// ---------------------------------------------------------------------------
// Internal: Go value → ctoon_mut_val
// ---------------------------------------------------------------------------

// mutStrncpy creates a copied TOON string value directly from a Go string's
// backing bytes — no intermediate C.CString malloc+free round trip needed,
// since ctoon_mut_strncpy copies the data itself and doesn't require a
// null terminator. Passing a pointer into Go-managed memory into a CGo call
// is safe as long as the C side doesn't retain it past the call, which
// holds here.
func mutStrncpy(doc *C.ctoon_mut_doc, s string) *C.ctoon_mut_val {
	if len(s) == 0 {
		return C.ctoon_mut_strncpy(doc, (*C.char)(unsafe.Pointer(&emptyCStrByte[0])), 0)
	}
	return C.ctoon_mut_strncpy(doc, (*C.char)(unsafe.Pointer(unsafe.StringData(s))), C.size_t(len(s)))
}

func goToMutVal(doc *C.ctoon_mut_doc, v interface{}) (*C.ctoon_mut_val, error) {
	if v == nil {
		return C.ctoon_mut_null(doc), nil
	}
	switch t := v.(type) {
	case bool:
		if t {
			return C.ctoon_mut_true(doc), nil
		}
		return C.ctoon_mut_false(doc), nil

	case int:
		return C.ctoon_mut_sint(doc, C.int64_t(t)), nil
	case int8:
		return C.ctoon_mut_sint(doc, C.int64_t(t)), nil
	case int16:
		return C.ctoon_mut_sint(doc, C.int64_t(t)), nil
	case int32:
		return C.ctoon_mut_sint(doc, C.int64_t(t)), nil
	case int64:
		return C.ctoon_mut_sint(doc, C.int64_t(t)), nil

	case uint:
		return C.ctoon_mut_uint(doc, C.uint64_t(t)), nil
	case uint8:
		return C.ctoon_mut_uint(doc, C.uint64_t(t)), nil
	case uint16:
		return C.ctoon_mut_uint(doc, C.uint64_t(t)), nil
	case uint32:
		return C.ctoon_mut_uint(doc, C.uint64_t(t)), nil
	case uint64:
		return C.ctoon_mut_uint(doc, C.uint64_t(t)), nil

	case float32:
		return C.ctoon_mut_real(doc, C.double(t)), nil
	case float64:
		return C.ctoon_mut_real(doc, C.double(t)), nil

	case string:
		return mutStrncpy(doc, t), nil

	case []interface{}:
		arr := C.ctoon_mut_arr(doc)
		if arr == nil {
			return nil, fmt.Errorf("ctoon: failed to create array")
		}
		for _, elem := range t {
			child, err := goToMutVal(doc, elem)
			if err != nil {
				return nil, err
			}
			if !C.ctoon_mut_arr_append(arr, child) {
				return nil, fmt.Errorf("ctoon: failed to append to array")
			}
		}
		return arr, nil

	case map[string]interface{}:
		obj := C.ctoon_mut_obj(doc)
		if obj == nil {
			return nil, fmt.Errorf("ctoon: failed to create object")
		}
		for k, elem := range t {
			keyVal := mutStrncpy(doc, k)
			if keyVal == nil {
				return nil, fmt.Errorf("ctoon: failed to create object key %q", k)
			}
			child, err := goToMutVal(doc, elem)
			if err != nil {
				return nil, err
			}
			if !C.ctoon_mut_obj_put(obj, keyVal, child) {
				return nil, fmt.Errorf("ctoon: failed to put key %q into object", k)
			}
		}
		return obj, nil

	default:
		return nil, fmt.Errorf("ctoon: unsupported Go type %T", v)
	}
}

// ---------------------------------------------------------------------------
// Internal: ctoon_val → Go value
// ---------------------------------------------------------------------------

func valToGo(val *C.ctoon_val) interface{} {
	if val == nil {
		return nil
	}
	// Dispatch on a single ctoon_get_type() call rather than walking a
	// chain of ctoon_is_X() predicate calls: each one is a separate CGo
	// call, and CGo call overhead (goroutine/OS-thread transition) dwarfs
	// the actual work being done for a single field. That's especially
	// costly for objects and arrays, since they used to be checked last
	// in the chain — every single nested object paid for 7 failed
	// predicate calls before even starting to read its own fields.
	switch C.ctoon_get_type(val) {
	case C.CTOON_TYPE_NULL:
		return nil
	case C.CTOON_TYPE_BOOL:
		return bool(C.ctoon_get_bool(val))
	case C.CTOON_TYPE_NUM:
		switch {
		case bool(C.ctoon_is_uint(val)):
			return uint64(C.ctoon_get_uint(val))
		case bool(C.ctoon_is_sint(val)):
			return int64(C.ctoon_get_sint(val))
		default:
			return float64(C.ctoon_get_real(val))
		}
	case C.CTOON_TYPE_STR:
		ptr := C.ctoon_get_str(val)
		n := C.ctoon_get_len(val)
		return C.GoStringN(ptr, C.int(n))
	case C.CTOON_TYPE_ARR:
		size := int(C.ctoon_arr_size(val))
		out := make([]interface{}, size)
		for i := 0; i < size; i++ {
			out[i] = valToGo(C.ctoon_arr_get(val, C.size_t(i)))
		}
		return out
	case C.CTOON_TYPE_OBJ:
		out := make(map[string]interface{})
		var iter C.ctoon_obj_iter
		C.ctoon_obj_iter_init(val, &iter)
		for bool(C.ctoon_obj_iter_has_next(&iter)) {
			keyVal := C.ctoon_obj_iter_next(&iter)
			if keyVal == nil {
				break
			}
			valPtr := C.ctoon_obj_iter_get_val(keyVal)
			kPtr := C.ctoon_get_str(keyVal)
			kLen := C.ctoon_get_len(keyVal)
			key := C.GoStringN(kPtr, C.int(kLen))
			out[key] = valToGo(valPtr)
		}
		return out
	default:
		// raw/unknown — best-effort string
		raw := C.ctoon_get_raw(val)
		if raw != nil {
			return C.GoString(raw)
		}
		return nil
	}
}