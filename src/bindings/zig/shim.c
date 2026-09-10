/*
 * Most of ctoon's tree-inspection and mutable-document-building API is
 * declared `ctoon_api_inline` (= `static inline`) in ctoon.h — meant to be
 * used by C/C++ code that #includes the header directly, where each
 * translation unit gets its own internal-linkage copy. That's exactly how
 * bridge.c (the Go binding's C shim) uses it.
 *
 * A compiled-and-linked FFI binding is a different situation: a caller
 * that declares these as `extern` (Rust's `extern "C"`, Zig's
 * `extern fn` via `@cImport`) needs a *real*, externally-linkable symbol
 * to link against, and `static inline` functions don't produce one in
 * the compiled ctoon.c object. This file's only job is to give each of
 * those functions a thin, non-static wrapper with real (extern) linkage,
 * so they can be called across the FFI boundary — the wrappers
 * themselves do nothing but forward the call.
 *
 * Shared verbatim by the Rust binding (src/bindings/rust/shim.c) and the
 * Zig binding (this copy) — same symbols, same `ctoon_rs_` prefix, kept
 * for both so ffi.rs's and ctoon.zig's extern declarations don't diverge
 * from what's actually exported.
 */

#define CTOON_ENABLE_JSON 1
#include "ctoon.h"

ctoon_val *ctoon_rs_doc_get_root(ctoon_doc *doc) { return ctoon_doc_get_root(doc); }
void ctoon_rs_doc_free(ctoon_doc *doc) { ctoon_doc_free(doc); }
char *ctoon_rs_write(const ctoon_doc *doc, size_t *len) { return ctoon_write(doc, len); }

ctoon_type ctoon_rs_get_type(ctoon_val *val) { return ctoon_get_type(val); }
bool ctoon_rs_get_bool(ctoon_val *val) { return ctoon_get_bool(val); }
bool ctoon_rs_is_uint(ctoon_val *val) { return ctoon_is_uint(val); }
bool ctoon_rs_is_sint(ctoon_val *val) { return ctoon_is_sint(val); }
uint64_t ctoon_rs_get_uint(ctoon_val *val) { return ctoon_get_uint(val); }
int64_t ctoon_rs_get_sint(ctoon_val *val) { return ctoon_get_sint(val); }
double ctoon_rs_get_real(ctoon_val *val) { return ctoon_get_real(val); }
const char *ctoon_rs_get_str(ctoon_val *val) { return ctoon_get_str(val); }
size_t ctoon_rs_get_len(ctoon_val *val) { return ctoon_get_len(val); }
const char *ctoon_rs_get_raw(ctoon_val *val) { return ctoon_get_raw(val); }

size_t ctoon_rs_arr_size(ctoon_val *arr) { return ctoon_arr_size(arr); }
ctoon_val *ctoon_rs_arr_get(ctoon_val *arr, size_t idx) { return ctoon_arr_get(arr, idx); }

size_t ctoon_rs_obj_size(ctoon_val *obj) { return ctoon_obj_size(obj); }
bool ctoon_rs_obj_iter_init(ctoon_val *obj, ctoon_obj_iter *iter) { return ctoon_obj_iter_init(obj, iter); }
bool ctoon_rs_obj_iter_has_next(const ctoon_obj_iter *iter) { return ctoon_obj_iter_has_next(iter); }
ctoon_val *ctoon_rs_obj_iter_next(ctoon_obj_iter *iter) { return ctoon_obj_iter_next(iter); }
ctoon_val *ctoon_rs_obj_iter_get_val(ctoon_val *key_val) { return ctoon_obj_iter_get_val(key_val); }

void ctoon_rs_mut_doc_set_root(ctoon_mut_doc *doc, ctoon_mut_val *root) { ctoon_mut_doc_set_root(doc, root); }
ctoon_mut_val *ctoon_rs_mut_null(ctoon_mut_doc *doc) { return ctoon_mut_null(doc); }
ctoon_mut_val *ctoon_rs_mut_true(ctoon_mut_doc *doc) { return ctoon_mut_true(doc); }
ctoon_mut_val *ctoon_rs_mut_false(ctoon_mut_doc *doc) { return ctoon_mut_false(doc); }
ctoon_mut_val *ctoon_rs_mut_sint(ctoon_mut_doc *doc, int64_t val) { return ctoon_mut_sint(doc, val); }
ctoon_mut_val *ctoon_rs_mut_uint(ctoon_mut_doc *doc, uint64_t val) { return ctoon_mut_uint(doc, val); }
ctoon_mut_val *ctoon_rs_mut_real(ctoon_mut_doc *doc, double val) { return ctoon_mut_real(doc, val); }
ctoon_mut_val *ctoon_rs_mut_strncpy(ctoon_mut_doc *doc, const char *str, size_t len) {
    return ctoon_mut_strncpy(doc, str, len);
}
ctoon_mut_val *ctoon_rs_mut_obj(ctoon_mut_doc *doc) { return ctoon_mut_obj(doc); }
ctoon_mut_val *ctoon_rs_mut_arr(ctoon_mut_doc *doc) { return ctoon_mut_arr(doc); }
bool ctoon_rs_mut_obj_put(ctoon_mut_val *obj, ctoon_mut_val *key, ctoon_mut_val *val) {
    return ctoon_mut_obj_put(obj, key, val);
}
bool ctoon_rs_mut_arr_append(ctoon_mut_val *arr, ctoon_mut_val *val) {
    return ctoon_mut_arr_append(arr, val);
}
