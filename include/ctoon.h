/*==============================================================================
 |  Copyright (c) 2026 CToon Project
 |
 |  Permission is hereby granted, free of charge, to any person obtaining a copy
 |  of this software and associated documentation files (the "Software"), to deal
 |  in the Software without restriction, including without limitation the rights
 |  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 |  copies of the Software, and to permit persons to whom the Software is
 |  furnished to do so, subject to the following conditions:
 |
 |  The above copyright notice and this permission notice shall be included in all
 |  copies or substantial portions of the Software.
 |
 |  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 |  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 |  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 |  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 |  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 |  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 |  SOFTWARE.
 |  *============================================================================*/

/**
 @file    ctoon.h
 @brief   CToon C API – parse and generate TOON-format data.
 @date 2025-11-04
 @author Mohammad Raziei
 */

#ifndef CTOON_H
#define CTOON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================
 * Version
 * ============================================================ */

#define CTOON_VERSION_MAJOR 0
#define CTOON_VERSION_MINOR 1
#define CTOON_VERSION_PATCH 0

#define CTOON_VERSION_ENCODE(maj,min,pat) \
    (((maj)*10000) + ((min)*100) + (pat))
#define CTOON_VERSION \
    CTOON_VERSION_ENCODE(CTOON_VERSION_MAJOR,CTOON_VERSION_MINOR,CTOON_VERSION_PATCH)

#define CTOON_VERSION_XSTR(maj,min,pat) #maj"."#min"."#pat
#define CTOON_VERSION_STR(maj,min,pat)  CTOON_VERSION_XSTR(maj,min,pat)
#define CTOON_VERSION_STRING \
    CTOON_VERSION_STR(CTOON_VERSION_MAJOR,CTOON_VERSION_MINOR,CTOON_VERSION_PATCH)

/* ============================================================
 * Opaque types
 * ============================================================ */

/** Document handle – owns all arena memory for its values. */
typedef struct ctoon_doc ctoon_doc;

/** Value handle – points into document arena; do NOT free individually. */
typedef struct ctoon_val ctoon_val;

/* ============================================================
 * Value type tags
 * ============================================================ */

typedef enum ctoon_type {
    CTOON_TYPE_NULL   = 0,
    CTOON_TYPE_TRUE,
    CTOON_TYPE_FALSE,
    CTOON_TYPE_NUMBER,
    CTOON_TYPE_STRING,
    CTOON_TYPE_ARRAY,
    CTOON_TYPE_OBJECT,
    CTOON_TYPE_RAW
} ctoon_type;

typedef enum ctoon_subtype {
    CTOON_SUBTYPE_NONE = 0,
    CTOON_SUBTYPE_UINT,
    CTOON_SUBTYPE_SINT,
    CTOON_SUBTYPE_REAL
} ctoon_subtype;

/* ============================================================
 * Document lifecycle
 * ============================================================ */

ctoon_doc  *ctoon_doc_new        (size_t max_memory);
void        ctoon_doc_free       (ctoon_doc *doc);
ctoon_val  *ctoon_doc_get_root   (ctoon_doc *doc);
void        ctoon_doc_set_root   (ctoon_doc *doc, ctoon_val *root);
size_t      ctoon_doc_get_val_count(ctoon_doc *doc);
const char *ctoon_doc_get_error  (ctoon_doc *doc);
size_t      ctoon_doc_get_error_pos(ctoon_doc *doc);

/* ============================================================
 * Type queries
 * ============================================================ */

ctoon_type    ctoon_get_type   (ctoon_val *val);
ctoon_subtype ctoon_get_subtype(ctoon_val *val);

bool ctoon_is_null  (ctoon_val *val);
bool ctoon_is_true  (ctoon_val *val);
bool ctoon_is_false (ctoon_val *val);
bool ctoon_is_bool  (ctoon_val *val);
bool ctoon_is_uint  (ctoon_val *val);
bool ctoon_is_sint  (ctoon_val *val);
bool ctoon_is_int   (ctoon_val *val);
bool ctoon_is_real  (ctoon_val *val);
bool ctoon_is_num   (ctoon_val *val);
bool ctoon_is_number(ctoon_val *val);   /* alias for ctoon_is_num    */
bool ctoon_is_str   (ctoon_val *val);
bool ctoon_is_string(ctoon_val *val);   /* alias for ctoon_is_str    */
bool ctoon_is_arr   (ctoon_val *val);
bool ctoon_is_array (ctoon_val *val);   /* alias for ctoon_is_arr    */
bool ctoon_is_obj   (ctoon_val *val);
bool ctoon_is_object(ctoon_val *val);   /* alias for ctoon_is_obj    */
bool ctoon_is_ctn   (ctoon_val *val);   /* array OR object           */
bool ctoon_is_raw   (ctoon_val *val);

/* ============================================================
 * Value accessors
 * ============================================================ */

bool        ctoon_get_bool (ctoon_val *val);
uint64_t    ctoon_get_uint (ctoon_val *val);
int64_t     ctoon_get_sint (ctoon_val *val);
int         ctoon_get_int  (ctoon_val *val);
double      ctoon_get_real (ctoon_val *val);
double      ctoon_get_num  (ctoon_val *val); /* alias for ctoon_get_real */

const char *ctoon_get_str    (ctoon_val *val);
size_t      ctoon_get_len    (ctoon_val *val);
bool        ctoon_equals_str (ctoon_val *val, const char *str);
bool        ctoon_equals_strn(ctoon_val *val, const char *str, size_t len);

const char *ctoon_get_raw(ctoon_val *val);

/* ============================================================
 * Array access
 * ============================================================ */

size_t     ctoon_arr_size     (ctoon_val *arr);
ctoon_val *ctoon_arr_get      (ctoon_val *arr, size_t idx);
ctoon_val *ctoon_arr_get_first(ctoon_val *arr);
/** Stateless sequential iterator – NOT thread-safe. */
ctoon_val *ctoon_arr_get_next (ctoon_val *arr);

/* ============================================================
 * Object access
 * ============================================================ */

size_t     ctoon_obj_size (ctoon_val *obj);
ctoon_val *ctoon_obj_get  (ctoon_val *obj, const char *key);
ctoon_val *ctoon_obj_getn (ctoon_val *obj, const char *key, size_t len);

/** Begin object iteration; sets *key to a string value for the first key. */
ctoon_val *ctoon_obj_iter_get (ctoon_val *obj, ctoon_val **key);
/** Advance object iteration – NOT thread-safe. */
ctoon_val *ctoon_obj_iter_next(ctoon_val *obj, ctoon_val **key);

/* ============================================================
 * Parse / serialise
 * ============================================================ */

ctoon_doc *ctoon_read_toon     (const char *dat, size_t len);
ctoon_doc *ctoon_read_toon_file(const char *path);

/** Returns heap-allocated NUL-terminated string; caller must free(). */
char *ctoon_write_toon     (ctoon_doc *doc, size_t *len);
bool  ctoon_write_toon_file(const char *path, ctoon_doc *doc);

/* ============================================================
 * Value construction
 * ============================================================ */

ctoon_val *ctoon_new_null  (ctoon_doc *doc);
ctoon_val *ctoon_new_bool  (ctoon_doc *doc, bool     value);
ctoon_val *ctoon_new_uint  (ctoon_doc *doc, uint64_t value);
ctoon_val *ctoon_new_sint  (ctoon_doc *doc, int64_t  value);
ctoon_val *ctoon_new_real  (ctoon_doc *doc, double   value);
ctoon_val *ctoon_new_str   (ctoon_doc *doc, const char *value);
ctoon_val *ctoon_new_strn  (ctoon_doc *doc, const char *value, size_t len);
ctoon_val *ctoon_new_array (ctoon_doc *doc);
ctoon_val *ctoon_new_object(ctoon_doc *doc);

bool ctoon_array_append(ctoon_doc *doc, ctoon_val *array, ctoon_val *value);
bool ctoon_object_set  (ctoon_doc *doc, ctoon_val *object, const char *key,   ctoon_val *value);
bool ctoon_object_setn (ctoon_doc *doc, ctoon_val *object,
                        const char *key, size_t key_len, ctoon_val *value);


/* ============================================================
 * Index-based object pair access (no global iterator state)
 * ============================================================ */

/** Get the key (always CTOON_TYPE_STRING) at index idx in object. NULL if OOB. */
ctoon_val *ctoon_obj_get_key_at(ctoon_val *obj, size_t idx);

/** Get the value at index idx in object. NULL if OOB. */
ctoon_val *ctoon_obj_get_val_at(ctoon_val *obj, size_t idx);

#ifdef __cplusplus
}
#endif

#endif /* CTOON_H */
