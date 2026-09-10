//! A minimal, dependency-free value type — no `serde_json` required for
//! the core API, since ctoon does its own JSON parsing/writing in C.
//! `Vec<(String, Value)>` (not a hash map) for objects, to preserve field
//! order exactly as read — TOON documents are usually human-authored, and
//! reordering fields alphabetically (as a `BTreeMap` would) is surprising.

use std::ops::Index;

#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    Null,
    Bool(bool),
    /// A non-negative integer, as read back from TOON/JSON text with no
    /// minus sign — see the crate-level docs for why this is a distinct
    /// variant from `Sint` rather than one signed integer type.
    Uint(u64),
    Sint(i64),
    Real(f64),
    Str(String),
    Array(Vec<Value>),
    Object(Vec<(String, Value)>),
}

impl Value {
    pub fn is_null(&self) -> bool {
        matches!(self, Value::Null)
    }

    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Value::Bool(b) => Some(*b),
            _ => None,
        }
    }

    pub fn as_u64(&self) -> Option<u64> {
        match self {
            Value::Uint(n) => Some(*n),
            Value::Sint(n) if *n >= 0 => Some(*n as u64),
            _ => None,
        }
    }

    pub fn as_i64(&self) -> Option<i64> {
        match self {
            Value::Sint(n) => Some(*n),
            Value::Uint(n) if *n <= i64::MAX as u64 => Some(*n as i64),
            _ => None,
        }
    }

    pub fn as_f64(&self) -> Option<f64> {
        match self {
            Value::Real(n) => Some(*n),
            Value::Uint(n) => Some(*n as f64),
            Value::Sint(n) => Some(*n as f64),
            _ => None,
        }
    }

    pub fn as_str(&self) -> Option<&str> {
        match self {
            Value::Str(s) => Some(s),
            _ => None,
        }
    }

    pub fn as_array(&self) -> Option<&[Value]> {
        match self {
            Value::Array(a) => Some(a),
            _ => None,
        }
    }

    pub fn as_object(&self) -> Option<&[(String, Value)]> {
        match self {
            Value::Object(o) => Some(o),
            _ => None,
        }
    }

    /// Looks up a key in an object. Returns `None` for a non-object value
    /// or a missing key — same shape as `serde_json::Value::get`.
    pub fn get(&self, key: &str) -> Option<&Value> {
        match self {
            Value::Object(o) => o.iter().find(|(k, _)| k == key).map(|(_, v)| v),
            _ => None,
        }
    }
}

/// `value["key"]` — returns `&Value::Null` for a missing key or a
/// non-object value, matching `serde_json::Value`'s indexing behaviour
/// (never panics).
impl Index<&str> for Value {
    type Output = Value;
    fn index(&self, key: &str) -> &Value {
        const NULL: Value = Value::Null;
        self.get(key).unwrap_or(&NULL)
    }
}

/// `value[i]` — returns `&Value::Null` for an out-of-range index or a
/// non-array value.
impl Index<usize> for Value {
    type Output = Value;
    fn index(&self, idx: usize) -> &Value {
        const NULL: Value = Value::Null;
        match self {
            Value::Array(a) => a.get(idx).unwrap_or(&NULL),
            _ => &NULL,
        }
    }
}

impl From<bool> for Value {
    fn from(b: bool) -> Self {
        Value::Bool(b)
    }
}
impl From<u64> for Value {
    fn from(n: u64) -> Self {
        Value::Uint(n)
    }
}
impl From<i64> for Value {
    fn from(n: i64) -> Self {
        Value::Sint(n)
    }
}
impl From<i32> for Value {
    fn from(n: i32) -> Self {
        Value::Sint(n as i64)
    }
}
impl From<f64> for Value {
    fn from(n: f64) -> Self {
        Value::Real(n)
    }
}
impl From<String> for Value {
    fn from(s: String) -> Self {
        Value::Str(s)
    }
}
impl From<&str> for Value {
    fn from(s: &str) -> Self {
        Value::Str(s.to_string())
    }
}
impl<T: Into<Value>> From<Vec<T>> for Value {
    fn from(v: Vec<T>) -> Self {
        Value::Array(v.into_iter().map(Into::into).collect())
    }
}
impl<T: Into<Value>> From<Option<T>> for Value {
    fn from(v: Option<T>) -> Self {
        match v {
            Some(v) => v.into(),
            None => Value::Null,
        }
    }
}
