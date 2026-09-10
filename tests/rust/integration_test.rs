//! Integration tests for the ctoon Rust binding — public API only (the
//! crate's own unit tests, in src/lib.rs, additionally cover internals).
//! Lives here rather than in the usual Cargo-native `<crate>/tests/`
//! location so every language's tests stay under one root `tests/`
//! folder — see Cargo.toml's `[[test]] path = "../../../tests/rust/..."`.

use ctoon::Value;

#[test]
fn loads_basic_object() {
    let v = ctoon::loads("name: Alice\nage: 30").unwrap();
    assert_eq!(v["name"].as_str(), Some("Alice"));
    assert_eq!(v["age"].as_u64(), Some(30));
}

#[test]
fn dumps_basic_object() {
    let v = Value::Object(vec![
        ("name".into(), "Alice".into()),
        ("age".into(), Value::Uint(30)),
    ]);
    let toon = ctoon::dumps(&v).unwrap();
    assert_eq!(toon, "name: Alice\nage: 30");
}

#[test]
fn roundtrip_array() {
    let v = Value::Array(vec![Value::Uint(1), Value::Uint(2), Value::Uint(3)]);
    let toon = ctoon::dumps(&v).unwrap();
    let back = ctoon::loads(&toon).unwrap();
    assert_eq!(v, back);
}

#[test]
fn roundtrip_nested() {
    let v = Value::Object(vec![(
        "order".into(),
        Value::Object(vec![
            ("id".into(), "ORD-1".into()),
            (
                "items".into(),
                Value::Array(vec![
                    Value::Object(vec![("sku".into(), "A".into()), ("qty".into(), Value::Uint(2))]),
                    Value::Object(vec![("sku".into(), "B".into()), ("qty".into(), Value::Uint(1))]),
                ]),
            ),
        ]),
    )]);
    let toon = ctoon::dumps(&v).unwrap();
    let back = ctoon::loads(&toon).unwrap();
    assert_eq!(v, back);
}

#[test]
fn json_to_toon_to_json() {
    let json_in = r#"{"x":1,"y":[1,2,3],"z":{"deep":true}}"#;
    let value = ctoon::loads_json(json_in).unwrap();
    let toon = ctoon::dumps(&value).unwrap();
    let back = ctoon::loads(&toon).unwrap();
    let json_out = ctoon::dumps_json(&back, 0).unwrap();
    let reparsed: Value = ctoon::loads_json(&json_out).unwrap();
    assert_eq!(value, reparsed);
}

#[test]
fn null_and_bool() {
    let v = Value::Object(vec![
        ("a".into(), Value::Null),
        ("b".into(), Value::Bool(true)),
        ("c".into(), Value::Bool(false)),
    ]);
    let toon = ctoon::dumps(&v).unwrap();
    let back = ctoon::loads(&toon).unwrap();
    assert_eq!(v, back);
}

#[test]
fn negative_and_float() {
    let v = Value::Object(vec![
        ("neg".into(), Value::Sint(-42)),
        ("pi".into(), Value::Real(3.14159)),
    ]);
    let toon = ctoon::dumps(&v).unwrap();
    let back = ctoon::loads(&toon).unwrap();
    assert_eq!(back["neg"].as_i64(), Some(-42));
    assert!((back["pi"].as_f64().unwrap() - 3.14159).abs() < 1e-9);
}

#[test]
fn unicode_string() {
    let v = Value::Object(vec![("emoji".into(), "🎉日本語".into())]);
    let toon = ctoon::dumps(&v).unwrap();
    let back = ctoon::loads(&toon).unwrap();
    assert_eq!(v, back);
}

#[test]
fn escaped_key_round_trip() {
    // Regression coverage for the keyed-tabular escaped-key bug fixed in
    // the C core (src/ctoon.c) — the same case tests/python and
    // tests/c/tests/cpp cover, exercised here through the Rust binding.
    let v = Value::Object(vec![(
        "properties".into(),
        Value::Object(vec![
            ("foo\nbar".into(), Value::Object(vec![("type".into(), "number".into())])),
            ("foo\"bar".into(), Value::Object(vec![("type".into(), "number".into())])),
        ]),
    )]);
    let toon = ctoon::dumps(&v).unwrap();
    let back = ctoon::loads(&toon).unwrap();
    assert_eq!(v, back);
}

#[test]
fn parse_error_is_reported() {
    let err = ctoon::loads("\"unterminated").unwrap_err();
    assert!(!err.to_string().is_empty());
}

#[test]
fn indexing_missing_key_returns_null() {
    let v = Value::Object(vec![("a".into(), Value::Uint(1))]);
    assert_eq!(v["missing"], Value::Null);
}
