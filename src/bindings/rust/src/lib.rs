//! Rust bindings for [ctoon](https://github.com/mohammadraziei/ctoon) — a
//! high-performance, zero-dependency C99 library for the
//! [TOON](https://github.com/toon-format/spec) serialization format, with
//! built-in JSON interop.
//!
//! Values are represented as [`Value`] — a small, dependency-free enum
//! (see `value.rs`), not a `serde` type. This mirrors the Python binding's
//! own approach: a `dict`/`list`-shaped dynamic value, not a
//! derive-macro/struct-mapping layer. JSON parsing and writing is done by
//! ctoon's own C implementation ([`loads_json`]/[`dumps_json`]) — this
//! crate has no JSON (or any other) dependency at all.
//!
//! ```
//! use ctoon::Value;
//!
//! let mut fields = Vec::new();
//! fields.push(("name".to_string(), Value::from("Alice")));
//! fields.push(("age".to_string(), Value::from(30i64)));
//! let doc = Value::Object(fields);
//!
//! let toon = ctoon::dumps(&doc).unwrap();
//! assert_eq!(toon, "name: Alice\nage: 30");
//!
//! let back = ctoon::loads(&toon).unwrap();
//! assert_eq!(back["name"].as_str(), Some("Alice"));
//! ```

mod error;
mod ffi;
mod value;

pub use error::Error;
pub use value::Value;

use ffi::*;
use std::ffi::CStr;
use std::os::raw::c_char;

fn read_err_to_error(err: &ctoon_read_err) -> Error {
    let message = if err.msg.is_null() {
        "parse error".to_string()
    } else {
        unsafe { CStr::from_ptr(err.msg) }.to_string_lossy().into_owned()
    };
    Error::Parse { message, pos: err.pos, code: err.code }
}

fn write_err_to_error(err: &ctoon_write_err) -> Error {
    let message = if err.msg.is_null() {
        "write error".to_string()
    } else {
        unsafe { CStr::from_ptr(err.msg) }.to_string_lossy().into_owned()
    };
    Error::Write { message, code: err.code }
}

/// Walks a `ctoon_val` tree into a [`Value`]. Safety: `val` must be
/// either null or a valid pointer returned by ctoon (e.g. from
/// `ctoon_doc_get_root`, `ctoon_arr_get`, `ctoon_obj_iter_get_val`), and
/// the document it belongs to must still be alive.
unsafe fn val_to_value(val: *mut ctoon_val) -> Value {
    if val.is_null() {
        return Value::Null;
    }
    match ctoon_rs_get_type(val) {
        CTOON_TYPE_NULL => Value::Null,
        CTOON_TYPE_BOOL => Value::Bool(ctoon_rs_get_bool(val)),
        CTOON_TYPE_NUM => {
            if ctoon_rs_is_uint(val) {
                Value::Uint(ctoon_rs_get_uint(val))
            } else if ctoon_rs_is_sint(val) {
                Value::Sint(ctoon_rs_get_sint(val))
            } else {
                Value::Real(ctoon_rs_get_real(val))
            }
        }
        CTOON_TYPE_STR => Value::Str(get_str(val)),
        CTOON_TYPE_ARR => {
            let n = ctoon_rs_arr_size(val);
            let mut arr = Vec::with_capacity(n);
            for i in 0..n {
                arr.push(val_to_value(ctoon_rs_arr_get(val, i)));
            }
            Value::Array(arr)
        }
        CTOON_TYPE_OBJ => {
            let n = ctoon_rs_obj_size(val);
            let mut obj = Vec::with_capacity(n);
            let mut iter: ctoon_obj_iter = std::mem::zeroed();
            ctoon_rs_obj_iter_init(val, &mut iter);
            while ctoon_rs_obj_iter_has_next(&iter) {
                let key_val = ctoon_rs_obj_iter_next(&mut iter);
                if key_val.is_null() {
                    break;
                }
                let v = ctoon_rs_obj_iter_get_val(key_val);
                obj.push((get_str(key_val), val_to_value(v)));
            }
            Value::Object(obj)
        }
        _ => {
            // raw/unknown value kind — best-effort string, matches the
            // fallback the C API's other consumers (e.g. the Go binding) use.
            let raw = ctoon_rs_get_raw(val);
            if raw.is_null() {
                Value::Null
            } else {
                Value::Str(CStr::from_ptr(raw).to_string_lossy().into_owned())
            }
        }
    }
}

/// Safety: `val` must be a valid, non-null pointer to a `CTOON_TYPE_STR`
/// value whose underlying document is still alive.
unsafe fn get_str(val: *mut ctoon_val) -> String {
    let ptr = ctoon_rs_get_str(val);
    let len = ctoon_rs_get_len(val);
    if ptr.is_null() || len == 0 {
        return String::new();
    }
    let bytes = std::slice::from_raw_parts(ptr as *const u8, len);
    String::from_utf8_lossy(bytes).into_owned()
}

/// Walks a [`Value`] into a `ctoon_mut_val` tree. Safety: `doc` must be a
/// valid, non-null `ctoon_mut_doc` the caller owns.
unsafe fn value_to_mut(doc: *mut ctoon_mut_doc, v: &Value) -> Result<*mut ctoon_mut_val, Error> {
    let ptr = match v {
        Value::Null => ctoon_rs_mut_null(doc),
        Value::Bool(b) => {
            if *b {
                ctoon_rs_mut_true(doc)
            } else {
                ctoon_rs_mut_false(doc)
            }
        }
        Value::Uint(n) => ctoon_rs_mut_uint(doc, *n),
        Value::Sint(n) => ctoon_rs_mut_sint(doc, *n),
        Value::Real(n) => ctoon_rs_mut_real(doc, *n),
        // &str's data pointer is guaranteed non-null even for an empty
        // string (Rust's own ABI guarantee) — ctoon_mut_strncpy requires a
        // non-null `str` regardless of `len`, so this is safe as-is with
        // no empty-string special case needed.
        Value::Str(s) => ctoon_rs_mut_strncpy(doc, s.as_ptr() as *const c_char, s.len()),
        Value::Array(arr) => {
            let a = ctoon_rs_mut_arr(doc);
            if a.is_null() {
                return Err(Error::UnsupportedValue("failed to create array".into()));
            }
            for elem in arr {
                let child = value_to_mut(doc, elem)?;
                if !ctoon_rs_mut_arr_append(a, child) {
                    return Err(Error::UnsupportedValue("failed to append array element".into()));
                }
            }
            a
        }
        Value::Object(fields) => {
            let o = ctoon_rs_mut_obj(doc);
            if o.is_null() {
                return Err(Error::UnsupportedValue("failed to create object".into()));
            }
            for (k, val) in fields {
                let key = ctoon_rs_mut_strncpy(doc, k.as_ptr() as *const c_char, k.len());
                if key.is_null() {
                    return Err(Error::UnsupportedValue(format!("failed to create key {:?}", k)));
                }
                let child = value_to_mut(doc, val)?;
                if !ctoon_rs_mut_obj_put(o, key, child) {
                    return Err(Error::UnsupportedValue(format!("failed to set key {:?}", k)));
                }
            }
            o
        }
    };
    if ptr.is_null() {
        Err(Error::UnsupportedValue(format!("failed to create value for {:?}", v)))
    } else {
        Ok(ptr)
    }
}

/// Parses a TOON string into a [`Value`].
pub fn loads(s: &str) -> Result<Value, Error> {
    // Always pass an owned, mutable copy — ctoon_read_opts takes a
    // non-const `char *` and, while it does not modify the buffer unless
    // the (never-set, here) in-situ flag is passed, that's an internal
    // behaviour we don't want to depend on for soundness across a public
    // API surface.
    let mut buf = s.as_bytes().to_vec();
    let mut err: ctoon_read_err = unsafe { std::mem::zeroed() };
    let doc = unsafe {
        ctoon_read_opts(
            buf.as_mut_ptr() as *mut c_char,
            buf.len(),
            CTOON_READ_NOFLAG,
            std::ptr::null(),
            &mut err,
        )
    };
    if doc.is_null() {
        return Err(read_err_to_error(&err));
    }
    let value = unsafe {
        let root = ctoon_rs_doc_get_root(doc);
        let v = val_to_value(root);
        ctoon_rs_doc_free(doc);
        v
    };
    Ok(value)
}

/// Serialises a [`Value`] to a TOON-formatted string.
pub fn dumps(v: &Value) -> Result<String, Error> {
    unsafe {
        let doc = ctoon_mut_doc_new(std::ptr::null());
        if doc.is_null() {
            return Err(Error::Write { message: "failed to create mutable document".into(), code: 0 });
        }
        let root = match value_to_mut(doc, v) {
            Ok(r) => r,
            Err(e) => {
                ctoon_mut_doc_free(doc);
                return Err(e);
            }
        };
        ctoon_rs_mut_doc_set_root(doc, root);

        let mut len: usize = 0;
        let mut err: ctoon_write_err = std::mem::zeroed();
        let raw = ctoon_mut_write_opts(doc, std::ptr::null(), std::ptr::null(), &mut len, &mut err);
        ctoon_mut_doc_free(doc);

        if raw.is_null() {
            return Err(write_err_to_error(&err));
        }
        let bytes = std::slice::from_raw_parts(raw as *const u8, len).to_vec();
        libc_free(raw as *mut std::os::raw::c_void);
        Ok(String::from_utf8_lossy(&bytes).into_owned())
    }
}

/// Parses a JSON string into a [`Value`] (using ctoon's own JSON reader —
/// this crate has no `serde_json` or other JSON-parsing dependency).
pub fn loads_json(s: &str) -> Result<Value, Error> {
    let mut buf = s.as_bytes().to_vec();
    let mut err: ctoon_read_err = unsafe { std::mem::zeroed() };
    let doc = unsafe {
        ctoon_read_json(
            buf.as_mut_ptr() as *mut c_char,
            buf.len(),
            CTOON_READ_NOFLAG,
            std::ptr::null(),
            &mut err,
        )
    };
    if doc.is_null() {
        return Err(read_err_to_error(&err));
    }
    let value = unsafe {
        let root = ctoon_rs_doc_get_root(doc);
        let v = val_to_value(root);
        ctoon_rs_doc_free(doc);
        v
    };
    Ok(value)
}

/// Serialises a [`Value`] to a JSON string using ctoon's own JSON writer.
pub fn dumps_json(v: &Value, indent: i32) -> Result<String, Error> {
    unsafe {
        let doc = ctoon_mut_doc_new(std::ptr::null());
        if doc.is_null() {
            return Err(Error::Write { message: "failed to create mutable document".into(), code: 0 });
        }
        let root = match value_to_mut(doc, v) {
            Ok(r) => r,
            Err(e) => {
                ctoon_mut_doc_free(doc);
                return Err(e);
            }
        };
        ctoon_rs_mut_doc_set_root(doc, root);

        let mut len: usize = 0;
        let mut err: ctoon_write_err = std::mem::zeroed();
        let raw = ctoon_write_json_mut(doc, indent, CTOON_WRITE_NOFLAG, std::ptr::null(), &mut len, &mut err);
        ctoon_mut_doc_free(doc);

        if raw.is_null() {
            return Err(write_err_to_error(&err));
        }
        let bytes = std::slice::from_raw_parts(raw as *const u8, len).to_vec();
        libc_free(raw as *mut std::os::raw::c_void);
        Ok(String::from_utf8_lossy(&bytes).into_owned())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip_basic() {
        // Non-negative integer literals read back as Uint regardless of
        // what variant they were written with — TOON text has no type
        // tag distinguishing signed/unsigned, so the reader classifies
        // any non-negative number as unsigned by convention. Same
        // behaviour as every other ctoon binding (see the top-level
        // README/CHANGELOG for this exact point re: the Go binding).
        let v = Value::Object(vec![
            ("name".into(), "Alice".into()),
            ("age".into(), Value::Sint(30)),
            ("tags".into(), Value::Array(vec!["a".into(), "b".into()])),
        ]);
        let expected = Value::Object(vec![
            ("name".into(), "Alice".into()),
            ("age".into(), Value::Uint(30)),
            ("tags".into(), Value::Array(vec!["a".into(), "b".into()])),
        ]);
        let toon = dumps(&v).unwrap();
        let back = loads(&toon).unwrap();
        assert_eq!(back, expected);
    }

    #[test]
    fn json_interop() {
        let v = Value::Object(vec![
            ("x".into(), Value::Sint(1)),
            ("y".into(), Value::Array(vec![Value::Sint(1), Value::Sint(2), Value::Sint(3)])),
        ]);
        let expected = Value::Object(vec![
            ("x".into(), Value::Uint(1)),
            ("y".into(), Value::Array(vec![Value::Uint(1), Value::Uint(2), Value::Uint(3)])),
        ]);
        let j = dumps_json(&v, 2).unwrap();
        let back = loads_json(&j).unwrap();
        assert_eq!(back, expected);
    }

    #[test]
    fn empty_string_value() {
        let v = Value::Object(vec![("".into(), "".into()), ("k".into(), "".into())]);
        let toon = dumps(&v).unwrap();
        let back = loads(&toon).unwrap();
        assert_eq!(v, back);
    }

    #[test]
    fn parse_error_has_message() {
        // Unterminated quoted string — verified to genuinely fail to
        // parse (confirmed via manual testing during development, which
        // is also how a real struct-layout bug in ctoon_read_err's field
        // order got caught: it segfaulted here instead of erroring).
        let err = loads("\"unterminated").unwrap_err();
        assert!(!err.to_string().is_empty());
    }

    #[test]
    fn indexing() {
        let v = Value::Object(vec![("name".into(), "Bob".into())]);
        assert_eq!(v["name"].as_str(), Some("Bob"));
        assert_eq!(v["missing"], Value::Null);
    }
}
