/*==============================================================================
 |  ctoon.h  –  CToon C API
 |  Copyright (c) 2026 CToon Project, MIT License
 *============================================================================*/

/**
 * @file  ctoon.h
 * @brief CToon: parse and generate TOON-format data.
 * @date 2025-11-04
 * @author Mohammad Raziei
 *
 * Naming follows yyjson conventions where possible:
 *   ctoon_read_*   – parse TOON text  → ctoon_doc
 *   ctoon_write_*  – serialise doc    → text
 *   ctoon_decode*  – high-level: TOON text → doc  (options-aware)
 *   ctoon_encode*  – high-level: doc  → TOON text (options-aware)
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

#define CTOON_VERSION_ENCODE(maj,min,pat) (((maj)*10000)+((min)*100)+(pat))
#define CTOON_VERSION \
    CTOON_VERSION_ENCODE(CTOON_VERSION_MAJOR,CTOON_VERSION_MINOR,CTOON_VERSION_PATCH)

#define CTOON_VERSION_XSTR(a,b,c) #a"."#b"."#c
#define CTOON_VERSION_STR(a,b,c)  CTOON_VERSION_XSTR(a,b,c)
#define CTOON_VERSION_STRING \
    CTOON_VERSION_STR(CTOON_VERSION_MAJOR,CTOON_VERSION_MINOR,CTOON_VERSION_PATCH)


/* ============================================================
 * Opaque types
 * ============================================================ */

/** Document – owns all arena memory for its values. */
typedef struct ctoon_doc ctoon_doc;

/** Value – points into document arena; do NOT free individually. */
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
    CTOON_SUBTYPE_UINT,   /**< Non-negative integer stored as uint64 */
    CTOON_SUBTYPE_SINT,   /**< Negative integer stored as int64      */
    CTOON_SUBTYPE_REAL    /**< Floating-point stored as double        */
} ctoon_subtype;


/* ============================================================
 * Encode / Decode flags and options
 * ============================================================ */

typedef uint32_t ctoon_read_flag;
#define CTOON_READ_NOFLAG     ((ctoon_read_flag)0)
/** Disable strict validation (array length checks, indent multiples, etc.) */
#define CTOON_READ_NO_STRICT  ((ctoon_read_flag)(1u << 0))

typedef uint32_t ctoon_write_flag;
#define CTOON_WRITE_NOFLAG        ((ctoon_write_flag)0)
/** Prefix array lengths with '#': items[#3]: ...  */
#define CTOON_WRITE_LENGTH_MARKER ((ctoon_write_flag)(1u << 0))

/** Array delimiter used during encoding. */
typedef enum ctoon_delimiter {
    CTOON_DELIMITER_COMMA = 0, /**< ,  (default)  */
    CTOON_DELIMITER_TAB,       /**< \t             */
    CTOON_DELIMITER_PIPE       /**< |              */
} ctoon_delimiter;

/** Error information populated on failure; msg is NULL on success. */
typedef struct ctoon_err {
    const char *msg; /**< Static string, or NULL if no error. */
    size_t      pos; /**< Byte offset where error was detected. */
} ctoon_err;

/** Full options for ctoon_read_opts() */
typedef struct ctoon_read_options {
    ctoon_read_flag flag;   /**< CTOON_READ_* flags.              */
    int             indent; /**< Spaces per indent level (default 2). */
} ctoon_read_options;

/** Full options for ctoon_write_opts() */
typedef struct ctoon_write_options {
    ctoon_write_flag  flag;      /**< CTOON_WRITE_* flags.              */
    ctoon_delimiter   delimiter; /**< Array value delimiter.            */
    int               indent;    /**< Spaces per indent level (default 2). */
} ctoon_write_options;


/* ============================================================
 * Document lifecycle
 * ============================================================ */

ctoon_doc  *ctoon_doc_new          (size_t max_memory);
void        ctoon_doc_free         (ctoon_doc *doc);
ctoon_val  *ctoon_doc_get_root     (ctoon_doc *doc);
void        ctoon_doc_set_root     (ctoon_doc *doc, ctoon_val *root);
size_t      ctoon_doc_get_val_count(ctoon_doc *doc);
const char *ctoon_doc_get_error    (ctoon_doc *doc);
size_t      ctoon_doc_get_error_pos(ctoon_doc *doc);


/* ============================================================
 * Low-level read / write  (like yyjson_read / yyjson_write)
 * ============================================================ */

/**
 * Parse a TOON string.
 * @param dat  Input bytes (need not be NUL-terminated).
 * @param len  Byte length.
 * @return New doc on success (caller must ctoon_doc_free), NULL on failure.
 */
ctoon_doc *ctoon_read      (const char *dat, size_t len);

/**
 * Parse a TOON string with flags.
 */
ctoon_doc *ctoon_read_flg  (const char *dat, size_t len, ctoon_read_flag flg);

/**
 * Parse a TOON string with full options and error output.
 */
ctoon_doc *ctoon_read_opts (const char *dat, size_t len,
                              const ctoon_read_options *opts, ctoon_err *err);

/**
 * Parse a TOON file.
 */
ctoon_doc *ctoon_read_file (const char *path);
ctoon_doc *ctoon_read_file_opts(const char *path,
                                 const ctoon_read_options *opts, ctoon_err *err);

/**
 * Serialise a document to a TOON string.
 * @param len  Receives byte length; may be NULL.
 * @return Heap-allocated NUL-terminated string; caller must free(). NULL on error.
 */
char *ctoon_write     (ctoon_doc *doc, size_t *len);

/**
 * Serialise with flags.
 */
char *ctoon_write_flg (ctoon_doc *doc, ctoon_write_flag flg, size_t *len);

/**
 * Serialise with full options and error output.
 */
char *ctoon_write_opts(ctoon_doc *doc, const ctoon_write_options *opts,
                        size_t *len, ctoon_err *err);

/**
 * Serialise to file.
 */
bool ctoon_write_file     (ctoon_doc *doc, const char *path);
bool ctoon_write_file_opts(ctoon_doc *doc, const char *path,
                            const ctoon_write_options *opts, ctoon_err *err);


/* ============================================================
 * High-level decode / encode  (preferred API)
 *
 * These mirror the Python API: decode(toon_str) → doc,
 * encode(doc) → toon_str, with options.
 * ============================================================ */

/**
 * Decode a TOON string.  Equivalent to ctoon_read() but encode/decode naming.
 */
ctoon_doc *ctoon_decode     (const char *dat, size_t len);
ctoon_doc *ctoon_decode_flg (const char *dat, size_t len, ctoon_read_flag flg);
ctoon_doc *ctoon_decode_opts(const char *dat, size_t len,
                               const ctoon_read_options *opts, ctoon_err *err);
ctoon_doc *ctoon_decode_file(const char *path);
ctoon_doc *ctoon_decode_file_opts(const char *path,
                                   const ctoon_read_options *opts, ctoon_err *err);

/**
 * Encode a document to TOON.  Equivalent to ctoon_write() but encode/decode naming.
 */
char *ctoon_encode     (ctoon_doc *doc, size_t *len);
char *ctoon_encode_flg (ctoon_doc *doc, ctoon_write_flag flg, size_t *len);
char *ctoon_encode_opts(ctoon_doc *doc, const ctoon_write_options *opts,
                         size_t *len, ctoon_err *err);
bool  ctoon_encode_file(ctoon_doc *doc, const char *path);
bool  ctoon_encode_file_opts(ctoon_doc *doc, const char *path,
                               const ctoon_write_options *opts, ctoon_err *err);


/* ============================================================
 * Type queries
 * ============================================================ */

ctoon_type    ctoon_get_type   (ctoon_val *val);
ctoon_subtype ctoon_get_subtype(ctoon_val *val);

bool ctoon_is_null  (ctoon_val *val);
bool ctoon_is_true  (ctoon_val *val);
bool ctoon_is_false (ctoon_val *val);
bool ctoon_is_bool  (ctoon_val *val);
bool ctoon_is_num   (ctoon_val *val);
bool ctoon_is_uint  (ctoon_val *val);
bool ctoon_is_sint  (ctoon_val *val);
bool ctoon_is_int   (ctoon_val *val);
bool ctoon_is_real  (ctoon_val *val);
bool ctoon_is_str   (ctoon_val *val);
bool ctoon_is_arr   (ctoon_val *val);
bool ctoon_is_obj   (ctoon_val *val);
bool ctoon_is_ctn   (ctoon_val *val); /**< array OR object */
bool ctoon_is_raw   (ctoon_val *val);


/* ============================================================
 * Value accessors
 * ============================================================ */

bool        ctoon_get_bool (ctoon_val *val);
uint64_t    ctoon_get_uint (ctoon_val *val);
int64_t     ctoon_get_sint (ctoon_val *val);
int         ctoon_get_int  (ctoon_val *val);
double      ctoon_get_real (ctoon_val *val);

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
/** Sequential iterator – NOT reentrant. */
ctoon_val *ctoon_arr_get_next (ctoon_val *arr);


/* ============================================================
 * Object access
 * ============================================================ */

size_t     ctoon_obj_size (ctoon_val *obj);
ctoon_val *ctoon_obj_get  (ctoon_val *obj, const char *key);
ctoon_val *ctoon_obj_getn (ctoon_val *obj, const char *key, size_t len);

/** Index-based access (no global state, safe for recursive use). */
ctoon_val *ctoon_obj_get_key_at(ctoon_val *obj, size_t idx);
ctoon_val *ctoon_obj_get_val_at(ctoon_val *obj, size_t idx);

/** Sequential iterator – NOT reentrant. */
ctoon_val *ctoon_obj_iter_get (ctoon_val *obj, ctoon_val **key);
ctoon_val *ctoon_obj_iter_next(ctoon_val *obj, ctoon_val **key);


/* ============================================================
 * Value construction
 * ============================================================ */

ctoon_val *ctoon_new_null  (ctoon_doc *doc);
ctoon_val *ctoon_new_bool  (ctoon_doc *doc, bool     val);
ctoon_val *ctoon_new_uint  (ctoon_doc *doc, uint64_t val);
ctoon_val *ctoon_new_sint  (ctoon_doc *doc, int64_t  val);
ctoon_val *ctoon_new_real  (ctoon_doc *doc, double   val);
ctoon_val *ctoon_new_str   (ctoon_doc *doc, const char *val);
ctoon_val *ctoon_new_strn  (ctoon_doc *doc, const char *val, size_t len);
ctoon_val *ctoon_new_arr   (ctoon_doc *doc);
ctoon_val *ctoon_new_obj   (ctoon_doc *doc);

bool ctoon_arr_append(ctoon_doc *doc, ctoon_val *arr, ctoon_val *val);
bool ctoon_obj_set   (ctoon_doc *doc, ctoon_val *obj, const char *key, ctoon_val *val);
bool ctoon_obj_setn  (ctoon_doc *doc, ctoon_val *obj,
                       const char *key, size_t key_len, ctoon_val *val);



/* ============================================================
 * Backwards-compatible declarations (deprecated, use new names)
 * ============================================================ */

ctoon_val *ctoon_new_array (ctoon_doc *doc);
ctoon_val *ctoon_new_object(ctoon_doc *doc);
bool ctoon_array_append(ctoon_doc *doc, ctoon_val *arr, ctoon_val *val);
bool ctoon_object_set  (ctoon_doc *doc, ctoon_val *obj, const char *key, ctoon_val *val);
bool ctoon_object_setn (ctoon_doc *doc, ctoon_val *obj,
                        const char *key, size_t key_len, ctoon_val *val);
ctoon_doc *ctoon_read_toon     (const char *dat, size_t len);
ctoon_doc *ctoon_read_toon_file(const char *path);
char      *ctoon_write_toon    (ctoon_doc *doc, size_t *len);
bool       ctoon_write_toon_file(const char *path, ctoon_doc *doc);


#ifdef __cplusplus
}
#endif

#endif /* CTOON_H */
