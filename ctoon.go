// Package ctoon is the top-level Go entry-point for the CToon library.
//
// It re-exports every public symbol from the implementation package
// (src/bindings/go/binding) so callers only need to import one path:
//
//	import "github.com/mohammadraziei/ctoon"
//
// This file plays the same role as Python's __init__.py:
// nothing lives here except re-exports and type aliases.
//
// Example:
//
//	data := map[string]interface{}{"name": "Alice", "age": int64(30)}
//	toon, _ := ctoon.Dumps(data)
//	fmt.Println(toon)
//
//	val, _ := ctoon.Loads(toon)
//	fmt.Println(val)
package ctoon

import (
	binding "github.com/mohammadraziei/ctoon/src/bindings/go"
)

// ---------------------------------------------------------------------------
// Option types  (re-exported from binding)
// ---------------------------------------------------------------------------

type (
	// ReadFlag controls TOON parsing behaviour (bit flags).
	ReadFlag = binding.ReadFlag
	// WriteFlag controls TOON serialisation behaviour (bit flags).
	WriteFlag = binding.WriteFlag
	// Delimiter selects the array-value separator used when writing TOON.
	Delimiter = binding.Delimiter
	// EncodeOptions bundles all serialisation settings.
	EncodeOptions = binding.EncodeOptions
	// DecodeOptions bundles all parsing settings.
	DecodeOptions = binding.DecodeOptions
)

// Read flag constants.
const (
	ReadNoFlag              = binding.ReadNoFlag
	ReadAllowTrailingCommas = binding.ReadAllowTrailingCommas
	ReadAllowComments       = binding.ReadAllowComments
	ReadAllowInfAndNaN      = binding.ReadAllowInfAndNaN
	ReadAllowInvalidUnicode = binding.ReadAllowInvalidUnicode
	ReadToon5               = binding.ReadToon5
)

// Write flag constants.
const (
	WriteNoFlag              = binding.WriteNoFlag
	WriteEscapeUnicode       = binding.WriteEscapeUnicode
	WriteEscapeSlashes       = binding.WriteEscapeSlashes
	WriteAllowInfAndNaN      = binding.WriteAllowInfAndNaN
	WriteInfAndNaNAsNull     = binding.WriteInfAndNaNAsNull
	WriteAllowInvalidUnicode = binding.WriteAllowInvalidUnicode
	WriteLengthMarker        = binding.WriteLengthMarker
	WriteNewlineAtEnd        = binding.WriteNewlineAtEnd
)

// Delimiter constants.
const (
	DelimiterComma = binding.DelimiterComma
	DelimiterTab   = binding.DelimiterTab
	DelimiterPipe  = binding.DelimiterPipe
)

// ---------------------------------------------------------------------------
// Default options constructors
// ---------------------------------------------------------------------------

// DefaultEncodeOptions returns compact TOON encoding with no special flags.
var DefaultEncodeOptions = binding.DefaultEncodeOptions

// DefaultDecodeOptions returns lenient TOON5 parsing.
var DefaultDecodeOptions = binding.DefaultDecodeOptions

// ---------------------------------------------------------------------------
// Core encode / decode
// ---------------------------------------------------------------------------

// Encode serialises a Go value to a TOON string.
//
//	toon, err := ctoon.Encode(myData, ctoon.DefaultEncodeOptions())
var Encode = binding.Encode

// Decode parses a TOON string and returns the equivalent Go value.
//
//	val, err := ctoon.Decode(toonStr, ctoon.DefaultDecodeOptions())
var Decode = binding.Decode

// EncodeToFile serialises v and writes the TOON result to the file at path.
var EncodeToFile = binding.EncodeToFile

// DecodeFromFile reads and parses a TOON file, returning the Go value.
var DecodeFromFile = binding.DecodeFromFile

// ---------------------------------------------------------------------------
// Convenience aliases  (mirror Python loads / dumps naming)
// ---------------------------------------------------------------------------

// Loads parses a TOON string with default options. Alias for Decode.
var Loads = binding.Loads

// Dumps serialises v to a TOON string with default options. Alias for Encode.
var Dumps = binding.Dumps

// LoadsToon explicitly names the TOON format. Alias for Loads.
var LoadsToon = binding.LoadsToon

// DumpsToon explicitly names the TOON format. Alias for Dumps.
var DumpsToon = binding.DumpsToon

// ---------------------------------------------------------------------------
// encoding/json-signature-compatible aliases
// ---------------------------------------------------------------------------

// Marshal serialises v to TOON-formatted bytes. Same signature as
// encoding/json.Marshal — for anyone reaching for the stdlib-idiomatic
// name out of habit. Loads/Dumps remain the primary API (kept consistent
// with every other ctoon binding's naming); this is a thin wrapper, not
// a separate implementation. Does not support struct reflection/tags —
// see binding.Marshal's doc comment for exactly what's supported.
var Marshal = binding.Marshal

// Unmarshal parses TOON-formatted bytes into the value pointed to by v.
// Same signature as encoding/json.Unmarshal.
var Unmarshal = binding.Unmarshal

// JSONMarshal serialises v to JSON-formatted bytes via ctoon's JSON writer.
// Same signature as encoding/json.Marshal. Named with the JSON prefix
// (not Marshal*JSON*) to avoid colliding in spirit with Go's
// json.Marshaler interface method name.
var JSONMarshal = binding.JSONMarshal

// JSONUnmarshal parses JSON-formatted bytes via ctoon's JSON reader into
// the value pointed to by v. Same signature as encoding/json.Unmarshal.
var JSONUnmarshal = binding.JSONUnmarshal

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------

// LoadsJSON parses a JSON string using ctoon's JSON reader.
var LoadsJSON = binding.LoadsJSON

// DumpsJSON serialises v to a JSON string.
//
//	json, err := ctoon.DumpsJSON(myData, 2)
var DumpsJSON = binding.DumpsJSON

// LoadJSON reads and parses a JSON file.
var LoadJSON = binding.LoadJSON

// DumpJSON serialises v and writes JSON to path.
var DumpJSON = binding.DumpJSON