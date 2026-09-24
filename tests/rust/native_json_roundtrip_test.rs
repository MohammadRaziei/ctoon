//! serde_json (a dev-dependency added specifically for this file — ctoon's
//! crate itself stays zero-dependency, see Cargo.toml's [dev-dependencies]
//! comment) vs ctoon's own JSON reader/writer.
//!
//! Every other Rust test builds `ctoon::Value` directly or goes through
//! `ctoon::loads_json()` -- ctoon's OWN JSON parser (src/ctoon.c's cj_*
//! reader via ffi.rs). None of them ever run a real JSON document through
//! an independent JSON parser and hand *that* result to ctoon's TOON
//! writer. That conversion path (the `to_ctoon_value()` function below,
//! serde_json::Value -> ctoon::Value) is the Rust analogue of the
//! struct-array / null-as-NaN / logical-array bugs found in the MATLAB
//! binding's own type-conversion layer -- see tests/matlab/test_ctoon.m's
//! "MATLAB-native jsondecode() round trips" section, and
//! tests/python/test_native_json_roundtrip.py / tests/go's
//! native_json_roundtrip_test.go for the sibling tests in the other
//! bindings.
//!
//! serde_json::Number, unlike Go's encoding/json, already distinguishes
//! u64/i64/f64 (via as_u64()/as_i64()/as_f64()), so to_ctoon_value() maps
//! it onto ctoon::Value::{Uint,Sint,Real} directly with no float64
//! coercion in between -- no MATLAB/Go-style precision loss to work around
//! here. The comparison in values_equal() is still written to tolerate it
//! defensively (comparing numerically, not variant-for-variant) in case
//! that ever changes upstream.
//!
//! Expected TOON strings are copied verbatim from toon-format/spec's own
//! tests/fixtures/encode/*.json -- never from running ctoon's own CLI or
//! any of ctoon's own bindings against these inputs (that would only prove
//! the implementation agrees with itself). Each case names its source file.

use ctoon::Value;
use serde_json::Value as JsonValue;
use std::fs;
use std::path::{Path, PathBuf};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// `tests/data/`, relative to the crate root (see tests/rust/CMakeLists.txt
/// and tests/go's testDataDir for the equivalent in that binding).
fn test_data_dir() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/data")
}

/// Returns `None` (meaning "skip these tests") when CTOON_SPEC_EXAMPLES_DIR
/// isn't set, or doesn't point at a real directory -- only available under
/// CMake/ctest, which fetches toon-format/spec centrally (see
/// tests/CMakeLists.txt). Early-return-with-eprintln is used instead of
/// panicking: cargo's built-in test harness has no runtime "skip" status,
/// so an early return is the closest equivalent (test still reports as
/// passed, having deliberately done nothing), same convention every other
/// binding's env-gated tests use here.
fn spec_examples_dir() -> Option<PathBuf> {
    let dir = std::env::var("CTOON_SPEC_EXAMPLES_DIR").ok()?;
    if dir.is_empty() {
        return None;
    }
    let path = PathBuf::from(dir);
    if path.is_dir() {
        Some(path)
    } else {
        None
    }
}

fn to_ctoon_value(v: &JsonValue) -> Value {
    match v {
        JsonValue::Null => Value::Null,
        JsonValue::Bool(b) => Value::Bool(*b),
        JsonValue::Number(n) => {
            if let Some(u) = n.as_u64() {
                Value::Uint(u)
            } else if let Some(i) = n.as_i64() {
                Value::Sint(i)
            } else {
                Value::Real(n.as_f64().expect("serde_json::Number is always one of u64/i64/f64"))
            }
        }
        JsonValue::String(s) => Value::Str(s.clone()),
        JsonValue::Array(arr) => Value::Array(arr.iter().map(to_ctoon_value).collect()),
        JsonValue::Object(map) => {
            Value::Object(map.iter().map(|(k, v)| (k.clone(), to_ctoon_value(v))).collect())
        }
    }
}

/// Numeric-tolerant equality between a serde_json::Value and a ctoon::Value
/// (rather than converting one to the other and using derived PartialEq,
/// which would hide exactly the kind of int/float mismatch this test
/// exists to catch).
fn values_equal(native: &JsonValue, own: &Value) -> bool {
    match (native, own) {
        (JsonValue::Null, Value::Null) => true,
        (JsonValue::Bool(a), Value::Bool(b)) => a == b,
        (JsonValue::Number(a), Value::Uint(b)) => a.as_u64() == Some(*b),
        (JsonValue::Number(a), Value::Sint(b)) => a.as_i64() == Some(*b),
        (JsonValue::Number(a), Value::Real(b)) => a.as_f64() == Some(*b),
        (JsonValue::String(a), Value::Str(b)) => a == b,
        (JsonValue::Array(a), Value::Array(b)) => {
            a.len() == b.len() && a.iter().zip(b.iter()).all(|(x, y)| values_equal(x, y))
        }
        (JsonValue::Object(a), Value::Object(b)) => {
            a.len() == b.len()
                && a.iter().all(|(k, v)| {
                    b.iter().find(|(bk, _)| bk == k).map_or(false, |(_, bv)| values_equal(v, bv))
                })
        }
        _ => false,
    }
}

/// Every `<name>.json` in `dir` with a matching `<name>.toon`, sorted for a
/// deterministic run order.
fn pair_files(dir: &Path) -> Vec<String> {
    let mut names: Vec<String> = fs::read_dir(dir)
        .unwrap_or_else(|e| panic!("cannot read {}: {}", dir.display(), e))
        .filter_map(|e| e.ok())
        .filter_map(|e| {
            let path = e.path();
            if path.extension()? != "json" {
                return None;
            }
            let stem = path.file_stem()?.to_str()?.to_string();
            if dir.join(format!("{stem}.toon")).is_file() {
                Some(stem)
            } else {
                None
            }
        })
        .collect();
    names.sort();
    names
}

fn check_pair(dir: &Path, name: &str) {
    let raw_json = fs::read_to_string(dir.join(format!("{name}.json")))
        .unwrap_or_else(|e| panic!("reading {name}.json: {e}"));

    let native: JsonValue = serde_json::from_str(&raw_json)
        .unwrap_or_else(|e| panic!("serde_json::from_str({name}.json): {e}"));

    let own = ctoon::loads_json(&raw_json)
        .unwrap_or_else(|e| panic!("ctoon::loads_json({name}.json): {e:?}"));

    assert!(
        values_equal(&native, &own),
        "{name}.json: serde_json and ctoon::loads_json disagree on the parsed value"
    );

    let expected_value = to_ctoon_value(&native);
    let produced = ctoon::dumps(&expected_value)
        .unwrap_or_else(|e| panic!("ctoon::dumps(serde_json value) for {name}: {e:?}"));

    let expected = fs::read_to_string(dir.join(format!("{name}.toon")))
        .unwrap_or_else(|e| panic!("reading {name}.toon: {e}"));

    assert_eq!(
        produced.trim_end_matches('\n'),
        expected.trim_end_matches('\n'),
        "{name}.json: ctoon::dumps(serde_json value) doesn't match paired {name}.toon"
    );

    let round_tripped = ctoon::loads(&produced)
        .unwrap_or_else(|e| panic!("ctoon::loads(ctoon::dumps(...)) for {name}: {e:?}"));
    assert_eq!(round_tripped, own, "{name}: TOON round trip changed the value");
}

// ---------------------------------------------------------------------------
// tests/data/*.json + *.toon — local to this repo, always available.
// ---------------------------------------------------------------------------

#[test]
fn native_json_roundtrip_local_data() {
    let dir = test_data_dir();
    let names = pair_files(&dir);
    assert!(!names.is_empty(), "no paired .json/.toon files found under {}", dir.display());
    for name in names {
        check_pair(&dir, &name);
    }
}

// ---------------------------------------------------------------------------
// toon-format/spec's examples/conversions/*.json + *.toon.
// ---------------------------------------------------------------------------

#[test]
fn native_json_roundtrip_spec_examples() {
    let Some(dir) = spec_examples_dir() else {
        eprintln!(
            "CTOON_SPEC_EXAMPLES_DIR not set/found — skipping (only available under CMake/ctest)."
        );
        return;
    };
    let names = pair_files(&dir);
    assert!(!names.is_empty(), "no paired .json/.toon files found under {}", dir.display());
    for name in names {
        check_pair(&dir, &name);
    }
}
