//! ctoon::json_to_toon / toon_to_json (+ _file): direct text -> text
//! conversion. Expected values come only from independent sources:
//! hand-written strings, paired .toon fixtures, and serde_json (never the
//! ctoon CLI).

use std::fs;
use std::path::{Path, PathBuf};

fn data_dir() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/data")
}

fn spec_examples_dir() -> Option<PathBuf> {
    let p = PathBuf::from(std::env::var("CTOON_SPEC_EXAMPLES_DIR").ok()?);
    if p.is_dir() { Some(p) } else { None }
}

#[test]
fn json_to_toon_basic() {
    assert_eq!(
        ctoon::json_to_toon(r#"{"name":"Alice","age":30,"tags":["a","b"]}"#).unwrap(),
        "name: Alice\nage: 30\ntags[2]: a,b"
    );
    assert_eq!(
        ctoon::json_to_toon(r#"{"u":[{"id":1,"n":"x"},{"id":2,"n":"y"}]}"#).unwrap(),
        "u[2]{id,n}:\n  1,x\n  2,y"
    );
    let mut o = ctoon::WriteOptions::default();
    o.delimiter = ctoon::Delimiter::Pipe;
    assert_eq!(ctoon::json_to_toon_opts(r#"{"t":[1,2,3]}"#, &o).unwrap(), "t[3|]: 1|2|3");
}

#[test]
fn toon_to_json_basic() {
    assert_eq!(
        ctoon::toon_to_json("name: Alice\nage: 30\ntags[2]: a,b").unwrap(),
        r#"{"name":"Alice","age":30,"tags":["a","b"]}"#
    );
    assert_eq!(
        ctoon::toon_to_json_opts("a: 1", 2, ctoon::flags::NOFLAG).unwrap(),
        "{\n  \"a\": 1\n}"
    );
}

#[test]
fn errors_are_reported() {
    assert!(ctoon::json_to_toon(r#"{"a":"#).is_err());
    assert!(ctoon::json_to_toon("").is_err());
    assert!(ctoon::toon_to_json("a[2]: 1").is_err());
}

#[test]
fn file_conversion() {
    let dir = std::env::temp_dir().join(format!("ctoon_rs_conv_{}", std::process::id()));
    fs::create_dir_all(&dir).unwrap();
    let (j, t, b) = (dir.join("in.json"), dir.join("out.toon"), dir.join("back.json"));
    fs::write(&j, r#"{"k":[1,2],"s":"hi"}"#).unwrap();
    ctoon::json_to_toon_file(j.to_str().unwrap(), t.to_str().unwrap()).unwrap();
    assert_eq!(fs::read_to_string(&t).unwrap(), "k[2]: 1,2\ns: hi");
    ctoon::toon_to_json_file(t.to_str().unwrap(), b.to_str().unwrap()).unwrap();
    assert_eq!(fs::read_to_string(&b).unwrap(), r#"{"k":[1,2],"s":"hi"}"#);

    let bad = dir.join("bad.json");
    fs::write(&bad, r#"{"a":"#).unwrap();
    let never = dir.join("never.toon");
    assert!(ctoon::json_to_toon_file(bad.to_str().unwrap(), never.to_str().unwrap()).is_err());
    assert!(!never.exists(), "failed conversion must not create the output file");
    let _ = fs::remove_dir_all(&dir);
}

fn check_pair(json_path: &Path) {
    let toon_path = json_path.with_extension("toon");
    let (Ok(raw_json), Ok(raw_toon)) = (fs::read_to_string(json_path), fs::read_to_string(&toon_path)) else {
        return;
    };
    let want = raw_toon.trim_end_matches('\n');
    let name = json_path.file_name().unwrap().to_string_lossy();
    let got = ctoon::json_to_toon(&raw_json).unwrap();
    assert_eq!(got.trim_end_matches('\n'), want, "{name}: json_to_toon != paired .toon");

    let back = ctoon::toon_to_json(want).unwrap();
    let a: serde_json::Value = serde_json::from_str(&raw_json).unwrap();
    let b: serde_json::Value = serde_json::from_str(&back).unwrap();
    assert!(same(&a, &b), "{name}: toon_to_json differs from source JSON");
}

/// Structural equality that treats numbers by value: TOON canonicalises
/// `15.0` to `15` (spec number normalisation), serde_json keeps them apart.
fn same(a: &serde_json::Value, b: &serde_json::Value) -> bool {
    use serde_json::Value::*;
    match (a, b) {
        (Number(x), Number(y)) => x == y || x.as_f64() == y.as_f64(),
        (Array(x), Array(y)) => x.len() == y.len() && x.iter().zip(y).all(|(p, q)| same(p, q)),
        (Object(x), Object(y)) => {
            x.len() == y.len() && x.iter().zip(y.iter()).all(|((k1, v1), (k2, v2))| k1 == k2 && same(v1, v2))
        }
        _ => a == b,
    }
}

fn check_dir(dir: &Path) -> usize {
    let mut n = 0;
    let mut files: Vec<_> = fs::read_dir(dir).unwrap().flatten().map(|e| e.path()).collect();
    files.sort();
    for f in files {
        if f.extension().map_or(false, |e| e == "json") {
            check_pair(&f);
            n += 1;
        }
    }
    n
}

#[test]
fn local_corpus() {
    assert!(check_dir(&data_dir()) > 0);
}

#[test]
fn spec_examples_corpus() {
    let Some(dir) = spec_examples_dir() else {
        eprintln!("CTOON_SPEC_EXAMPLES_DIR not set; skipping");
        return;
    };
    assert!(check_dir(&dir) > 0);
}
