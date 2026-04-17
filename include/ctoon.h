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

#define CTOON_VERSION_ENCODE(major, minor, patch) (((major) * 10000) + ((minor) * 100) + ((patch) * 1))
#define CTOON_VERSION CTOON_VERSION_ENCODE(CTOON_VERSION_MAJOR, CTOON_VERSION_MINOR, CTOON_VERSION_PATCH)

#define CTOON_VERSION_XSTRINGIZE(major, minor, patch) #major"."#minor"."#patch
#define CTOON_VERSION_STRINGIZE(major, minor, patch) CTOON_VERSION_XSTRINGIZE(major, minor, patch)
#define CTOON_VERSION_STRING CTOON_VERSION_STRINGIZE(CTOON_VERSION_MAJOR, CTOON_VERSION_MINOR, CTOON_VERSION_PATCH)


/*==============================================================================
 * MARK: - Types
 *============================================================================*/

typedef struct ctoon_doc ctoon_doc;
typedef struct ctoon_val ctoon_val;

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
 * MARK: - TOON Read/Write
 *============================================================================*/

ctoon_doc *ctoon_read_toon(const char *dat, size_t len);
ctoon_doc *ctoon_read_toon_file(const char *path);

char *ctoon_write_toon(ctoon_doc *doc, size_t *len);
bool ctoon_write_toon_file(const char *path, ctoon_doc *doc);


/*==============================================================================
 * MARK: - Value Creation
 *============================================================================*/

ctoon_val *ctoon_new_null(ctoon_doc *doc);
ctoon_val *ctoon_new_bool(ctoon_doc *doc, bool value);
ctoon_val *ctoon_new_number(ctoon_doc *doc, double value);
ctoon_val *ctoon_new_string(ctoon_doc *doc, const char *value);
ctoon_val *ctoon_new_array(ctoon_doc *doc);
ctoon_val *ctoon_new_object(ctoon_doc *doc);

void ctoon_array_append(ctoon_val *array, ctoon_val *value);
void ctoon_object_set(ctoon_val *object, const char *key, ctoon_val *value);
void ctoon_set_root(ctoon_doc *doc, ctoon_val *root);



/*==============================================================================
 * MARK: - C++ Wrapper
 *============================================================================*/

#ifdef __cplusplus
}
#endif

#endif /* CTOON_H */