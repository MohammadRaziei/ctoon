/*==============================================================================
 Copyright (c) 2026 CToon Project

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 *============================================================================*/

/**
 @file ctoon.h
 @date 2024-01-01
 @author CToon Project
 */

#ifndef CTOON_H
#define CTOON_H

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * MARK: - Header Files
 *============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <float.h>
#include <stdint.h>
#include <stdbool.h>



/*==============================================================================
 * MARK: - Version
 *============================================================================*/

#define CTOON_VERSION_MAJOR 0
#define CTOON_VERSION_MINOR 1
#define CTOON_VERSION_PATCH 0

#define CTOON_VERSION_ENCODE(major, minor, patch) \
    (((major) * 10000) + ((minor) * 100) + (patch))

#define CTOON_VERSION \
    CTOON_VERSION_ENCODE(CTOON_VERSION_MAJOR, CTOON_VERSION_MINOR, CTOON_VERSION_PATCH)



/*==============================================================================
 * MARK: - Types
 *============================================================================*/

typedef struct ctoon_doc ctoon_doc;
typedef struct ctoon_val ctoon_val;
typedef struct ctoon_mut_val ctoon_mut_val;
typedef struct ctoon_alc ctoon_alc;
typedef struct ctoon_err ctoon_err;

typedef enum ctoon_type {
    CTOON_TYPE_NULL = 0,
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



/*==============================================================================
 * MARK: - JSON Read/Write Flags
 *============================================================================*/

typedef uint32_t ctoon_read_flag;
typedef uint32_t ctoon_write_flag;

#define CTOON_READ_NOFLAG                    0
#define CTOON_READ_INSITU                    (1 << 0)
#define CTOON_READ_STOP_WHEN_DONE            (1 << 1)
#define CTOON_READ_ALLOW_TRAILING_COMMAS     (1 << 2)
#define CTOON_READ_ALLOW_COMMENTS            (1 << 3)
#define CTOON_READ_ALLOW_INF_AND_NAN         (1 << 4)
#define CTOON_READ_NUMBER_AS_RAW             (1 << 5)
#define CTOON_READ_ALLOW_INVALID_UNICODE     (1 << 6)
#define CTOON_READ_BIGNUM_AS_RAW             (1 << 7)

#define CTOON_WRITE_NOFLAG                   0
#define CTOON_WRITE_PRETTY                   (1 << 0)
#define CTOON_WRITE_ESCAPE_UNICODE           (1 << 1)
#define CTOON_WRITE_ESCAPE_SLASHES           (1 << 2)
#define CTOON_WRITE_ALLOW_INF_AND_NAN        (1 << 3)
#define CTOON_WRITE_INF_AND_NAN_AS_NULL      (1 << 4)
#define CTOON_WRITE_ALLOW_INVALID_UNICODE    (1 << 5)



/*==============================================================================
 * MARK: - TOON Read/Write Flags
 *============================================================================*/

typedef uint32_t ctoon_toon_read_flag;
typedef uint32_t ctoon_toon_write_flag;

#define CTOON_TOON_READ_NOFLAG               0
#define CTOON_TOON_READ_INSITU               (1 << 0)
#define CTOON_TOON_READ_ALLOW_COMMENTS       (1 << 1)
#define CTOON_TOON_READ_STRICT               (1 << 2)

#define CTOON_TOON_WRITE_NOFLAG              0
#define CTOON_TOON_WRITE_PRETTY              (1 << 0)
#define CTOON_TOON_WRITE_COMPACT             (1 << 1)



/*==============================================================================
 * MARK: - Memory Allocation
 *============================================================================*/

ctoon_doc *ctoon_doc_new(const char *dat, size_t len, size_t max_memory);
void ctoon_doc_free(ctoon_doc *doc);

ctoon_val *ctoon_doc_get_root(ctoon_doc *doc);
size_t ctoon_doc_get_read_size(ctoon_doc *doc);
size_t ctoon_doc_get_val_count(ctoon_doc *doc);



/*==============================================================================
 * MARK: - Value Type
 *============================================================================*/

ctoon_type ctoon_get_type(ctoon_val *val);
ctoon_subtype ctoon_get_subtype(ctoon_val *val);
ctoon_type ctoon_get_tag(ctoon_val *val);

bool ctoon_is_null(ctoon_val *val);
bool ctoon_is_true(ctoon_val *val);
bool ctoon_is_false(ctoon_val *val);
bool ctoon_is_bool(ctoon_val *val);
bool ctoon_is_uint(ctoon_val *val);
bool ctoon_is_sint(ctoon_val *val);
bool ctoon_is_int(ctoon_val *val);
bool ctoon_is_real(ctoon_val *val);
bool ctoon_is_num(ctoon_val *val);
bool ctoon_is_str(ctoon_val *val);
bool ctoon_is_arr(ctoon_val *val);
bool ctoon_is_obj(ctoon_val *val);
bool ctoon_is_ctn(ctoon_val *val);
bool ctoon_is_raw(ctoon_val *val);



/*==============================================================================
 * MARK: - Value Content
 *============================================================================*/

bool ctoon_get_bool(ctoon_val *val);
uint64_t ctoon_get_uint(ctoon_val *val);
int64_t ctoon_get_sint(ctoon_val *val);
int ctoon_get_int(ctoon_val *val);
double ctoon_get_real(ctoon_val *val);
double ctoon_get_num(ctoon_val *val);

const char *ctoon_get_str(ctoon_val *val);
size_t ctoon_get_len(ctoon_val *val);
bool ctoon_equals_str(ctoon_val *val, const char *str);
bool ctoon_equals_strn(ctoon_val *val, const char *str, size_t len);

const char *ctoon_get_raw(ctoon_val *val);



/*==============================================================================
 * MARK: - Array Access
 *============================================================================*/

size_t ctoon_arr_size(ctoon_val *arr);
ctoon_val *ctoon_arr_get(ctoon_val *arr, size_t idx);
ctoon_val *ctoon_arr_get_first(ctoon_val *arr);
ctoon_val *ctoon_arr_get_next(ctoon_val *arr);



/*==============================================================================
 * MARK: - Object Access
 *============================================================================*/

size_t ctoon_obj_size(ctoon_val *obj);
ctoon_val *ctoon_obj_get(ctoon_val *obj, const char *key);
ctoon_val *ctoon_obj_getn(ctoon_val *obj, const char *key, size_t len);
ctoon_val *ctoon_obj_iter_get(ctoon_val *obj, ctoon_val **key);
ctoon_val *ctoon_obj_iter_next(ctoon_val *obj, ctoon_val **key);
const char *ctoon_obj_iter_key(ctoon_val *key);
size_t ctoon_obj_iter_key_len(ctoon_val *key);



/*==============================================================================
 * MARK: - JSON Read/Write
 *============================================================================*/

#ifndef CTOON_WITHOUT_JSON
ctoon_doc *ctoon_read_json(const char *dat, size_t len, ctoon_read_flag flg);
ctoon_doc *ctoon_read_json_opts(const char *dat, size_t len, ctoon_read_flag flg,
                                ctoon_alc *alc, ctoon_err *err);
ctoon_doc *ctoon_read_json_file(const char *path, ctoon_read_flag flg,
                                ctoon_alc *alc, ctoon_err *err);

char *ctoon_write_json(ctoon_doc *doc, ctoon_write_flag flg, size_t *len);
char *ctoon_write_json_opts(ctoon_doc *doc, ctoon_write_flag flg,
                            ctoon_alc *alc, size_t *len, ctoon_err *err);
bool ctoon_write_json_file(const char *path, ctoon_doc *doc,
                           ctoon_write_flag flg, ctoon_err *err);
#endif



/*==============================================================================
 * MARK: - TOON Read/Write
 *============================================================================*/

ctoon_doc *ctoon_read_toon(const char *dat, size_t len, ctoon_toon_read_flag flg);
ctoon_doc *ctoon_read_toon_opts(const char *dat, size_t len, ctoon_toon_read_flag flg,
                                ctoon_alc *alc, ctoon_err *err);
ctoon_doc *ctoon_read_toon_file(const char *path, ctoon_toon_read_flag flg,
                                ctoon_alc *alc, ctoon_err *err);

char *ctoon_write_toon(ctoon_doc *doc, ctoon_toon_write_flag flg, size_t *len);
char *ctoon_write_toon_opts(ctoon_doc *doc, ctoon_toon_write_flag flg,
                            ctoon_alc *alc, size_t *len, ctoon_err *err);
bool ctoon_write_toon_file(const char *path, ctoon_doc *doc,
                           ctoon_toon_write_flag flg, ctoon_err *err);



/*==============================================================================
 * MARK: - Error Handling
 *============================================================================*/

struct ctoon_err {
    size_t pos;
    size_t code;
    const char *msg;
};

const char *ctoon_get_err_msg(ctoon_err *err);
size_t ctoon_get_err_pos(ctoon_err *err);
size_t ctoon_get_err_code(ctoon_err *err);



/*==============================================================================
 * MARK: - Memory Allocation (Advanced)
 *============================================================================*/

struct ctoon_alc {
    void *(*malloc)(void *ctx, size_t size);
    void *(*realloc)(void *ctx, void *ptr, size_t old_size, size_t new_size);
    void (*free)(void *ctx, void *ptr);
    void *ctx;
};

ctoon_alc *ctoon_alc_new(void);
void ctoon_alc_free(ctoon_alc *alc);
ctoon_alc *ctoon_alc_new_default(void);



/*==============================================================================
 * MARK: - Mutable API (Advanced)
 *============================================================================*/

ctoon_mut_val *ctoon_mut_doc_get_root(ctoon_doc *doc);
ctoon_mut_val *ctoon_mut_arr_add_val(ctoon_mut_val *arr, ctoon_mut_val *val);
ctoon_mut_val *ctoon_mut_obj_add_val(ctoon_mut_val *obj, const char *key,
                                     ctoon_mut_val *val);
ctoon_mut_val *ctoon_mut_obj_add_valn(ctoon_mut_val *obj, const char *key,
                                      size_t len, ctoon_mut_val *val);

ctoon_mut_val *ctoon_mut_null(void);
ctoon_mut_val *ctoon_mut_bool(bool val);
ctoon_mut_val *ctoon_mut_uint(uint64_t val);
ctoon_mut_val *ctoon_mut_sint(int64_t val);
ctoon_mut_val *ctoon_mut_real(double val);
ctoon_mut_val *ctoon_mut_str(const char *val);
ctoon_mut_val *ctoon_mut_strn(const char *val, size_t len);
ctoon_mut_val *ctoon_mut_arr(void);
ctoon_mut_val *ctoon_mut_obj(void);



/*==============================================================================
 * MARK: - C++ Wrapper
 *============================================================================*/

#ifdef __cplusplus
}
#endif

#endif /* CTOON_H */