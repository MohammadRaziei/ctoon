//! Raw, unsafe FFI declarations for ctoon's C API. Not part of the public
//! interface of this crate — see `lib.rs` for the safe wrapper.
//!
//! Most of ctoon's tree-inspection and mutable-document-building API is
//! `static inline` in ctoon.h (meant for code that #includes the header
//! directly), so it produces no externally-linkable symbol in the
//! compiled ctoon.c object. Those functions are declared here under their
//! `ctoon_rs_`-prefixed shim names instead — see shim.c, which provides a
//! thin extern-linkage wrapper for each one. The handful of functions
//! that genuinely are `extern` in ctoon.h (`ctoon_read_opts`,
//! `ctoon_read_json`, `ctoon_mut_doc_new`, `ctoon_mut_doc_free`,
//! `ctoon_mut_write_opts`, `ctoon_write_json_mut`) are declared under
//! their real names, unshimmed.

#![allow(non_camel_case_types)]

use std::os::raw::{c_char, c_double, c_int};

// Opaque types — never constructed on the Rust side, only passed around
// as pointers returned by / passed to the C API.
#[repr(C)]
pub struct ctoon_doc {
    _private: [u8; 0],
}
#[repr(C)]
pub struct ctoon_val {
    _private: [u8; 0],
}
#[repr(C)]
pub struct ctoon_mut_doc {
    _private: [u8; 0],
}
#[repr(C)]
pub struct ctoon_mut_val {
    _private: [u8; 0],
}
#[repr(C)]
pub struct ctoon_alc {
    _private: [u8; 0],
}

pub type ctoon_type = u8;
pub type ctoon_read_flag = u32;
pub type ctoon_write_flag = u32;
pub type ctoon_delimiter = c_int;

pub const CTOON_TYPE_NULL: ctoon_type = 2;
pub const CTOON_TYPE_BOOL: ctoon_type = 3;
pub const CTOON_TYPE_NUM: ctoon_type = 4;
pub const CTOON_TYPE_STR: ctoon_type = 5;
pub const CTOON_TYPE_ARR: ctoon_type = 6;
pub const CTOON_TYPE_OBJ: ctoon_type = 7;

pub const CTOON_READ_NOFLAG: ctoon_read_flag = 0;
pub const CTOON_WRITE_NOFLAG: ctoon_write_flag = 0;

#[repr(C)]
#[derive(Debug)]
pub struct ctoon_read_err {
    pub code: u32,
    pub msg: *const c_char,
    pub pos: usize,
}

#[repr(C)]
#[derive(Debug)]
pub struct ctoon_write_err {
    pub code: u32,
    pub msg: *const c_char,
}

#[repr(C)]
pub struct ctoon_obj_iter {
    pub idx: usize,
    pub max: usize,
    pub cur: *mut ctoon_val,
    pub obj: *mut ctoon_val,
}

extern "C" {
    // ── Genuinely `extern` in ctoon.h — real names, no shim needed ────
    pub fn ctoon_read_opts(
        dat: *mut c_char,
        len: usize,
        flg: ctoon_read_flag,
        alc: *const ctoon_alc,
        err: *mut ctoon_read_err,
    ) -> *mut ctoon_doc;

    pub fn ctoon_read_json(
        dat: *mut c_char,
        len: usize,
        flg: ctoon_read_flag,
        alc: *const ctoon_alc,
        err: *mut ctoon_read_err,
    ) -> *mut ctoon_doc;

    pub fn ctoon_mut_doc_new(alc: *const ctoon_alc) -> *mut ctoon_mut_doc;
    pub fn ctoon_mut_doc_free(doc: *mut ctoon_mut_doc);

    pub fn ctoon_mut_write_opts(
        doc: *const ctoon_mut_doc,
        opts: *const std::os::raw::c_void, // NULL always passed — see lib.rs
        alc: *const ctoon_alc,
        len: *mut usize,
        err: *mut ctoon_write_err,
    ) -> *mut c_char;

    pub fn ctoon_write_json_mut(
        doc: *const ctoon_mut_doc,
        indent: c_int,
        flg: ctoon_write_flag,
        alc: *const ctoon_alc,
        len: *mut usize,
        err: *mut ctoon_write_err,
    ) -> *mut c_char;

    // ── `static inline` in ctoon.h — shimmed via shim.c ───────────────
    pub fn ctoon_rs_doc_get_root(doc: *mut ctoon_doc) -> *mut ctoon_val;
    pub fn ctoon_rs_doc_free(doc: *mut ctoon_doc);

    pub fn ctoon_rs_get_type(val: *mut ctoon_val) -> ctoon_type;
    pub fn ctoon_rs_get_bool(val: *mut ctoon_val) -> bool;
    pub fn ctoon_rs_is_uint(val: *mut ctoon_val) -> bool;
    pub fn ctoon_rs_is_sint(val: *mut ctoon_val) -> bool;
    pub fn ctoon_rs_get_uint(val: *mut ctoon_val) -> u64;
    pub fn ctoon_rs_get_sint(val: *mut ctoon_val) -> i64;
    pub fn ctoon_rs_get_real(val: *mut ctoon_val) -> c_double;
    pub fn ctoon_rs_get_str(val: *mut ctoon_val) -> *const c_char;
    pub fn ctoon_rs_get_len(val: *mut ctoon_val) -> usize;
    pub fn ctoon_rs_get_raw(val: *mut ctoon_val) -> *const c_char;

    pub fn ctoon_rs_arr_size(arr: *mut ctoon_val) -> usize;
    pub fn ctoon_rs_arr_get(arr: *mut ctoon_val, idx: usize) -> *mut ctoon_val;

    pub fn ctoon_rs_obj_size(obj: *mut ctoon_val) -> usize;
    pub fn ctoon_rs_obj_iter_init(obj: *mut ctoon_val, iter: *mut ctoon_obj_iter) -> bool;
    pub fn ctoon_rs_obj_iter_has_next(iter: *const ctoon_obj_iter) -> bool;
    pub fn ctoon_rs_obj_iter_next(iter: *mut ctoon_obj_iter) -> *mut ctoon_val;
    pub fn ctoon_rs_obj_iter_get_val(key_val: *mut ctoon_val) -> *mut ctoon_val;

    pub fn ctoon_rs_mut_doc_set_root(doc: *mut ctoon_mut_doc, root: *mut ctoon_mut_val);
    pub fn ctoon_rs_mut_null(doc: *mut ctoon_mut_doc) -> *mut ctoon_mut_val;
    pub fn ctoon_rs_mut_true(doc: *mut ctoon_mut_doc) -> *mut ctoon_mut_val;
    pub fn ctoon_rs_mut_false(doc: *mut ctoon_mut_doc) -> *mut ctoon_mut_val;
    pub fn ctoon_rs_mut_sint(doc: *mut ctoon_mut_doc, val: i64) -> *mut ctoon_mut_val;
    pub fn ctoon_rs_mut_uint(doc: *mut ctoon_mut_doc, val: u64) -> *mut ctoon_mut_val;
    pub fn ctoon_rs_mut_real(doc: *mut ctoon_mut_doc, val: c_double) -> *mut ctoon_mut_val;
    pub fn ctoon_rs_mut_strncpy(
        doc: *mut ctoon_mut_doc,
        str_: *const c_char,
        len: usize,
    ) -> *mut ctoon_mut_val;
    pub fn ctoon_rs_mut_obj(doc: *mut ctoon_mut_doc) -> *mut ctoon_mut_val;
    pub fn ctoon_rs_mut_arr(doc: *mut ctoon_mut_doc) -> *mut ctoon_mut_val;
    pub fn ctoon_rs_mut_obj_put(
        obj: *mut ctoon_mut_val,
        key: *mut ctoon_mut_val,
        val: *mut ctoon_mut_val,
    ) -> bool;
    pub fn ctoon_rs_mut_arr_append(arr: *mut ctoon_mut_val, val: *mut ctoon_mut_val) -> bool;

    #[link_name = "free"]
    pub fn libc_free(ptr: *mut std::os::raw::c_void);
}
