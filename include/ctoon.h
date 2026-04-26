/*==============================================================================
 Copyright (c) 2026 Mohammad Raziei <mohammadraziei1375@gmail.com>

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
 @date 2019-03-09
 @author Mohammad Razeiei
 */

#ifndef CTOON_H
#define CTOON_H



/*==============================================================================
 * MARK: - Header Files
 *============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <float.h>



/*==============================================================================
 * MARK: - Compile-time Options
 *============================================================================*/

/*
 Define as 1 to disable TOON reader at compile-time.
 This disables functions with "read" in their name.
 Reduces binary size by about 60%.
 */
#ifndef CTOON_DISABLE_READER
#endif

/*
 Define as 1 to disable TOON writer at compile-time.
 This disables functions with "write" in their name.
 Reduces binary size by about 30%.
 */
#ifndef CTOON_DISABLE_WRITER
#endif

/*
 Define as 1 to disable TOON incremental reader at compile-time.
 This disables functions with "incr" in their name.
 */
#ifndef CTOON_DISABLE_INCR_READER
#endif

/*
 Define as 1 to disable TOON Pointer, TOON Patch and TOON Merge Patch supports.
 This disables functions with "ptr" or "patch" in their name.
 */
#ifndef CTOON_DISABLE_UTILS
#endif

/*
 Define as 1 to disable the fast floating-point number conversion in ctoon.
 Libc's `strtod/snprintf` will be used instead.

 This reduces binary size by about 30%, but significantly slows down the
 floating-point read/write speed.
 */
#ifndef CTOON_DISABLE_FAST_FP_CONV
#endif

/*
 Define as 1 to disable non-standard TOON features support at compile-time,
 such as CTOON_READ_ALLOW_XXX and CTOON_WRITE_ALLOW_XXX.

 This reduces binary size by about 10%, and slightly improves performance.
 */
#ifndef CTOON_DISABLE_NON_STANDARD
#endif

/*
 Define as 1 to disable UTF-8 validation at compile-time.

 Use this if all input strings are guaranteed to be valid UTF-8
 (e.g. language-level String types are already validated).

 Disabling UTF-8 validation improves performance for non-ASCII strings by about
 3% to 7%.

 Note: If this flag is enabled while passing illegal UTF-8 strings,
 the following errors may occur:
 - Escaped characters may be ignored when parsing TOON strings.
 - Ending quotes may be ignored when parsing TOON strings, causing the
   string to merge with the next value.
 - When serializing with `ctoon_mut_val`, the string's end may be accessed
   out of bounds, potentially causing a segmentation fault.
 */
#ifndef CTOON_DISABLE_UTF8_VALIDATION
#endif

/*
 Define as 1 to improve performance on architectures that do not support
 unaligned memory access.

 Normally, this does not need to be set manually. See the C file for details.
 */
#ifndef CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
#endif

/* Define as 1 to export symbols when building this library as a Windows DLL. */
#ifndef CTOON_EXPORTS
#endif

/* Define as 1 to import symbols when using this library as a Windows DLL. */
#ifndef CTOON_IMPORTS
#endif

/* Define as 1 to include <stdint.h> for compilers without C99 support. */
#ifndef CTOON_HAS_STDINT_H
#endif

/* Define as 1 to include <stdbool.h> for compilers without C99 support. */
#ifndef CTOON_HAS_STDBOOL_H
#endif

/* Define to an integer to set a depth limit for containers (arrays, objects). */
#ifndef CTOON_READER_DEPTH_LIMIT
#endif


/*==============================================================================
 * MARK: - Compiler Macros
 *============================================================================*/

/** compiler version (MSVC) */
#ifdef _MSC_VER
#   define CTOON_MSC_VER _MSC_VER
#else
#   define CTOON_MSC_VER 0
#endif

/** compiler version (GCC) */
#ifdef __GNUC__
#   define CTOON_GCC_VER __GNUC__
#   if defined(__GNUC_PATCHLEVEL__)
#       define ctoon_gcc_available(major, minor, patch) \
            ((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) \
            >= (major * 10000 + minor * 100 + patch))
#   else
#       define ctoon_gcc_available(major, minor, patch) \
            ((__GNUC__ * 10000 + __GNUC_MINOR__ * 100) \
            >= (major * 10000 + minor * 100 + patch))
#   endif
#else
#   define CTOON_GCC_VER 0
#   define ctoon_gcc_available(major, minor, patch) 0
#endif

/** real gcc check */
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && \
    !defined(__clang__) && !defined(__llvm__) && \
    !defined(__INTEL_COMPILER) && !defined(__ICC) && \
    !defined(__NVCC__) && !defined(__PGI) && !defined(__TINYC__)
#   define CTOON_IS_REAL_GCC 1
#else
#   define CTOON_IS_REAL_GCC 0
#endif

/** C version (STDC) */
#if defined(__STDC__) && (__STDC__ >= 1) && defined(__STDC_VERSION__)
#   define CTOON_STDC_VER __STDC_VERSION__
#else
#   define CTOON_STDC_VER 0
#endif

/** C++ version */
#if defined(__cplusplus)
#   define CTOON_CPP_VER __cplusplus
#else
#   define CTOON_CPP_VER 0
#endif

/** compiler builtin check (since gcc 10.0, clang 2.6, icc 2021) */
#ifndef ctoon_has_builtin
#   ifdef __has_builtin
#       define ctoon_has_builtin(x) __has_builtin(x)
#   else
#       define ctoon_has_builtin(x) 0
#   endif
#endif

/** compiler attribute check (since gcc 5.0, clang 2.9, icc 17) */
#ifndef ctoon_has_attribute
#   ifdef __has_attribute
#       define ctoon_has_attribute(x) __has_attribute(x)
#   else
#       define ctoon_has_attribute(x) 0
#   endif
#endif

/** compiler feature check (since clang 2.6, icc 17) */
#ifndef ctoon_has_feature
#   ifdef __has_feature
#       define ctoon_has_feature(x) __has_feature(x)
#   else
#       define ctoon_has_feature(x) 0
#   endif
#endif

/** include check (since gcc 5.0, clang 2.7, icc 16, msvc 2017 15.3) */
#ifndef ctoon_has_include
#   ifdef __has_include
#       define ctoon_has_include(x) __has_include(x)
#   else
#       define ctoon_has_include(x) 0
#   endif
#endif

/** inline for compiler */
#ifndef ctoon_inline
#   if CTOON_MSC_VER >= 1200
#       define ctoon_inline __forceinline
#   elif defined(_MSC_VER)
#       define ctoon_inline __inline
#   elif ctoon_has_attribute(always_inline) || CTOON_GCC_VER >= 4
#       define ctoon_inline __inline__ __attribute__((always_inline))
#   elif defined(__clang__) || defined(__GNUC__)
#       define ctoon_inline __inline__
#   elif defined(__cplusplus) || CTOON_STDC_VER >= 199901L
#       define ctoon_inline inline
#   else
#       define ctoon_inline
#   endif
#endif

/** noinline for compiler */
#ifndef ctoon_noinline
#   if CTOON_MSC_VER >= 1400
#       define ctoon_noinline __declspec(noinline)
#   elif ctoon_has_attribute(noinline) || CTOON_GCC_VER >= 4
#       define ctoon_noinline __attribute__((noinline))
#   else
#       define ctoon_noinline
#   endif
#endif

/** align for compiler */
#ifndef ctoon_align
#   if CTOON_MSC_VER >= 1300
#       define ctoon_align(x) __declspec(align(x))
#   elif ctoon_has_attribute(aligned) || defined(__GNUC__)
#       define ctoon_align(x) __attribute__((aligned(x)))
#   elif CTOON_CPP_VER >= 201103L
#       define ctoon_align(x) alignas(x)
#   else
#       define ctoon_align(x)
#   endif
#endif

/** likely for compiler */
#ifndef ctoon_likely
#   if ctoon_has_builtin(__builtin_expect) || \
    (CTOON_GCC_VER >= 4 && CTOON_GCC_VER != 5)
#       define ctoon_likely(expr) __builtin_expect(!!(expr), 1)
#   else
#       define ctoon_likely(expr) (expr)
#   endif
#endif

/** unlikely for compiler */
#ifndef ctoon_unlikely
#   if ctoon_has_builtin(__builtin_expect) || \
    (CTOON_GCC_VER >= 4 && CTOON_GCC_VER != 5)
#       define ctoon_unlikely(expr) __builtin_expect(!!(expr), 0)
#   else
#       define ctoon_unlikely(expr) (expr)
#   endif
#endif

/** compile-time constant check for compiler */
#ifndef ctoon_constant_p
#   if ctoon_has_builtin(__builtin_constant_p) || (CTOON_GCC_VER >= 3)
#       define CTOON_HAS_CONSTANT_P 1
#       define ctoon_constant_p(value) __builtin_constant_p(value)
#   else
#       define CTOON_HAS_CONSTANT_P 0
#       define ctoon_constant_p(value) 0
#   endif
#endif

/** deprecate warning */
#ifndef ctoon_deprecated
#   if CTOON_MSC_VER >= 1400
#       define ctoon_deprecated(msg) __declspec(deprecated(msg))
#   elif ctoon_has_feature(attribute_deprecated_with_message) || \
        (CTOON_GCC_VER > 4 || (CTOON_GCC_VER == 4 && __GNUC_MINOR__ >= 5))
#       define ctoon_deprecated(msg) __attribute__((deprecated(msg)))
#   elif CTOON_GCC_VER >= 3
#       define ctoon_deprecated(msg) __attribute__((deprecated))
#   else
#       define ctoon_deprecated(msg)
#   endif
#endif

/** function export */
#ifndef ctoon_api
#   if defined(_WIN32)
#       if defined(CTOON_EXPORTS) && CTOON_EXPORTS
#           define ctoon_api __declspec(dllexport)
#       elif defined(CTOON_IMPORTS) && CTOON_IMPORTS
#           define ctoon_api __declspec(dllimport)
#       else
#           define ctoon_api
#       endif
#   elif ctoon_has_attribute(visibility) || CTOON_GCC_VER >= 4
#       define ctoon_api __attribute__((visibility("default")))
#   else
#       define ctoon_api
#   endif
#endif

/** inline function export */
#ifndef ctoon_api_inline
#   define ctoon_api_inline static ctoon_inline
#endif

/** stdint (C89 compatible) */
#if (defined(CTOON_HAS_STDINT_H) && CTOON_HAS_STDINT_H) || \
    CTOON_MSC_VER >= 1600 || CTOON_STDC_VER >= 199901L || \
    defined(_STDINT_H) || defined(_STDINT_H_) || \
    defined(__CLANG_STDINT_H) || defined(_STDINT_H_INCLUDED) || \
    ctoon_has_include(<stdint.h>)
#   include <stdint.h>
#elif defined(_MSC_VER)
#   if _MSC_VER < 1300
        typedef signed char         int8_t;
        typedef signed short        int16_t;
        typedef signed int          int32_t;
        typedef unsigned char       uint8_t;
        typedef unsigned short      uint16_t;
        typedef unsigned int        uint32_t;
        typedef signed __int64      int64_t;
        typedef unsigned __int64    uint64_t;
#   else
        typedef signed __int8       int8_t;
        typedef signed __int16      int16_t;
        typedef signed __int32      int32_t;
        typedef unsigned __int8     uint8_t;
        typedef unsigned __int16    uint16_t;
        typedef unsigned __int32    uint32_t;
        typedef signed __int64      int64_t;
        typedef unsigned __int64    uint64_t;
#   endif
#else
#   if UCHAR_MAX == 0xFFU
        typedef signed char     int8_t;
        typedef unsigned char   uint8_t;
#   else
#       error cannot find 8-bit integer type
#   endif
#   if USHRT_MAX == 0xFFFFU
        typedef unsigned short  uint16_t;
        typedef signed short    int16_t;
#   elif UINT_MAX == 0xFFFFU
        typedef unsigned int    uint16_t;
        typedef signed int      int16_t;
#   else
#       error cannot find 16-bit integer type
#   endif
#   if UINT_MAX == 0xFFFFFFFFUL
        typedef unsigned int    uint32_t;
        typedef signed int      int32_t;
#   elif ULONG_MAX == 0xFFFFFFFFUL
        typedef unsigned long   uint32_t;
        typedef signed long     int32_t;
#   elif USHRT_MAX == 0xFFFFFFFFUL
        typedef unsigned short  uint32_t;
        typedef signed short    int32_t;
#   else
#       error cannot find 32-bit integer type
#   endif
#   if defined(__INT64_TYPE__) && defined(__UINT64_TYPE__)
        typedef __INT64_TYPE__  int64_t;
        typedef __UINT64_TYPE__ uint64_t;
#   elif defined(__GNUC__) || defined(__clang__)
#       if !defined(_SYS_TYPES_H) && !defined(__int8_t_defined)
        __extension__ typedef long long             int64_t;
#       endif
        __extension__ typedef unsigned long long    uint64_t;
#   elif defined(_LONG_LONG) || defined(__MWERKS__) || defined(_CRAYC) || \
        defined(__SUNPRO_C) || defined(__SUNPRO_CC)
        typedef long long           int64_t;
        typedef unsigned long long  uint64_t;
#   elif (defined(__BORLANDC__) && __BORLANDC__ > 0x460) || \
        defined(__WATCOM_INT64__) || defined (__alpha) || defined (__DECC)
        typedef __int64             int64_t;
        typedef unsigned __int64    uint64_t;
#   else
#       error cannot find 64-bit integer type
#   endif
#endif

/** stdbool (C89 compatible) */
#if (defined(CTOON_HAS_STDBOOL_H) && CTOON_HAS_STDBOOL_H) || \
    CTOON_MSC_VER >= 1800 || CTOON_STDC_VER >= 199901L || \
    (ctoon_has_include(<stdbool.h>) && !defined(__STRICT_ANSI__))
#   include <stdbool.h>
#elif !defined(__bool_true_false_are_defined)
#   define __bool_true_false_are_defined 1
#   if defined(__cplusplus)
#       if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#           define _Bool bool
#           if __cplusplus < 201103L
#               define bool bool
#               define false false
#               define true true
#           endif
#       endif
#   else
#       define bool unsigned char
#       define true 1
#       define false 0
#   endif
#endif

/** char bit check */
#if defined(CHAR_BIT)
#   if CHAR_BIT != 8
#       error non 8-bit char is not supported
#   endif
#endif

/**
 Microsoft Visual C++ 6.0 doesn't support converting number from u64 to f64:
 error C2520: conversion from unsigned __int64 to double not implemented.
 */
#ifndef CTOON_U64_TO_F64_NO_IMPL
#   if (0 < CTOON_MSC_VER) && (CTOON_MSC_VER <= 1200)
#       define CTOON_U64_TO_F64_NO_IMPL 1
#   else
#       define CTOON_U64_TO_F64_NO_IMPL 0
#   endif
#endif



/*==============================================================================
 * MARK: - Compile Hint Begin
 *============================================================================*/

/* extern "C" begin */
#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * MARK: - Version
 *============================================================================*/

/** The major version of ctoon. */
#define CTOON_VERSION_MAJOR  0

/** The minor version of ctoon. */
#define CTOON_VERSION_MINOR  2

/** The patch version of ctoon. */
#define CTOON_VERSION_PATCH  0

#define CTOON_VERSION_ENCODE(maj,min,pat) (((maj)*10000)+((min)*100)+(pat))
#define CTOON_VERSION \
    CTOON_VERSION_ENCODE(CTOON_VERSION_MAJOR,CTOON_VERSION_MINOR,CTOON_VERSION_PATCH)

/** The version of ctoon in hex: `(major << 16) | (minor << 8) | (patch)`. */
#define CTOON_VERSION_HEX \
    ((CTOON_VERSION_MAJOR << 16) | (CTOON_VERSION_MINOR << 8) | CTOON_VERSION_PATCH)

/* Internal helpers for CTOON_VERSION_STRING */
#define _CTOON_VERSION_XSTR(a,b,c) #a"."#b"."#c
#define _CTOON_VERSION_STR(a,b,c)  _CTOON_VERSION_XSTR(a,b,c)

/** The version string of ctoon, e.g. "0.2.0". */
#define CTOON_VERSION_STRING \
    _CTOON_VERSION_STR(CTOON_VERSION_MAJOR,CTOON_VERSION_MINOR,CTOON_VERSION_PATCH)

/** The version of ctoon in hex, same as `CTOON_VERSION_HEX`. */
ctoon_api uint32_t ctoon_version(void);



/*==============================================================================
 * MARK: - TOON Types
 *============================================================================*/

/** Type of a TOON value (3 bit). */
typedef uint8_t ctoon_type;
/** No type, invalid. */
#define CTOON_TYPE_NONE        ((uint8_t)0)        /* _____000 */
/** Raw string type, no subtype. */
#define CTOON_TYPE_RAW         ((uint8_t)1)        /* _____001 */
/** Null type: `null` literal, no subtype. */
#define CTOON_TYPE_NULL        ((uint8_t)2)        /* _____010 */
/** Boolean type, subtype: TRUE, FALSE. */
#define CTOON_TYPE_BOOL        ((uint8_t)3)        /* _____011 */
/** Number type, subtype: UINT, SINT, REAL. */
#define CTOON_TYPE_NUM         ((uint8_t)4)        /* _____100 */
/** String type, subtype: NONE, NOESC. */
#define CTOON_TYPE_STR         ((uint8_t)5)        /* _____101 */
/** Array type, no subtype. */
#define CTOON_TYPE_ARR         ((uint8_t)6)        /* _____110 */
/** Object type, no subtype. */
#define CTOON_TYPE_OBJ         ((uint8_t)7)        /* _____111 */

/** Subtype of a TOON value (2 bit). */
typedef uint8_t ctoon_subtype;
/** No subtype. */
#define CTOON_SUBTYPE_NONE     ((uint8_t)(0 << 3)) /* ___00___ */
/** False subtype: `false` literal. */
#define CTOON_SUBTYPE_FALSE    ((uint8_t)(0 << 3)) /* ___00___ */
/** True subtype: `true` literal. */
#define CTOON_SUBTYPE_TRUE     ((uint8_t)(1 << 3)) /* ___01___ */
/** Unsigned integer subtype: `uint64_t`. */
#define CTOON_SUBTYPE_UINT     ((uint8_t)(0 << 3)) /* ___00___ */
/** Signed integer subtype: `int64_t`. */
#define CTOON_SUBTYPE_SINT     ((uint8_t)(1 << 3)) /* ___01___ */
/** Real number subtype: `double`. */
#define CTOON_SUBTYPE_REAL     ((uint8_t)(2 << 3)) /* ___10___ */
/** String that do not need to be escaped for writing (internal use). */
#define CTOON_SUBTYPE_NOESC    ((uint8_t)(1 << 3)) /* ___01___ */

/** The mask used to extract the type of a TOON value. */
#define CTOON_TYPE_MASK        ((uint8_t)0x07)     /* _____111 */
/** The number of bits used by the type. */
#define CTOON_TYPE_BIT         ((uint8_t)3)
/** The mask used to extract the subtype of a TOON value. */
#define CTOON_SUBTYPE_MASK     ((uint8_t)0x18)     /* ___11___ */
/** The number of bits used by the subtype. */
#define CTOON_SUBTYPE_BIT      ((uint8_t)2)
/** The mask used to extract the reserved bits of a TOON value. */
#define CTOON_RESERVED_MASK    ((uint8_t)0xE0)     /* 111_____ */
/** The number of reserved bits. */
#define CTOON_RESERVED_BIT     ((uint8_t)3)
/** The mask used to extract the tag of a TOON value. */
#define CTOON_TAG_MASK         ((uint8_t)0xFF)     /* 11111111 */
/** The number of bits used by the tag. */
#define CTOON_TAG_BIT          ((uint8_t)8)

/** Padding size for TOON reader. */
#define CTOON_PADDING_SIZE     4



/*==============================================================================
 * MARK: - Allocator
 *============================================================================*/

/**
 A memory allocator.

 Typically you don't need to use it, unless you want to customize your own
 memory allocator.
 */
typedef struct ctoon_alc {
    /** Same as libc's malloc(size), should not be NULL. */
    void *(*malloc)(void *ctx, size_t size);
    /** Same as libc's realloc(ptr, size), should not be NULL. */
    void *(*realloc)(void *ctx, void *ptr, size_t old_size, size_t size);
    /** Same as libc's free(ptr), should not be NULL. */
    void (*free)(void *ctx, void *ptr);
    /** A context for malloc/realloc/free, can be NULL. */
    void *ctx;
} ctoon_alc;

/**
 A pool allocator uses fixed length pre-allocated memory.

 This allocator may be used to avoid malloc/realloc calls. The pre-allocated
 memory should be held by the caller. The maximum amount of memory required to
 read a TOON can be calculated using the `ctoon_read_max_memory_usage()`
 function, but the amount of memory required to write a TOON cannot be directly
 calculated.

 This is not a general-purpose allocator. It is designed to handle a single TOON
 data at a time. If it is used for overly complex memory tasks, such as parsing
 multiple TOON documents using the same allocator but releasing only a few of
 them, it may cause memory fragmentation, resulting in performance degradation
 and memory waste.

 @param alc The allocator to be initialized.
    If this parameter is NULL, the function will fail and return false.
    If `buf` or `size` is invalid, this will be set to an empty allocator.
 @param buf The buffer memory for this allocator.
    If this parameter is NULL, the function will fail and return false.
 @param size The size of `buf`, in bytes.
    If this parameter is less than 8 words (32/64 bytes on 32/64-bit OS), the
    function will fail and return false.
 @return true if the `alc` has been successfully initialized.

 @b Example
 @code
    // parse TOON with stack memory
    char buf[1024];
    ctoon_alc alc;
    ctoon_alc_pool_init(&alc, buf, 1024);

    const char *json = "{\"name\":\"Helvetica\",\"size\":16}"
    ctoon_doc *doc = ctoon_read_opts(json, strlen(json), 0, &alc, NULL);
    // the memory of `doc` is on the stack
 @endcode

 @warning This Allocator is not thread-safe.
 */
ctoon_api bool ctoon_alc_pool_init(ctoon_alc *alc, void *buf, size_t size);

/**
 A dynamic allocator.

 This allocator has a similar usage to the pool allocator above. However, when
 there is not enough memory, this allocator will dynamically request more memory
 using libc's `malloc` function, and frees it all at once when it is destroyed.

 @return A new dynamic allocator, or NULL if memory allocation failed.
 @note The returned value should be freed with `ctoon_alc_dyn_free()`.

 @warning This Allocator is not thread-safe.
 */
ctoon_api ctoon_alc *ctoon_alc_dyn_new(void);

/**
 Free a dynamic allocator which is created by `ctoon_alc_dyn_new()`.
 @param alc The dynamic allocator to be destroyed.
 */
ctoon_api void ctoon_alc_dyn_free(ctoon_alc *alc);



/*==============================================================================
 * MARK: - Text Locating
 *============================================================================*/

/**
 Locate the line and column number for a byte position in a string.
 This can be used to get better description for error position.

 @param str The input string.
 @param len The byte length of the input string.
 @param pos The byte position within the input string.
 @param line A pointer to receive the line number, starting from 1.
 @param col  A pointer to receive the column number, starting from 1.
 @param chr  A pointer to receive the character index, starting from 0.
 @return true on success, false if `str` is NULL or `pos` is out of bounds.
 @note Line/column/character are calculated based on Unicode characters for
    compatibility with text editors. For multi-byte UTF-8 characters,
    the returned value may not directly correspond to the byte position.
 */
ctoon_api bool ctoon_locate_pos(const char *str, size_t len, size_t pos,
                                  size_t *line, size_t *col, size_t *chr);



/*==============================================================================
 * MARK: - TOON Structure
 *============================================================================*/

/**
 An immutable document for reading TOON.
 This document holds memory for all its TOON values and strings. When it is no
 longer used, the user should call `ctoon_doc_free()` to free its memory.
 */
typedef struct ctoon_doc ctoon_doc;

/**
 An immutable value for reading TOON.
 A TOON Value has the same lifetime as its document. The memory is held by its
 document and and cannot be freed alone.
 */
typedef struct ctoon_val ctoon_val;

/**
 A mutable document for building TOON.
 This document holds memory for all its TOON values and strings. When it is no
 longer used, the user should call `ctoon_mut_doc_free()` to free its memory.
 */
typedef struct ctoon_mut_doc ctoon_mut_doc;

/**
 A mutable value for building TOON.
 A TOON Value has the same lifetime as its document. The memory is held by its
 document and and cannot be freed alone.
 */
typedef struct ctoon_mut_val ctoon_mut_val;



/*==============================================================================
 * MARK: - TOON Reader API
 *============================================================================*/

/** Run-time options for TOON reader. */
typedef uint32_t ctoon_read_flag;

/** Default option (RFC 8259 compliant):
    - Read positive integer as uint64_t.
    - Read negative integer as int64_t.
    - Read floating-point number as double with round-to-nearest mode.
    - Read integer which cannot fit in uint64_t or int64_t as double.
    - Report error if double number is infinity.
    - Report error if string contains invalid UTF-8 character or BOM.
    - Report error on trailing commas, comments, inf and nan literals. */
static const ctoon_read_flag CTOON_READ_NOFLAG                    = 0;

/** Read the input data in-situ.
    This option allows the reader to modify and use input data to store string
    values, which can increase reading speed slightly.
    The caller should hold the input data before free the document.
    The input data must be padded by at least `CTOON_PADDING_SIZE` bytes.
    For example: `[1,2]` should be `[1,2]\0\0\0\0`, input length should be 5. */
static const ctoon_read_flag CTOON_READ_INSITU                    = 1 << 0;

/** Stop when done instead of issuing an error if there's additional content
    after a TOON document. This option may be used to parse small pieces of TOON
    in larger data, such as `NDTOON`. */
static const ctoon_read_flag CTOON_READ_STOP_WHEN_DONE            = 1 << 1;

/** Allow single trailing comma at the end of an object or array,
    such as `[1,2,3,]`, `{"a":1,"b":2,}` (non-standard). */
static const ctoon_read_flag CTOON_READ_ALLOW_TRAILING_COMMAS     = 1 << 2;

/** Allow C-style single-line and mult-line comments (non-standard). */
static const ctoon_read_flag CTOON_READ_ALLOW_COMMENTS            = 1 << 3;

/** Allow inf/nan number and literal, case-insensitive,
    such as 1e999, NaN, inf, -Infinity (non-standard). */
static const ctoon_read_flag CTOON_READ_ALLOW_INF_AND_NAN         = 1 << 4;

/** Read all numbers as raw strings (value with `CTOON_TYPE_RAW` type),
    inf/nan literal is also read as raw with `ALLOW_INF_AND_NAN` flag. */
static const ctoon_read_flag CTOON_READ_NUMBER_AS_RAW             = 1 << 5;

/** Allow reading invalid unicode when parsing string values (non-standard).
    Invalid characters will be allowed to appear in the string values, but
    invalid escape sequences will still be reported as errors.
    This flag does not affect the performance of correctly encoded strings.

    @warning Strings in TOON values may contain incorrect encoding when this
    option is used, you need to handle these strings carefully to avoid security
    risks. */
static const ctoon_read_flag CTOON_READ_ALLOW_INVALID_UNICODE     = 1 << 6;

/** Read big numbers as raw strings. These big numbers include integers that
    cannot be represented by `int64_t` and `uint64_t`, and floating-point
    numbers that cannot be represented by finite `double`.
    The flag will be overridden by `CTOON_READ_NUMBER_AS_RAW` flag. */
static const ctoon_read_flag CTOON_READ_BIGNUM_AS_RAW             = 1 << 7;

/** Allow UTF-8 BOM and skip it before parsing if any (non-standard). */
static const ctoon_read_flag CTOON_READ_ALLOW_BOM                 = 1 << 8;

/** Allow extended number formats (non-standard):
    - Hexadecimal numbers, such as `0x7B`.
    - Numbers with leading or trailing decimal point, such as `.123`, `123.`.
    - Numbers with a leading plus sign, such as `+123`. */
static const ctoon_read_flag CTOON_READ_ALLOW_EXT_NUMBER          = 1 << 9;

/** Allow extended escape sequences in strings (non-standard):
    - Additional escapes: `\a`, `\e`, `\v`, ``\'``, `\?`, `\0`.
    - Hex escapes: `\xNN`, such as `\x7B`.
    - Line continuation: backslash followed by line terminator sequences.
    - Unknown escape: if backslash is followed by an unsupported character,
        the backslash will be removed and the character will be kept as-is.
        However, `\1`-`\9` will still trigger an error. */
static const ctoon_read_flag CTOON_READ_ALLOW_EXT_ESCAPE          = 1 << 10;

/** Allow extended whitespace characters (non-standard):
    - Vertical tab `\v` and form feed `\f`.
    - Line separator `\u2028` and paragraph separator `\u2029`.
    - Non-breaking space `\xA0`.
    - Byte order mark: `\uFEFF`.
    - Other Unicode characters in the Zs (Separator, space) category. */
static const ctoon_read_flag CTOON_READ_ALLOW_EXT_WHITESPACE      = 1 << 11;

/** Allow strings enclosed in single quotes (non-standard), such as ``'ab'``. */
static const ctoon_read_flag CTOON_READ_ALLOW_SINGLE_QUOTED_STR   = 1 << 12;

/** Allow object keys without quotes (non-standard), such as `{a:1,b:2}`.
    This extends the ECMAScript IdentifierName rule by allowing any
    non-whitespace character with code point above `U+007F`. */
static const ctoon_read_flag CTOON_READ_ALLOW_UNQUOTED_KEY        = 1 << 13;

/** Allow TOON5 format, see: [https://json5.org].
    This flag supports all TOON5 features with some additional extensions:
    - Accepts more escape sequences than TOON5 (e.g. `\a`, `\e`).
    - Unquoted keys are not limited to ECMAScript IdentifierName.
    - Allow case-insensitive `NaN`, `Inf` and `Infinity` literals. */
static const ctoon_read_flag CTOON_READ_TOON5 =
    (1 << 2)  | /* CTOON_READ_ALLOW_TRAILING_COMMAS */
    (1 << 3)  | /* CTOON_READ_ALLOW_COMMENTS */
    (1 << 4)  | /* CTOON_READ_ALLOW_INF_AND_NAN */
    (1 << 9)  | /* CTOON_READ_ALLOW_EXT_NUMBER */
    (1 << 10) | /* CTOON_READ_ALLOW_EXT_ESCAPE */
    (1 << 11) | /* CTOON_READ_ALLOW_EXT_WHITESPACE */
    (1 << 12) | /* CTOON_READ_ALLOW_SINGLE_QUOTED_STR */
    (1 << 13);  /* CTOON_READ_ALLOW_UNQUOTED_KEY */



/** Result code for TOON reader. */
typedef uint32_t ctoon_read_code;

/** Success, no error. */
static const ctoon_read_code CTOON_READ_SUCCESS                       = 0;

/** Invalid parameter, such as NULL input string or 0 input length. */
static const ctoon_read_code CTOON_READ_ERROR_INVALID_PARAMETER       = 1;

/** Memory allocation failed. */
static const ctoon_read_code CTOON_READ_ERROR_MEMORY_ALLOCATION       = 2;

/** Input TOON string is empty. */
static const ctoon_read_code CTOON_READ_ERROR_EMPTY_CONTENT           = 3;

/** Unexpected content after document, such as `[123]abc`. */
static const ctoon_read_code CTOON_READ_ERROR_UNEXPECTED_CONTENT      = 4;

/** Unexpected end of input, the parsed part is valid, such as `[123`. */
static const ctoon_read_code CTOON_READ_ERROR_UNEXPECTED_END          = 5;

/** Unexpected character inside the document, such as `[abc]`. */
static const ctoon_read_code CTOON_READ_ERROR_UNEXPECTED_CHARACTER    = 6;

/** Invalid TOON structure, such as `[1,]`. */
static const ctoon_read_code CTOON_READ_ERROR_TOON_STRUCTURE          = 7;

/** Invalid comment, deprecated, use `UNEXPECTED_END` for unclosed comment. */
static const ctoon_read_code CTOON_READ_ERROR_INVALID_COMMENT         = 8;

/** Invalid number, such as `123.e12`, `000`. */
static const ctoon_read_code CTOON_READ_ERROR_INVALID_NUMBER          = 9;

/** Invalid string, such as invalid escaped character inside a string. */
static const ctoon_read_code CTOON_READ_ERROR_INVALID_STRING          = 10;

/** Invalid TOON literal, such as `truu`. */
static const ctoon_read_code CTOON_READ_ERROR_LITERAL                 = 11;

/** Failed to open a file. */
static const ctoon_read_code CTOON_READ_ERROR_FILE_OPEN               = 12;

/** Failed to read a file. */
static const ctoon_read_code CTOON_READ_ERROR_FILE_READ               = 13;

/** Incomplete input during incremental parsing; parsing state is preserved. */
static const ctoon_read_code CTOON_READ_ERROR_MORE                    = 14;

/** Read depth limit exceeded. */
static const ctoon_read_code CTOON_READ_ERROR_DEPTH                   = 15;

/** Error information for TOON reader. */
typedef struct ctoon_read_err {
    /** Error code, see `ctoon_read_code` for all possible values. */
    ctoon_read_code code;
    /** Error message, constant, no need to free (NULL if success). */
    const char *msg;
    /** Error byte position for input data (0 if success). */
    size_t pos;
} ctoon_read_err;



#if !defined(CTOON_DISABLE_READER) || !CTOON_DISABLE_READER

/**
 Read TOON with options.

 This function is thread-safe when:
 1. The `dat` is not modified by other threads.
 2. The `alc` is thread-safe or NULL.

 @param dat The TOON data (UTF-8 without BOM), null-terminator is not required.
    If this parameter is NULL, the function will fail and return NULL.
    The `dat` will not be modified without the flag `CTOON_READ_INSITU`, so you
    can pass a `const char *` string and case it to `char *` if you don't use
    the `CTOON_READ_INSITU` flag.
 @param len The length of TOON data in bytes.
    If this parameter is 0, the function will fail and return NULL.
 @param flg The TOON read options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by TOON reader.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return A new TOON document, or NULL if an error occurs.
    When it's no longer needed, it should be freed with `ctoon_doc_free()`.
 */
ctoon_api ctoon_doc *ctoon_read_opts(char *dat,
                                        size_t len,
                                        ctoon_read_flag flg,
                                        const ctoon_alc *alc,
                                        ctoon_read_err *err);

/**
 Read a TOON file.

 This function is thread-safe when:
 1. The file is not modified by other threads.
 2. The `alc` is thread-safe or NULL.

 @param path The TOON file's path.
    This should be a null-terminated string using the system's native encoding.
    If this path is NULL or invalid, the function will fail and return NULL.
 @param flg The TOON read options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by TOON reader.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return A new TOON document, or NULL if an error occurs.
    When it's no longer needed, it should be freed with `ctoon_doc_free()`.

 @warning On 32-bit operating system, files larger than 2GB may fail to read.
 */
ctoon_api ctoon_doc *ctoon_read_file(const char *path,
                                        ctoon_read_flag flg,
                                        const ctoon_alc *alc,
                                        ctoon_read_err *err);

/**
 Read TOON from a file pointer.

 @param fp The file pointer.
    The data will be read from the current position of the FILE to the end.
    If this fp is NULL or invalid, the function will fail and return NULL.
 @param flg The TOON read options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by TOON reader.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return A new TOON document, or NULL if an error occurs.
    When it's no longer needed, it should be freed with `ctoon_doc_free()`.

 @warning On 32-bit operating system, files larger than 2GB may fail to read.
 */
ctoon_api ctoon_doc *ctoon_read_fp(FILE *fp,
                                      ctoon_read_flag flg,
                                      const ctoon_alc *alc,
                                      ctoon_read_err *err);

/**
 Read a TOON string.

 This function is thread-safe.

 @param dat The TOON data (UTF-8 without BOM), null-terminator is not required.
    If this parameter is NULL, the function will fail and return NULL.
 @param len The length of TOON data in bytes.
    If this parameter is 0, the function will fail and return NULL.
 @param flg The TOON read options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @return A new TOON document, or NULL if an error occurs.
    When it's no longer needed, it should be freed with `ctoon_doc_free()`.
 */
ctoon_api_inline ctoon_doc *ctoon_read(const char *dat,
                                          size_t len,
                                          ctoon_read_flag flg) {
    flg &= ~CTOON_READ_INSITU; /* const string cannot be modified */
    return ctoon_read_opts((char *)(void *)(size_t)(const void *)dat,
                            len, flg, NULL, NULL);
}



#if !defined(CTOON_DISABLE_INCR_READER) || !CTOON_DISABLE_INCR_READER

/** Opaque state for incremental TOON reader. */
typedef struct ctoon_incr_state ctoon_incr_state;

/**
 Initialize state for incremental read.

 To read a large TOON document incrementally:
 1. Call `ctoon_incr_new()` to create the state for incremental reading.
 2. Call `ctoon_incr_read()` repeatedly.
 3. Call `ctoon_incr_free()` to free the state.

 Note: The incremental TOON reader only supports standard TOON.
 Flags for non-standard features (e.g. comments, trailing commas) are ignored.

 @param buf The TOON data, null-terminator is not required.
    If this parameter is NULL, the function will fail and return NULL.
 @param buf_len The length of the TOON data in `buf`.
    If use `CTOON_READ_INSITU`, `buf_len` should not include the padding size.
 @param flg The TOON read options.
    Multiple options can be combined with `|` operator.
 @param alc The memory allocator used by TOON reader.
    Pass NULL to use the libc's default allocator.
 @return A state for incremental reading.
    It should be freed with `ctoon_incr_free()`.
    NULL is returned if memory allocation fails.
*/
ctoon_api ctoon_incr_state *ctoon_incr_new(char *buf, size_t buf_len,
                                              ctoon_read_flag flg,
                                              const ctoon_alc *alc);

/**
 Performs incremental read of up to `len` bytes.

 If NULL is returned and `err->code` is set to `CTOON_READ_ERROR_MORE`, it
 indicates that more data is required to continue parsing. Then, call this
 function again with incremented `len`. Continue until a document is returned or
 an error other than `CTOON_READ_ERROR_MORE` is returned.

 Note: Parsing in very small increments is not efficient. An increment of
 several kilobytes or megabytes is recommended.

 @param state The state for incremental reading, created using
    `ctoon_incr_new()`.
 @param len The number of bytes of TOON data available to parse.
    If this parameter is 0, the function will fail and return NULL.
 @param err A pointer to receive error information.
 @return A new TOON document, or NULL if an error occurs.
    When the document is no longer needed, it should be freed with
    `ctoon_doc_free()`.
*/
ctoon_api ctoon_doc *ctoon_incr_read(ctoon_incr_state *state, size_t len,
                                        ctoon_read_err *err);

/** Release the incremental read state and free the memory. */
ctoon_api void ctoon_incr_free(ctoon_incr_state *state);

#endif /* CTOON_DISABLE_INCR_READER */

/**
 Returns the size of maximum memory usage to read a TOON data.

 You may use this value to avoid malloc() or calloc() call inside the reader
 to get better performance, or read multiple TOON with one piece of memory.

 @param len The length of TOON data in bytes.
 @param flg The TOON read options.
 @return The maximum memory size to read this TOON, or 0 if overflow.

 @b Example
 @code
    // read multiple TOON with same pre-allocated memory

    char *dat1, *dat2, *dat3; // TOON data
    size_t len1, len2, len3; // TOON length
    size_t max_len = MAX(len1, MAX(len2, len3));
    ctoon_doc *doc;

    // use one allocator for multiple TOON
    size_t size = ctoon_read_max_memory_usage(max_len, 0);
    void *buf = malloc(size);
    ctoon_alc alc;
    ctoon_alc_pool_init(&alc, buf, size);

    // no more alloc() or realloc() call during reading
    doc = ctoon_read_opts(dat1, len1, 0, &alc, NULL);
    ctoon_doc_free(doc);
    doc = ctoon_read_opts(dat2, len2, 0, &alc, NULL);
    ctoon_doc_free(doc);
    doc = ctoon_read_opts(dat3, len3, 0, &alc, NULL);
    ctoon_doc_free(doc);

    free(buf);
 @endcode
 @see ctoon_alc_pool_init()
 */
ctoon_api_inline size_t ctoon_read_max_memory_usage(size_t len,
                                                      ctoon_read_flag flg) {
    /*
     1. The max value count is (json_size / 2 + 1),
        for example: "[1,2,3,4]" size is 9, value count is 5.
     2. Some broken TOON may cost more memory during reading, but fail at end,
        for example: "[[[[[[[[".
     3. ctoon use 16 bytes per value, see struct ctoon_val.
     4. ctoon use dynamic memory with a growth factor of 1.5.

     The max memory size is (json_size / 2 * 16 * 1.5 + padding).
     */
    size_t mul = (size_t)12 + !(flg & CTOON_READ_INSITU);
    size_t pad = 256;
    size_t max = (size_t)(~(size_t)0);
    if (flg & CTOON_READ_STOP_WHEN_DONE) len = len < 256 ? 256 : len;
    if (len >= (max - pad - mul) / mul) return 0;
    return len * mul + pad;
}

/**
 Read a TOON number.

 This function is thread-safe when data is not modified by other threads.

 @param dat The TOON data (UTF-8 without BOM), null-terminator is required.
    If this parameter is NULL, the function will fail and return NULL.
 @param val The output value where result is stored.
    If this parameter is NULL, the function will fail and return NULL.
    The value will hold either UINT or SINT or REAL number;
 @param flg The TOON read options.
    Multiple options can be combined with `|` operator. 0 means no options.
    Supports `CTOON_READ_NUMBER_AS_RAW` and `CTOON_READ_ALLOW_INF_AND_NAN`.
 @param alc The memory allocator used for long number.
    It is only used when the built-in floating point reader is disabled.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return If successful, a pointer to the character after the last character
    used in the conversion, NULL if an error occurs.
 */
ctoon_api const char *ctoon_read_number(const char *dat,
                                          ctoon_val *val,
                                          ctoon_read_flag flg,
                                          const ctoon_alc *alc,
                                          ctoon_read_err *err);

/** Same as `ctoon_read_number()`. */
ctoon_api_inline const char *ctoon_mut_read_number(const char *dat,
                                                     ctoon_mut_val *val,
                                                     ctoon_read_flag flg,
                                                     const ctoon_alc *alc,
                                                     ctoon_read_err *err) {
    return ctoon_read_number(dat, (ctoon_val *)val, flg, alc, err);
}

#endif /* CTOON_DISABLE_READER */



/*==============================================================================
 * MARK: - TOON Writer API
 *============================================================================*/

/** Run-time flags for TOON writer. */
typedef uint32_t ctoon_write_flag;

/** Default: write canonical TOON, report error on inf/nan, no unicode escape. */
static const ctoon_write_flag CTOON_WRITE_NOFLAG                  = 0;

/** Escape unicode as `\uXXXX`, making output ASCII-only. */
static const ctoon_write_flag CTOON_WRITE_ESCAPE_UNICODE          = 1 << 1;

/** Escape '/' as '\/'. */
static const ctoon_write_flag CTOON_WRITE_ESCAPE_SLASHES          = 1 << 2;

/** Write inf and nan as 'Infinity' and 'NaN' literals (non-standard). */
static const ctoon_write_flag CTOON_WRITE_ALLOW_INF_AND_NAN       = 1 << 3;

/** Write inf and nan as null literal (overrides ALLOW_INF_AND_NAN). */
static const ctoon_write_flag CTOON_WRITE_INF_AND_NAN_AS_NULL     = 1 << 4;

/** Allow invalid unicode bytes to be copied as-is. */
static const ctoon_write_flag CTOON_WRITE_ALLOW_INVALID_UNICODE   = 1 << 5;

/** Prefix array lengths with '#': items[#3]: ... */
static const ctoon_write_flag CTOON_WRITE_LENGTH_MARKER           = 1 << 6;

/** Add a trailing newline at the end of output. */
static const ctoon_write_flag CTOON_WRITE_NEWLINE_AT_END          = 1 << 7;

/**
 * Highest 8 bits of ctoon_write_flag are reserved for floating-point format
 * control (same encoding as yyjson for ABI compatibility).
 */
#define CTOON_WRITE_FP_FLAG_BITS  8
#define CTOON_WRITE_FP_PREC_BITS  4

/** Write floating-point as fixed notation with `prec` decimal places (1-15). */
#define CTOON_WRITE_FP_TO_FIXED(prec) ((ctoon_write_flag)( \
    (uint32_t)((uint32_t)(prec)) << (32 - 4) ))

/** Write floating-point as single-precision float. */
#define CTOON_WRITE_FP_TO_FLOAT ((ctoon_write_flag)(1 << (32 - 5)))


/** Array value delimiter used during encoding. */
typedef enum ctoon_delimiter {
    CTOON_DELIMITER_COMMA = 0, /**< , (default) */
    CTOON_DELIMITER_TAB,       /**< \t            */
    CTOON_DELIMITER_PIPE       /**< |             */
} ctoon_delimiter;

/** Result codes for the TOON writer. */
typedef uint32_t ctoon_write_code;
static const ctoon_write_code CTOON_WRITE_SUCCESS                     = 0;
static const ctoon_write_code CTOON_WRITE_ERROR_INVALID_PARAMETER     = 1;
static const ctoon_write_code CTOON_WRITE_ERROR_MEMORY_ALLOCATION     = 2;
static const ctoon_write_code CTOON_WRITE_ERROR_INVALID_VALUE_TYPE    = 3;
static const ctoon_write_code CTOON_WRITE_ERROR_NAN_OR_INF            = 4;
static const ctoon_write_code CTOON_WRITE_ERROR_FILE_OPEN             = 5;
static const ctoon_write_code CTOON_WRITE_ERROR_FILE_WRITE            = 6;
static const ctoon_write_code CTOON_WRITE_ERROR_INVALID_STRING        = 7;

/** Error information for TOON writer. */
typedef struct ctoon_write_err {
    ctoon_write_code code; /**< Error code (0 = success). */
    const char      *msg;  /**< Constant error message, NULL on success. */
} ctoon_write_err;

/** Full options for ctoon_write_opts / ctoon_val_write_opts. */
typedef struct ctoon_write_options {
    ctoon_write_flag  flag;      /**< CTOON_WRITE_* flags (no indent here). */
    ctoon_delimiter   delimiter; /**< Array value delimiter.                 */
    int               indent;    /**< Spaces per indent level (0 = minify, default 2). */
} ctoon_write_options;


#if !defined(CTOON_DISABLE_WRITER) || !CTOON_DISABLE_WRITER

/**
 Write a document to TOON string with full options.
 @param doc  The TOON document.
 @param opts Write options (NULL = defaults: indent=2, comma delimiter).
 @param alc  Allocator, or NULL for libc default.
 @param len  Receives output byte length (not including null-terminator); may be NULL.
 @param err  Receives error info; may be NULL.
 @return Heap-allocated null-terminated TOON string, or NULL on error.
         Caller must free() the result (or use alc->free).
 */
ctoon_api char *ctoon_write_opts(const ctoon_doc *doc,
                                  const ctoon_write_options *opts,
                                  const ctoon_alc *alc,
                                  size_t *len,
                                  ctoon_write_err *err);

/** Write a document with default options (indent=2, comma delimiter). */
ctoon_api_inline char *ctoon_write(const ctoon_doc *doc, size_t *len) {
    return ctoon_write_opts(doc, NULL, NULL, len, NULL);
}

/** Write a document to file with full options. */
ctoon_api bool ctoon_write_file(const char *path,
                                 const ctoon_doc *doc,
                                 const ctoon_write_options *opts,
                                 const ctoon_alc *alc,
                                 ctoon_write_err *err);

/** Write a document to file pointer with full options. */
ctoon_api bool ctoon_write_fp(FILE *fp,
                               const ctoon_doc *doc,
                               const ctoon_write_options *opts,
                               const ctoon_alc *alc,
                               ctoon_write_err *err);

/** Write a single value (not a full document) to TOON string with options. */
ctoon_api char *ctoon_val_write_opts(const ctoon_val *val,
                                      const ctoon_write_options *opts,
                                      const ctoon_alc *alc,
                                      size_t *len,
                                      ctoon_write_err *err);

ctoon_api_inline char *ctoon_val_write(const ctoon_val *val, size_t *len) {
    return ctoon_val_write_opts(val, NULL, NULL, len, NULL);
}

/** Write a mutable document to TOON string with full options. */
ctoon_api char *ctoon_mut_write_opts(const ctoon_mut_doc *doc,
                                      const ctoon_write_options *opts,
                                      const ctoon_alc *alc,
                                      size_t *len,
                                      ctoon_write_err *err);

ctoon_api_inline char *ctoon_mut_write(const ctoon_mut_doc *doc, size_t *len) {
    return ctoon_mut_write_opts(doc, NULL, NULL, len, NULL);
}

ctoon_api bool ctoon_mut_write_file(const char *path,
                                     const ctoon_mut_doc *doc,
                                     const ctoon_write_options *opts,
                                     const ctoon_alc *alc,
                                     ctoon_write_err *err);

ctoon_api bool ctoon_mut_write_fp(FILE *fp,
                                   const ctoon_mut_doc *doc,
                                   const ctoon_write_options *opts,
                                   const ctoon_alc *alc,
                                   ctoon_write_err *err);

ctoon_api char *ctoon_mut_val_write_opts(const ctoon_mut_val *val,
                                          const ctoon_write_options *opts,
                                          const ctoon_alc *alc,
                                          size_t *len,
                                          ctoon_write_err *err);

ctoon_api_inline char *ctoon_mut_val_write(const ctoon_mut_val *val,
                                            size_t *len) {
    return ctoon_mut_val_write_opts(val, NULL, NULL, len, NULL);
}

/** Write a TOON number value to a buffer (at least 40 bytes). */
ctoon_api char *ctoon_write_number(const ctoon_val *val, char *buf);

/**
 * Serialize a document to a JSON string.
 *
 * The flags parameter accepts the same \c CTOON_WRITE_* flags used by
 * the TOON writer (e.g. \c CTOON_WRITE_ESCAPE_UNICODE,
 * \c CTOON_WRITE_ALLOW_INF_AND_NAN, \c CTOON_WRITE_INF_AND_NAN_AS_NULL).
 * Note: flags that are TOON-specific (e.g. \c CTOON_WRITE_LENGTH_MARKER)
 * are silently ignored.
 *
 * @param doc    The document to serialize. Must not be NULL.
 * @param indent Spaces per indent level. 0 = compact (no whitespace).
 * @param flags  \c CTOON_WRITE_* flags (subset shared with JSON).
 * @param alc    Custom allocator, or NULL for the default.
 * @param len    If not NULL, receives the byte length of the result
 *               (not counting the null terminator).
 * @param err    If not NULL, receives error details on failure.
 * @return       Heap-allocated null-terminated JSON string, or NULL on
 *               failure. The caller must free() the returned pointer.
 */
ctoon_api char *ctoon_doc_to_json(const ctoon_doc *doc,
                                   int indent,
                                   ctoon_write_flag flags,
                                   const ctoon_alc *alc,
                                   size_t *len,
                                   ctoon_write_err *err);

/** Same as ctoon_doc_to_json() but operates on a mutable document. */
ctoon_api char *ctoon_mut_doc_to_json(const ctoon_mut_doc *doc,
                                       int indent,
                                       ctoon_write_flag flags,
                                       const ctoon_alc *alc,
                                       size_t *len,
                                       ctoon_write_err *err);

#endif /* CTOON_DISABLE_WRITER */



/*==============================================================================
 * MARK: - TOON Document API
 *============================================================================*/

/** Returns the root value of this TOON document.
    Returns NULL if `doc` is NULL. */
ctoon_api_inline ctoon_val *ctoon_doc_get_root(ctoon_doc *doc);

/** Returns read size of input TOON data.
    Returns 0 if `doc` is NULL.
    For example: the read size of `[1,2,3]` is 7 bytes.  */
ctoon_api_inline size_t ctoon_doc_get_read_size(ctoon_doc *doc);

/** Returns total value count in this TOON document.
    Returns 0 if `doc` is NULL.
    For example: the value count of `[1,2,3]` is 4. */
ctoon_api_inline size_t ctoon_doc_get_val_count(ctoon_doc *doc);

/** Release the TOON document and free the memory.
    After calling this function, the `doc` and all values from the `doc` are no
    longer available. This function will do nothing if the `doc` is NULL. */
ctoon_api_inline void ctoon_doc_free(ctoon_doc *doc);



/*==============================================================================
 * MARK: - TOON Value Type API
 *============================================================================*/

/** Returns whether the TOON value is raw.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_raw(ctoon_val *val);

/** Returns whether the TOON value is `null`.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_null(ctoon_val *val);

/** Returns whether the TOON value is `true`.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_true(ctoon_val *val);

/** Returns whether the TOON value is `false`.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_false(ctoon_val *val);

/** Returns whether the TOON value is bool (true/false).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_bool(ctoon_val *val);

/** Returns whether the TOON value is unsigned integer (uint64_t).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_uint(ctoon_val *val);

/** Returns whether the TOON value is signed integer (int64_t).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_sint(ctoon_val *val);

/** Returns whether the TOON value is integer (uint64_t/int64_t).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_int(ctoon_val *val);

/** Returns whether the TOON value is real number (double).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_real(ctoon_val *val);

/** Returns whether the TOON value is number (uint64_t/int64_t/double).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_num(ctoon_val *val);

/** Returns whether the TOON value is string.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_str(ctoon_val *val);

/** Returns whether the TOON value is array.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_arr(ctoon_val *val);

/** Returns whether the TOON value is object.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_obj(ctoon_val *val);

/** Returns whether the TOON value is container (array/object).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_is_ctn(ctoon_val *val);



/*==============================================================================
 * MARK: - TOON Value Content API
 *============================================================================*/

/** Returns the TOON value's type.
    Returns CTOON_TYPE_NONE if `val` is NULL. */
ctoon_api_inline ctoon_type ctoon_get_type(ctoon_val *val);

/** Returns the TOON value's subtype.
    Returns CTOON_SUBTYPE_NONE if `val` is NULL. */
ctoon_api_inline ctoon_subtype ctoon_get_subtype(ctoon_val *val);

/** Returns the TOON value's tag.
    Returns 0 if `val` is NULL. */
ctoon_api_inline uint8_t ctoon_get_tag(ctoon_val *val);

/** Returns the TOON value's type description.
    The return value should be one of these strings: "raw", "null", "string",
    "array", "object", "true", "false", "uint", "sint", "real", "unknown". */
ctoon_api_inline const char *ctoon_get_type_desc(ctoon_val *val);

/** Returns the content if the value is raw.
    Returns NULL if `val` is NULL or type is not raw. */
ctoon_api_inline const char *ctoon_get_raw(ctoon_val *val);

/** Returns the content if the value is bool.
    Returns false if `val` is NULL or type is not bool. */
ctoon_api_inline bool ctoon_get_bool(ctoon_val *val);

/** Returns the content and cast to uint64_t.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
ctoon_api_inline uint64_t ctoon_get_uint(ctoon_val *val);

/** Returns the content and cast to int64_t.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
ctoon_api_inline int64_t ctoon_get_sint(ctoon_val *val);

/** Returns the content and cast to int.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
ctoon_api_inline int ctoon_get_int(ctoon_val *val);

/** Returns the content if the value is real number, or 0.0 on error.
    Returns 0.0 if `val` is NULL or type is not real(double). */
ctoon_api_inline double ctoon_get_real(ctoon_val *val);

/** Returns the content and typecast to `double` if the value is number.
    Returns 0.0 if `val` is NULL or type is not number(uint/sint/real). */
ctoon_api_inline double ctoon_get_num(ctoon_val *val);

/** Returns the content if the value is string.
    Returns NULL if `val` is NULL or type is not string. */
ctoon_api_inline const char *ctoon_get_str(ctoon_val *val);

/** Returns the content length (string length, array size, object size.
    Returns 0 if `val` is NULL or type is not string/array/object. */
ctoon_api_inline size_t ctoon_get_len(ctoon_val *val);

/** Returns whether the TOON value is equals to a string.
    Returns false if input is NULL or type is not string. */
ctoon_api_inline bool ctoon_equals_str(ctoon_val *val, const char *str);

/** Returns whether the TOON value is equals to a string.
    The `str` should be a UTF-8 string, null-terminator is not required.
    Returns false if input is NULL or type is not string. */
ctoon_api_inline bool ctoon_equals_strn(ctoon_val *val, const char *str,
                                          size_t len);

/** Returns whether two TOON values are equal (deep compare).
    Returns false if input is NULL.
    @note the result may be inaccurate if object has duplicate keys.
    @warning This function is recursive and may cause a stack overflow
        if the object level is too deep. */
ctoon_api_inline bool ctoon_equals(ctoon_val *lhs, ctoon_val *rhs);

/** Set the value to raw.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_raw(ctoon_val *val,
                                      const char *raw, size_t len);

/** Set the value to null.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_null(ctoon_val *val);

/** Set the value to bool.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_bool(ctoon_val *val, bool num);

/** Set the value to uint.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_uint(ctoon_val *val, uint64_t num);

/** Set the value to sint.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_sint(ctoon_val *val, int64_t num);

/** Set the value to int.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_int(ctoon_val *val, int num);

/** Set the value to float.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_float(ctoon_val *val, float num);

/** Set the value to double.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_double(ctoon_val *val, double num);

/** Set the value to real.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_real(ctoon_val *val, double num);

/** Set the floating-point number's output format to fixed-point notation.
    Returns false if input is NULL or `val` is not real type.
    @see CTOON_WRITE_FP_TO_FIXED flag.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_fp_to_fixed(ctoon_val *val, int prec);

/** Set the floating-point number's output format to single-precision.
    Returns false if input is NULL or `val` is not real type.
    @see CTOON_WRITE_FP_TO_FLOAT flag.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_fp_to_float(ctoon_val *val, bool flt);

/** Set the value to string (null-terminated).
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_str(ctoon_val *val, const char *str);

/** Set the value to string (with length).
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_strn(ctoon_val *val,
                                       const char *str, size_t len);

/** Marks this string as not needing to be escaped during TOON writing.
    This can be used to avoid the overhead of escaping if the string contains
    only characters that do not require escaping.
    Returns false if input is NULL or `val` is not string.
    @see CTOON_SUBTYPE_NOESC subtype.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_set_str_noesc(ctoon_val *val, bool noesc);



/*==============================================================================
 * MARK: - TOON Array API
 *============================================================================*/

/** Returns the number of elements in this array.
    Returns 0 if `arr` is NULL or type is not array. */
ctoon_api_inline size_t ctoon_arr_size(ctoon_val *arr);

/** Returns the element at the specified position in this array.
    Returns NULL if array is NULL/empty or the index is out of bounds.
    @warning This function takes a linear search time if array is not flat.
        For example: `[1,{},3]` is flat, `[1,[2],3]` is not flat. */
ctoon_api_inline ctoon_val *ctoon_arr_get(ctoon_val *arr, size_t idx);

/** Returns the first element of this array.
    Returns NULL if `arr` is NULL/empty or type is not array. */
ctoon_api_inline ctoon_val *ctoon_arr_get_first(ctoon_val *arr);

/** Returns the last element of this array.
    Returns NULL if `arr` is NULL/empty or type is not array.
    @warning This function takes a linear search time if array is not flat.
        For example: `[1,{},3]` is flat, `[1,[2],3]` is not flat.*/
ctoon_api_inline ctoon_val *ctoon_arr_get_last(ctoon_val *arr);



/*==============================================================================
 * MARK: - TOON Array Iterator API
 *============================================================================*/

/**
 A TOON array iterator.

 @b Example
 @code
    ctoon_val *val;
    ctoon_arr_iter iter = ctoon_arr_iter_with(arr);
    while ((val = ctoon_arr_iter_next(&iter))) {
        your_func(val);
    }
 @endcode
 */
typedef struct ctoon_arr_iter {
    size_t idx; /**< next value's index */
    size_t max; /**< maximum index (arr.size) */
    ctoon_val *cur; /**< next value */
} ctoon_arr_iter;

/**
 Initialize an iterator for this array.

 @param arr The array to be iterated over.
    If this parameter is NULL or not an array, `iter` will be set to empty.
 @param iter The iterator to be initialized.
    If this parameter is NULL, the function will fail and return false.
 @return true if the `iter` has been successfully initialized.

 @note The iterator does not need to be destroyed.
 */
ctoon_api_inline bool ctoon_arr_iter_init(ctoon_val *arr,
                                            ctoon_arr_iter *iter);

/**
 Create an iterator with an array , same as `ctoon_arr_iter_init()`.

 @param arr The array to be iterated over.
    If this parameter is NULL or not an array, an empty iterator will returned.
 @return A new iterator for the array.

 @note The iterator does not need to be destroyed.
 */
ctoon_api_inline ctoon_arr_iter ctoon_arr_iter_with(ctoon_val *arr);

/**
 Returns whether the iteration has more elements.
 If `iter` is NULL, this function will return false.
 */
ctoon_api_inline bool ctoon_arr_iter_has_next(ctoon_arr_iter *iter);

/**
 Returns the next element in the iteration, or NULL on end.
 If `iter` is NULL, this function will return NULL.
 */
ctoon_api_inline ctoon_val *ctoon_arr_iter_next(ctoon_arr_iter *iter);

/**
 Macro for iterating over an array.
 It works like iterator, but with a more intuitive API.

 @b Example
 @code
    size_t idx, max;
    ctoon_val *val;
    ctoon_arr_foreach(arr, idx, max, val) {
        your_func(idx, val);
    }
 @endcode
 */
#define ctoon_arr_foreach(arr, idx, max, val) \
    for ((idx) = 0, \
        (max) = ctoon_arr_size(arr), \
        (val) = ctoon_arr_get_first(arr); \
        (idx) < (max); \
        (idx)++, \
        (val) = unsafe_ctoon_get_next(val))



/*==============================================================================
 * MARK: - TOON Object API
 *============================================================================*/

/** Returns the number of key-value pairs in this object.
    Returns 0 if `obj` is NULL or type is not object. */
ctoon_api_inline size_t ctoon_obj_size(ctoon_val *obj);

/** Returns the value to which the specified key is mapped.
    Returns NULL if this object contains no mapping for the key.
    Returns NULL if `obj/key` is NULL, or type is not object.

    The `key` should be a null-terminated UTF-8 string.

    @warning This function takes a linear search time. */
ctoon_api_inline ctoon_val *ctoon_obj_get(ctoon_val *obj, const char *key);

/** Returns the value to which the specified key is mapped.
    Returns NULL if this object contains no mapping for the key.
    Returns NULL if `obj/key` is NULL, or type is not object.

    The `key` should be a UTF-8 string, null-terminator is not required.
    The `key_len` should be the length of the key, in bytes.

    @warning This function takes a linear search time. */
ctoon_api_inline ctoon_val *ctoon_obj_getn(ctoon_val *obj, const char *key,
                                              size_t key_len);



/*==============================================================================
 * MARK: - TOON Object Iterator API
 *============================================================================*/

/**
 A TOON object iterator.

 @b Example
 @code
    ctoon_val *key, *val;
    ctoon_obj_iter iter = ctoon_obj_iter_with(obj);
    while ((key = ctoon_obj_iter_next(&iter))) {
        val = ctoon_obj_iter_get_val(key);
        your_func(key, val);
    }
 @endcode

 If the ordering of the keys is known at compile-time, you can use this method
 to speed up value lookups:
 @code
    // {"k1":1, "k2": 3, "k3": 3}
    ctoon_val *key, *val;
    ctoon_obj_iter iter = ctoon_obj_iter_with(obj);
    ctoon_val *v1 = ctoon_obj_iter_get(&iter, "k1");
    ctoon_val *v3 = ctoon_obj_iter_get(&iter, "k3");
 @endcode
 @see ctoon_obj_iter_get() and ctoon_obj_iter_getn()
 */
typedef struct ctoon_obj_iter {
    size_t idx; /**< next key's index */
    size_t max; /**< maximum key index (obj.size) */
    ctoon_val *cur; /**< next key */
    ctoon_val *obj; /**< the object being iterated */
} ctoon_obj_iter;

/**
 Initialize an iterator for this object.

 @param obj The object to be iterated over.
    If this parameter is NULL or not an object, `iter` will be set to empty.
 @param iter The iterator to be initialized.
    If this parameter is NULL, the function will fail and return false.
 @return true if the `iter` has been successfully initialized.

 @note The iterator does not need to be destroyed.
 */
ctoon_api_inline bool ctoon_obj_iter_init(ctoon_val *obj,
                                            ctoon_obj_iter *iter);

/**
 Create an iterator with an object, same as `ctoon_obj_iter_init()`.

 @param obj The object to be iterated over.
    If this parameter is NULL or not an object, an empty iterator will returned.
 @return A new iterator for the object.

 @note The iterator does not need to be destroyed.
 */
ctoon_api_inline ctoon_obj_iter ctoon_obj_iter_with(ctoon_val *obj);

/**
 Returns whether the iteration has more elements.
 If `iter` is NULL, this function will return false.
 */
ctoon_api_inline bool ctoon_obj_iter_has_next(ctoon_obj_iter *iter);

/**
 Returns the next key in the iteration, or NULL on end.
 If `iter` is NULL, this function will return NULL.
 */
ctoon_api_inline ctoon_val *ctoon_obj_iter_next(ctoon_obj_iter *iter);

/**
 Returns the value for key inside the iteration.
 If `iter` is NULL, this function will return NULL.
 */
ctoon_api_inline ctoon_val *ctoon_obj_iter_get_val(ctoon_val *key);

/**
 Iterates to a specified key and returns the value.

 This function does the same thing as `ctoon_obj_get()`, but is much faster
 if the ordering of the keys is known at compile-time and you are using the same
 order to look up the values. If the key exists in this object, then the
 iterator will stop at the next key, otherwise the iterator will not change and
 NULL is returned.

 @param iter The object iterator, should not be NULL.
 @param key The key, should be a UTF-8 string with null-terminator.
 @return The value to which the specified key is mapped.
    NULL if this object contains no mapping for the key or input is invalid.

 @warning This function takes a linear search time if the key is not nearby.
 */
ctoon_api_inline ctoon_val *ctoon_obj_iter_get(ctoon_obj_iter *iter,
                                                  const char *key);

/**
 Iterates to a specified key and returns the value.

 This function does the same thing as `ctoon_obj_getn()`, but is much faster
 if the ordering of the keys is known at compile-time and you are using the same
 order to look up the values. If the key exists in this object, then the
 iterator will stop at the next key, otherwise the iterator will not change and
 NULL is returned.

 @param iter The object iterator, should not be NULL.
 @param key The key, should be a UTF-8 string, null-terminator is not required.
 @param key_len The the length of `key`, in bytes.
 @return The value to which the specified key is mapped.
    NULL if this object contains no mapping for the key or input is invalid.

 @warning This function takes a linear search time if the key is not nearby.
 */
ctoon_api_inline ctoon_val *ctoon_obj_iter_getn(ctoon_obj_iter *iter,
                                                   const char *key,
                                                   size_t key_len);

/**
 Macro for iterating over an object.
 It works like iterator, but with a more intuitive API.

 @b Example
 @code
    size_t idx, max;
    ctoon_val *key, *val;
    ctoon_obj_foreach(obj, idx, max, key, val) {
        your_func(key, val);
    }
 @endcode
 */
#define ctoon_obj_foreach(obj, idx, max, key, val) \
    for ((idx) = 0, \
        (max) = ctoon_obj_size(obj), \
        (key) = (obj) ? unsafe_ctoon_get_first(obj) : NULL, \
        (val) = (key) + 1; \
        (idx) < (max); \
        (idx)++, \
        (key) = unsafe_ctoon_get_next(val), \
        (val) = (key) + 1)



/*==============================================================================
 * MARK: - Mutable TOON Document API
 *============================================================================*/

/** Returns the root value of this TOON document.
    Returns NULL if `doc` is NULL. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_get_root(ctoon_mut_doc *doc);

/** Sets the root value of this TOON document.
    Pass NULL to clear root value of the document. */
ctoon_api_inline void ctoon_mut_doc_set_root(ctoon_mut_doc *doc,
                                               ctoon_mut_val *root);

/**
 Set the string pool size for a mutable document.
 This function does not allocate memory immediately, but uses the size when
 the next memory allocation is needed.

 If the caller knows the approximate bytes of strings that the document needs to
 store (e.g. copy string with `ctoon_mut_strcpy` function), setting a larger
 size can avoid multiple memory allocations and improve performance.

 @param doc The mutable document.
 @param len The desired string pool size in bytes (total string length).
 @return true if successful, false if size is 0 or overflow.
 */
ctoon_api bool ctoon_mut_doc_set_str_pool_size(ctoon_mut_doc *doc,
                                                 size_t len);

/**
 Set the value pool size for a mutable document.
 This function does not allocate memory immediately, but uses the size when
 the next memory allocation is needed.

 If the caller knows the approximate number of values that the document needs to
 store (e.g. create new value with `ctoon_mut_xxx` functions), setting a larger
 size can avoid multiple memory allocations and improve performance.

 @param doc The mutable document.
 @param count The desired value pool size (number of `ctoon_mut_val`).
 @return true if successful, false if size is 0 or overflow.
 */
ctoon_api bool ctoon_mut_doc_set_val_pool_size(ctoon_mut_doc *doc,
                                                 size_t count);

/** Release the TOON document and free the memory.
    After calling this function, the `doc` and all values from the `doc` are no
    longer available. This function will do nothing if the `doc` is NULL.  */
ctoon_api void ctoon_mut_doc_free(ctoon_mut_doc *doc);

/** Creates and returns a new mutable TOON document, returns NULL on error.
    If allocator is NULL, the default allocator will be used. */
ctoon_api ctoon_mut_doc *ctoon_mut_doc_new(const ctoon_alc *alc);

/** Copies and returns a new mutable document from input, returns NULL on error.
    This makes a `deep-copy` on the immutable document.
    If allocator is NULL, the default allocator will be used.
    @note `imut_doc` -> `mut_doc`. */
ctoon_api ctoon_mut_doc *ctoon_doc_mut_copy(ctoon_doc *doc,
                                               const ctoon_alc *alc);

/** Copies and returns a new mutable document from input, returns NULL on error.
    This makes a `deep-copy` on the mutable document.
    If allocator is NULL, the default allocator will be used.
    @note `mut_doc` -> `mut_doc`. */
ctoon_api ctoon_mut_doc *ctoon_mut_doc_mut_copy(ctoon_mut_doc *doc,
                                                   const ctoon_alc *alc);

/** Copies and returns a new mutable value from input, returns NULL on error.
    This makes a `deep-copy` on the immutable value.
    The memory was managed by mutable document.
    @note `imut_val` -> `mut_val`. */
ctoon_api ctoon_mut_val *ctoon_val_mut_copy(ctoon_mut_doc *doc,
                                               ctoon_val *val);

/** Copies and returns a new mutable value from input, returns NULL on error.
    This makes a `deep-copy` on the mutable value.
    The memory was managed by mutable document.
    @note `mut_val` -> `mut_val`.
    @warning This function is recursive and may cause a stack overflow
        if the object level is too deep. */
ctoon_api ctoon_mut_val *ctoon_mut_val_mut_copy(ctoon_mut_doc *doc,
                                                   ctoon_mut_val *val);

/** Copies and returns a new immutable document from input,
    returns NULL on error. This makes a `deep-copy` on the mutable document.
    The returned document should be freed with `ctoon_doc_free()`.
    @note `mut_doc` -> `imut_doc`.
    @warning This function is recursive and may cause a stack overflow
        if the object level is too deep. */
ctoon_api ctoon_doc *ctoon_mut_doc_imut_copy(ctoon_mut_doc *doc,
                                                const ctoon_alc *alc);

/** Copies and returns a new immutable document from input,
    returns NULL on error. This makes a `deep-copy` on the mutable value.
    The returned document should be freed with `ctoon_doc_free()`.
    @note `mut_val` -> `imut_doc`.
    @warning This function is recursive and may cause a stack overflow
        if the object level is too deep. */
ctoon_api ctoon_doc *ctoon_mut_val_imut_copy(ctoon_mut_val *val,
                                                const ctoon_alc *alc);



/*==============================================================================
 * MARK: - Mutable TOON Value Type API
 *============================================================================*/

/** Returns whether the TOON value is raw.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_raw(ctoon_mut_val *val);

/** Returns whether the TOON value is `null`.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_null(ctoon_mut_val *val);

/** Returns whether the TOON value is `true`.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_true(ctoon_mut_val *val);

/** Returns whether the TOON value is `false`.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_false(ctoon_mut_val *val);

/** Returns whether the TOON value is bool (true/false).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_bool(ctoon_mut_val *val);

/** Returns whether the TOON value is unsigned integer (uint64_t).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_uint(ctoon_mut_val *val);

/** Returns whether the TOON value is signed integer (int64_t).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_sint(ctoon_mut_val *val);

/** Returns whether the TOON value is integer (uint64_t/int64_t).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_int(ctoon_mut_val *val);

/** Returns whether the TOON value is real number (double).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_real(ctoon_mut_val *val);

/** Returns whether the TOON value is number (uint/sint/real).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_num(ctoon_mut_val *val);

/** Returns whether the TOON value is string.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_str(ctoon_mut_val *val);

/** Returns whether the TOON value is array.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_arr(ctoon_mut_val *val);

/** Returns whether the TOON value is object.
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_obj(ctoon_mut_val *val);

/** Returns whether the TOON value is container (array/object).
    Returns false if `val` is NULL. */
ctoon_api_inline bool ctoon_mut_is_ctn(ctoon_mut_val *val);



/*==============================================================================
 * MARK: - Mutable TOON Value Content API
 *============================================================================*/

/** Returns the TOON value's type.
    Returns `CTOON_TYPE_NONE` if `val` is NULL. */
ctoon_api_inline ctoon_type ctoon_mut_get_type(ctoon_mut_val *val);

/** Returns the TOON value's subtype.
    Returns `CTOON_SUBTYPE_NONE` if `val` is NULL. */
ctoon_api_inline ctoon_subtype ctoon_mut_get_subtype(ctoon_mut_val *val);

/** Returns the TOON value's tag.
    Returns 0 if `val` is NULL. */
ctoon_api_inline uint8_t ctoon_mut_get_tag(ctoon_mut_val *val);

/** Returns the TOON value's type description.
    The return value should be one of these strings: "raw", "null", "string",
    "array", "object", "true", "false", "uint", "sint", "real", "unknown". */
ctoon_api_inline const char *ctoon_mut_get_type_desc(ctoon_mut_val *val);

/** Returns the content if the value is raw.
    Returns NULL if `val` is NULL or type is not raw. */
ctoon_api_inline const char *ctoon_mut_get_raw(ctoon_mut_val *val);

/** Returns the content if the value is bool.
    Returns NULL if `val` is NULL or type is not bool. */
ctoon_api_inline bool ctoon_mut_get_bool(ctoon_mut_val *val);

/** Returns the content and cast to uint64_t.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
ctoon_api_inline uint64_t ctoon_mut_get_uint(ctoon_mut_val *val);

/** Returns the content and cast to int64_t.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
ctoon_api_inline int64_t ctoon_mut_get_sint(ctoon_mut_val *val);

/** Returns the content and cast to int.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
ctoon_api_inline int ctoon_mut_get_int(ctoon_mut_val *val);

/** Returns the content if the value is real number.
    Returns 0.0 if `val` is NULL or type is not real(double). */
ctoon_api_inline double ctoon_mut_get_real(ctoon_mut_val *val);

/** Returns the content and typecast to `double` if the value is number.
    Returns 0.0 if `val` is NULL or type is not number(uint/sint/real). */
ctoon_api_inline double ctoon_mut_get_num(ctoon_mut_val *val);

/** Returns the content if the value is string.
    Returns NULL if `val` is NULL or type is not string. */
ctoon_api_inline const char *ctoon_mut_get_str(ctoon_mut_val *val);

/** Returns the content length (string length, array size, object size.
    Returns 0 if `val` is NULL or type is not string/array/object. */
ctoon_api_inline size_t ctoon_mut_get_len(ctoon_mut_val *val);

/** Returns whether the TOON value is equals to a string.
    The `str` should be a null-terminated UTF-8 string.
    Returns false if input is NULL or type is not string. */
ctoon_api_inline bool ctoon_mut_equals_str(ctoon_mut_val *val,
                                             const char *str);

/** Returns whether the TOON value is equals to a string.
    The `str` should be a UTF-8 string, null-terminator is not required.
    Returns false if input is NULL or type is not string. */
ctoon_api_inline bool ctoon_mut_equals_strn(ctoon_mut_val *val,
                                              const char *str, size_t len);

/** Returns whether two TOON values are equal (deep compare).
    Returns false if input is NULL.
    @note the result may be inaccurate if object has duplicate keys.
    @warning This function is recursive and may cause a stack overflow
        if the object level is too deep. */
ctoon_api_inline bool ctoon_mut_equals(ctoon_mut_val *lhs,
                                         ctoon_mut_val *rhs);

/** Set the value to raw.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_raw(ctoon_mut_val *val,
                                          const char *raw, size_t len);

/** Set the value to null.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_null(ctoon_mut_val *val);

/** Set the value to bool.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_bool(ctoon_mut_val *val, bool num);

/** Set the value to uint.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_uint(ctoon_mut_val *val, uint64_t num);

/** Set the value to sint.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_sint(ctoon_mut_val *val, int64_t num);

/** Set the value to int.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_int(ctoon_mut_val *val, int num);

/** Set the value to float.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_float(ctoon_mut_val *val, float num);

/** Set the value to double.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_double(ctoon_mut_val *val, double num);

/** Set the value to real.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_real(ctoon_mut_val *val, double num);

/** Set the floating-point number's output format to fixed-point notation.
    Returns false if input is NULL or `val` is not real type.
    @see CTOON_WRITE_FP_TO_FIXED flag.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_mut_set_fp_to_fixed(ctoon_mut_val *val,
                                                  int prec);

/** Set the floating-point number's output format to single-precision.
    Returns false if input is NULL or `val` is not real type.
    @see CTOON_WRITE_FP_TO_FLOAT flag.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_mut_set_fp_to_float(ctoon_mut_val *val,
                                                  bool flt);

/** Set the value to string (null-terminated).
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_str(ctoon_mut_val *val, const char *str);

/** Set the value to string (with length).
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_strn(ctoon_mut_val *val,
                                           const char *str, size_t len);

/** Marks this string as not needing to be escaped during TOON writing.
    This can be used to avoid the overhead of escaping if the string contains
    only characters that do not require escaping.
    Returns false if input is NULL or `val` is not string.
    @see CTOON_SUBTYPE_NOESC subtype.
    @warning This will modify the `immutable` value, use with caution. */
ctoon_api_inline bool ctoon_mut_set_str_noesc(ctoon_mut_val *val,
                                                bool noesc);

/** Set the value to array.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_arr(ctoon_mut_val *val);

/** Set the value to array.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
ctoon_api_inline bool ctoon_mut_set_obj(ctoon_mut_val *val);



/*==============================================================================
 * MARK: - Mutable TOON Value Creation API
 *============================================================================*/

/** Creates and returns a raw value, returns NULL on error.
    The `str` should be a null-terminated UTF-8 string.

    @warning The input string is not copied, you should keep this string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_raw(ctoon_mut_doc *doc,
                                                 const char *str);

/** Creates and returns a raw value, returns NULL on error.
    The `str` should be a UTF-8 string, null-terminator is not required.

    @warning The input string is not copied, you should keep this string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_rawn(ctoon_mut_doc *doc,
                                                  const char *str,
                                                  size_t len);

/** Creates and returns a raw value, returns NULL on error.
    The `str` should be a null-terminated UTF-8 string.
    The input string is copied and held by the document. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_rawcpy(ctoon_mut_doc *doc,
                                                    const char *str);

/** Creates and returns a raw value, returns NULL on error.
    The `str` should be a UTF-8 string, null-terminator is not required.
    The input string is copied and held by the document. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_rawncpy(ctoon_mut_doc *doc,
                                                     const char *str,
                                                     size_t len);

/** Creates and returns a null value, returns NULL on error. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_null(ctoon_mut_doc *doc);

/** Creates and returns a true value, returns NULL on error. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_true(ctoon_mut_doc *doc);

/** Creates and returns a false value, returns NULL on error. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_false(ctoon_mut_doc *doc);

/** Creates and returns a bool value, returns NULL on error. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_bool(ctoon_mut_doc *doc,
                                                  bool val);

/** Creates and returns an unsigned integer value, returns NULL on error. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_uint(ctoon_mut_doc *doc,
                                                  uint64_t num);

/** Creates and returns a signed integer value, returns NULL on error. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_sint(ctoon_mut_doc *doc,
                                                  int64_t num);

/** Creates and returns a signed integer value, returns NULL on error. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_int(ctoon_mut_doc *doc,
                                                 int64_t num);

/** Creates and returns a float number value, returns NULL on error. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_float(ctoon_mut_doc *doc,
                                                   float num);

/** Creates and returns a double number value, returns NULL on error. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_double(ctoon_mut_doc *doc,
                                                    double num);

/** Creates and returns a real number value, returns NULL on error. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_real(ctoon_mut_doc *doc,
                                                  double num);

/** Creates and returns a string value, returns NULL on error.
    The `str` should be a null-terminated UTF-8 string.
    @warning The input string is not copied, you should keep this string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_str(ctoon_mut_doc *doc,
                                                 const char *str);

/** Creates and returns a string value, returns NULL on error.
    The `str` should be a UTF-8 string, null-terminator is not required.
    @warning The input string is not copied, you should keep this string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_strn(ctoon_mut_doc *doc,
                                                  const char *str,
                                                  size_t len);

/** Creates and returns a string value, returns NULL on error.
    The `str` should be a null-terminated UTF-8 string.
    The input string is copied and held by the document. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_strcpy(ctoon_mut_doc *doc,
                                                    const char *str);

/** Creates and returns a string value, returns NULL on error.
    The `str` should be a UTF-8 string, null-terminator is not required.
    The input string is copied and held by the document. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_strncpy(ctoon_mut_doc *doc,
                                                     const char *str,
                                                     size_t len);



/*==============================================================================
 * MARK: - Mutable TOON Array API
 *============================================================================*/

/** Returns the number of elements in this array.
    Returns 0 if `arr` is NULL or type is not array. */
ctoon_api_inline size_t ctoon_mut_arr_size(ctoon_mut_val *arr);

/** Returns the element at the specified position in this array.
    Returns NULL if array is NULL/empty or the index is out of bounds.
    @warning This function takes a linear search time. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_get(ctoon_mut_val *arr,
                                                     size_t idx);

/** Returns the first element of this array.
    Returns NULL if `arr` is NULL/empty or type is not array. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_get_first(ctoon_mut_val *arr);

/** Returns the last element of this array.
    Returns NULL if `arr` is NULL/empty or type is not array. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_get_last(ctoon_mut_val *arr);



/*==============================================================================
 * MARK: - Mutable TOON Array Iterator API
 *============================================================================*/

/**
 A mutable TOON array iterator.

 @warning You should not modify the array while iterating over it, but you can
    use `ctoon_mut_arr_iter_remove()` to remove current value.

 @b Example
 @code
    ctoon_mut_val *val;
    ctoon_mut_arr_iter iter = ctoon_mut_arr_iter_with(arr);
    while ((val = ctoon_mut_arr_iter_next(&iter))) {
        your_func(val);
        if (your_val_is_unused(val)) {
            ctoon_mut_arr_iter_remove(&iter);
        }
    }
 @endcode
 */
typedef struct ctoon_mut_arr_iter {
    size_t idx; /**< next value's index */
    size_t max; /**< maximum index (arr.size) */
    ctoon_mut_val *cur; /**< current value */
    ctoon_mut_val *pre; /**< previous value */
    ctoon_mut_val *arr; /**< the array being iterated */
} ctoon_mut_arr_iter;

/**
 Initialize an iterator for this array.

 @param arr The array to be iterated over.
    If this parameter is NULL or not an array, `iter` will be set to empty.
 @param iter The iterator to be initialized.
    If this parameter is NULL, the function will fail and return false.
 @return true if the `iter` has been successfully initialized.

 @note The iterator does not need to be destroyed.
 */
ctoon_api_inline bool ctoon_mut_arr_iter_init(ctoon_mut_val *arr,
    ctoon_mut_arr_iter *iter);

/**
 Create an iterator with an array , same as `ctoon_mut_arr_iter_init()`.

 @param arr The array to be iterated over.
    If this parameter is NULL or not an array, an empty iterator will returned.
 @return A new iterator for the array.

 @note The iterator does not need to be destroyed.
 */
ctoon_api_inline ctoon_mut_arr_iter ctoon_mut_arr_iter_with(
    ctoon_mut_val *arr);

/**
 Returns whether the iteration has more elements.
 If `iter` is NULL, this function will return false.
 */
ctoon_api_inline bool ctoon_mut_arr_iter_has_next(
    ctoon_mut_arr_iter *iter);

/**
 Returns the next element in the iteration, or NULL on end.
 If `iter` is NULL, this function will return NULL.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_iter_next(
    ctoon_mut_arr_iter *iter);

/**
 Removes and returns current element in the iteration.
 If `iter` is NULL, this function will return NULL.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_iter_remove(
    ctoon_mut_arr_iter *iter);

/**
 Macro for iterating over an array.
 It works like iterator, but with a more intuitive API.

 @warning You should not modify the array while iterating over it.

 @b Example
 @code
    size_t idx, max;
    ctoon_mut_val *val;
    ctoon_mut_arr_foreach(arr, idx, max, val) {
        your_func(idx, val);
    }
 @endcode
 */
#define ctoon_mut_arr_foreach(arr, idx, max, val) \
    for ((idx) = 0, \
        (max) = ctoon_mut_arr_size(arr), \
        (val) = ctoon_mut_arr_get_first(arr); \
        (idx) < (max); \
        (idx)++, \
        (val) = (val)->next)



/*==============================================================================
 * MARK: - Mutable TOON Array Creation API
 *============================================================================*/

/**
 Creates and returns an empty mutable array.
 @param doc A mutable document, used for memory allocation only.
 @return The new array. NULL if input is NULL or memory allocation failed.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr(ctoon_mut_doc *doc);

/**
 Creates and returns a new mutable array with the given boolean values.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of boolean values.
 @param count The value count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const bool vals[3] = { true, false, true };
    ctoon_mut_val *arr = ctoon_mut_arr_with_bool(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_bool(
    ctoon_mut_doc *doc, const bool *vals, size_t count);

/**
 Creates and returns a new mutable array with the given sint numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of sint numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const int64_t vals[3] = { -1, 0, 1 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_sint64(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_sint(
    ctoon_mut_doc *doc, const int64_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given uint numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of uint numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const uint64_t vals[3] = { 0, 1, 0 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_uint(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_uint(
    ctoon_mut_doc *doc, const uint64_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given real numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of real numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const double vals[3] = { 0.1, 0.2, 0.3 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_real(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_real(
    ctoon_mut_doc *doc, const double *vals, size_t count);

/**
 Creates and returns a new mutable array with the given int8 numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of int8 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const int8_t vals[3] = { -1, 0, 1 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_sint8(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_sint8(
    ctoon_mut_doc *doc, const int8_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given int16 numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of int16 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const int16_t vals[3] = { -1, 0, 1 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_sint16(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_sint16(
    ctoon_mut_doc *doc, const int16_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given int32 numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of int32 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const int32_t vals[3] = { -1, 0, 1 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_sint32(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_sint32(
    ctoon_mut_doc *doc, const int32_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given int64 numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of int64 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const int64_t vals[3] = { -1, 0, 1 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_sint64(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_sint64(
    ctoon_mut_doc *doc, const int64_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given uint8 numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of uint8 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const uint8_t vals[3] = { 0, 1, 0 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_uint8(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_uint8(
    ctoon_mut_doc *doc, const uint8_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given uint16 numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of uint16 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const uint16_t vals[3] = { 0, 1, 0 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_uint16(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_uint16(
    ctoon_mut_doc *doc, const uint16_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given uint32 numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of uint32 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const uint32_t vals[3] = { 0, 1, 0 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_uint32(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_uint32(
    ctoon_mut_doc *doc, const uint32_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given uint64 numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of uint64 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
     const uint64_t vals[3] = { 0, 1, 0 };
     ctoon_mut_val *arr = ctoon_mut_arr_with_uint64(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_uint64(
    ctoon_mut_doc *doc, const uint64_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given float numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of float numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const float vals[3] = { -1.0f, 0.0f, 1.0f };
    ctoon_mut_val *arr = ctoon_mut_arr_with_float(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_float(
    ctoon_mut_doc *doc, const float *vals, size_t count);

/**
 Creates and returns a new mutable array with the given double numbers.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of double numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const double vals[3] = { -1.0, 0.0, 1.0 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_double(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_double(
    ctoon_mut_doc *doc, const double *vals, size_t count);

/**
 Creates and returns a new mutable array with the given strings, these strings
 will not be copied.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of UTF-8 null-terminator strings.
    If this array contains NULL, the function will fail and return NULL.
 @param count The number of values in `vals`.
    If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @warning The input strings are not copied, you should keep these strings
    unmodified for the lifetime of this TOON document. If these strings will be
    modified, you should use `ctoon_mut_arr_with_strcpy()` instead.

 @b Example
 @code
    const char *vals[3] = { "a", "b", "c" };
    ctoon_mut_val *arr = ctoon_mut_arr_with_str(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_str(
    ctoon_mut_doc *doc, const char **vals, size_t count);

/**
 Creates and returns a new mutable array with the given strings and string
 lengths, these strings will not be copied.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of UTF-8 strings, null-terminator is not required.
    If this array contains NULL, the function will fail and return NULL.
 @param lens A C array of string lengths, in bytes.
 @param count The number of strings in `vals`.
    If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @warning The input strings are not copied, you should keep these strings
    unmodified for the lifetime of this TOON document. If these strings will be
    modified, you should use `ctoon_mut_arr_with_strncpy()` instead.

 @b Example
 @code
    const char *vals[3] = { "a", "bb", "c" };
    const size_t lens[3] = { 1, 2, 1 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_strn(doc, vals, lens, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_strn(
    ctoon_mut_doc *doc, const char **vals, const size_t *lens, size_t count);

/**
 Creates and returns a new mutable array with the given strings, these strings
 will be copied.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of UTF-8 null-terminator strings.
    If this array contains NULL, the function will fail and return NULL.
 @param count The number of values in `vals`.
    If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const char *vals[3] = { "a", "b", "c" };
    ctoon_mut_val *arr = ctoon_mut_arr_with_strcpy(doc, vals, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_strcpy(
    ctoon_mut_doc *doc, const char **vals, size_t count);

/**
 Creates and returns a new mutable array with the given strings and string
 lengths, these strings will be copied.

 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of UTF-8 strings, null-terminator is not required.
    If this array contains NULL, the function will fail and return NULL.
 @param lens A C array of string lengths, in bytes.
 @param count The number of strings in `vals`.
    If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.

 @b Example
 @code
    const char *vals[3] = { "a", "bb", "c" };
    const size_t lens[3] = { 1, 2, 1 };
    ctoon_mut_val *arr = ctoon_mut_arr_with_strn(doc, vals, lens, 3);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_strncpy(
    ctoon_mut_doc *doc, const char **vals, const size_t *lens, size_t count);



/*==============================================================================
 * MARK: - Mutable TOON Array Modification API
 *============================================================================*/

/**
 Inserts a value into an array at a given index.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param val The value to be inserted. Returns false if it is NULL.
 @param idx The index to which to insert the new value.
    Returns false if the index is out of range.
 @return Whether successful.
 @warning This function takes a linear search time.
 */
ctoon_api_inline bool ctoon_mut_arr_insert(ctoon_mut_val *arr,
                                             ctoon_mut_val *val, size_t idx);

/**
 Inserts a value at the end of the array.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param val The value to be inserted. Returns false if it is NULL.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_append(ctoon_mut_val *arr,
                                             ctoon_mut_val *val);

/**
 Inserts a value at the head of the array.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param val The value to be inserted. Returns false if it is NULL.
 @return    Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_prepend(ctoon_mut_val *arr,
                                              ctoon_mut_val *val);

/**
 Replaces a value at index and returns old value.
 @param arr The array to which the value is to be replaced.
    Returns false if it is NULL or not an array.
 @param idx The index to which to replace the value.
    Returns false if the index is out of range.
 @param val The new value to replace. Returns false if it is NULL.
 @return Old value, or NULL on error.
 @warning This function takes a linear search time.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_replace(ctoon_mut_val *arr,
                                                         size_t idx,
                                                         ctoon_mut_val *val);

/**
 Removes and returns a value at index.
 @param arr The array from which the value is to be removed.
    Returns false if it is NULL or not an array.
 @param idx The index from which to remove the value.
    Returns false if the index is out of range.
 @return Old value, or NULL on error.
 @warning This function takes a linear search time.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_remove(ctoon_mut_val *arr,
                                                        size_t idx);

/**
 Removes and returns the first value in this array.
 @param arr The array from which the value is to be removed.
    Returns false if it is NULL or not an array.
 @return The first value, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_remove_first(
    ctoon_mut_val *arr);

/**
 Removes and returns the last value in this array.
 @param arr The array from which the value is to be removed.
    Returns false if it is NULL or not an array.
 @return The last value, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_remove_last(
    ctoon_mut_val *arr);

/**
 Removes all values within a specified range in the array.
 @param arr The array from which the value is to be removed.
    Returns false if it is NULL or not an array.
 @param idx The start index of the range (0 is the first).
 @param len The number of items in the range (can be 0).
 @return Whether successful.
 @warning This function takes a linear search time.
 */
ctoon_api_inline bool ctoon_mut_arr_remove_range(ctoon_mut_val *arr,
                                                   size_t idx, size_t len);

/**
 Removes all values in this array.
 @param arr The array from which all of the values are to be removed.
    Returns false if it is NULL or not an array.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_clear(ctoon_mut_val *arr);

/**
 Rotates values in this array for the given number of times.
 For example: `[1,2,3,4,5]` rotate 2 is `[3,4,5,1,2]`.
 @param arr The array to be rotated.
 @param idx Index (or times) to rotate.
 @warning This function takes a linear search time.
 */
ctoon_api_inline bool ctoon_mut_arr_rotate(ctoon_mut_val *arr,
                                             size_t idx);



/*==============================================================================
 * MARK: - Mutable TOON Array Modification Convenience API
 *============================================================================*/

/**
 Adds a value at the end of the array.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param val The value to be inserted. Returns false if it is NULL.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_val(ctoon_mut_val *arr,
                                              ctoon_mut_val *val);

/**
 Adds a `null` value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_null(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr);

/**
 Adds a `true` value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_true(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr);

/**
 Adds a `false` value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_false(ctoon_mut_doc *doc,
                                                ctoon_mut_val *arr);

/**
 Adds a bool value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param val The bool value to be added.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_bool(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr,
                                               bool val);

/**
 Adds an unsigned integer value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param num The number to be added.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_uint(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr,
                                               uint64_t num);

/**
 Adds a signed integer value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param num The number to be added.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_sint(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr,
                                               int64_t num);

/**
 Adds an integer value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param num The number to be added.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_int(ctoon_mut_doc *doc,
                                              ctoon_mut_val *arr,
                                              int64_t num);

/**
 Adds a float value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param num The number to be added.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_float(ctoon_mut_doc *doc,
                                                ctoon_mut_val *arr,
                                                float num);

/**
 Adds a double value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param num The number to be added.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_double(ctoon_mut_doc *doc,
                                                 ctoon_mut_val *arr,
                                                 double num);

/**
 Adds a double value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param num The number to be added.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_real(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr,
                                               double num);

/**
 Adds a string value at the end of the array (no copy).
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param str A null-terminated UTF-8 string.
 @return Whether successful.
 @warning The input string is not copied, you should keep this string unmodified
    for the lifetime of this TOON document.
 */
ctoon_api_inline bool ctoon_mut_arr_add_str(ctoon_mut_doc *doc,
                                              ctoon_mut_val *arr,
                                              const char *str);

/**
 Adds a string value at the end of the array (no copy).
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param str A UTF-8 string, null-terminator is not required.
 @param len The length of the string, in bytes.
 @return Whether successful.
 @warning The input string is not copied, you should keep this string unmodified
    for the lifetime of this TOON document.
 */
ctoon_api_inline bool ctoon_mut_arr_add_strn(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr,
                                               const char *str,
                                               size_t len);

/**
 Adds a string value at the end of the array (copied).
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param str A null-terminated UTF-8 string.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_strcpy(ctoon_mut_doc *doc,
                                                 ctoon_mut_val *arr,
                                                 const char *str);

/**
 Adds a string value at the end of the array (copied).
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param str A UTF-8 string, null-terminator is not required.
 @param len The length of the string, in bytes.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_arr_add_strncpy(ctoon_mut_doc *doc,
                                                  ctoon_mut_val *arr,
                                                  const char *str,
                                                  size_t len);

/**
 Creates and adds a new array at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @return The new array, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_add_arr(ctoon_mut_doc *doc,
                                                         ctoon_mut_val *arr);

/**
 Creates and adds a new object at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @return The new object, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_add_obj(ctoon_mut_doc *doc,
                                                         ctoon_mut_val *arr);



/*==============================================================================
 * MARK: - Mutable TOON Object API
 *============================================================================*/

/** Returns the number of key-value pairs in this object.
    Returns 0 if `obj` is NULL or type is not object. */
ctoon_api_inline size_t ctoon_mut_obj_size(ctoon_mut_val *obj);

/** Returns the value to which the specified key is mapped.
    Returns NULL if this object contains no mapping for the key.
    Returns NULL if `obj/key` is NULL, or type is not object.

    The `key` should be a null-terminated UTF-8 string.

    @warning This function takes a linear search time. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_get(ctoon_mut_val *obj,
                                                     const char *key);

/** Returns the value to which the specified key is mapped.
    Returns NULL if this object contains no mapping for the key.
    Returns NULL if `obj/key` is NULL, or type is not object.

    The `key` should be a UTF-8 string, null-terminator is not required.
    The `key_len` should be the length of the key, in bytes.

    @warning This function takes a linear search time. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_getn(ctoon_mut_val *obj,
                                                      const char *key,
                                                      size_t key_len);



/*==============================================================================
 * MARK: - Mutable TOON Object Iterator API
 *============================================================================*/

/**
 A mutable TOON object iterator.

 @warning You should not modify the object while iterating over it, but you can
    use `ctoon_mut_obj_iter_remove()` to remove current value.

 @b Example
 @code
    ctoon_mut_val *key, *val;
    ctoon_mut_obj_iter iter = ctoon_mut_obj_iter_with(obj);
    while ((key = ctoon_mut_obj_iter_next(&iter))) {
        val = ctoon_mut_obj_iter_get_val(key);
        your_func(key, val);
        if (your_val_is_unused(key, val)) {
            ctoon_mut_obj_iter_remove(&iter);
        }
    }
 @endcode

 If the ordering of the keys is known at compile-time, you can use this method
 to speed up value lookups:
 @code
    // {"k1":1, "k2": 3, "k3": 3}
    ctoon_mut_val *key, *val;
    ctoon_mut_obj_iter iter = ctoon_mut_obj_iter_with(obj);
    ctoon_mut_val *v1 = ctoon_mut_obj_iter_get(&iter, "k1");
    ctoon_mut_val *v3 = ctoon_mut_obj_iter_get(&iter, "k3");
 @endcode
 @see `ctoon_mut_obj_iter_get()` and `ctoon_mut_obj_iter_getn()`
 */
typedef struct ctoon_mut_obj_iter {
    size_t idx; /**< next key's index */
    size_t max; /**< maximum key index (obj.size) */
    ctoon_mut_val *cur; /**< current key */
    ctoon_mut_val *pre; /**< previous key */
    ctoon_mut_val *obj; /**< the object being iterated */
} ctoon_mut_obj_iter;

/**
 Initialize an iterator for this object.

 @param obj The object to be iterated over.
    If this parameter is NULL or not an array, `iter` will be set to empty.
 @param iter The iterator to be initialized.
    If this parameter is NULL, the function will fail and return false.
 @return true if the `iter` has been successfully initialized.

 @note The iterator does not need to be destroyed.
 */
ctoon_api_inline bool ctoon_mut_obj_iter_init(ctoon_mut_val *obj,
    ctoon_mut_obj_iter *iter);

/**
 Create an iterator with an object, same as `ctoon_obj_iter_init()`.

 @param obj The object to be iterated over.
    If this parameter is NULL or not an object, an empty iterator will returned.
 @return A new iterator for the object.

 @note The iterator does not need to be destroyed.
 */
ctoon_api_inline ctoon_mut_obj_iter ctoon_mut_obj_iter_with(
    ctoon_mut_val *obj);

/**
 Returns whether the iteration has more elements.
 If `iter` is NULL, this function will return false.
 */
ctoon_api_inline bool ctoon_mut_obj_iter_has_next(
    ctoon_mut_obj_iter *iter);

/**
 Returns the next key in the iteration, or NULL on end.
 If `iter` is NULL, this function will return NULL.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_iter_next(
    ctoon_mut_obj_iter *iter);

/**
 Returns the value for key inside the iteration.
 If `iter` is NULL, this function will return NULL.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_iter_get_val(
    ctoon_mut_val *key);

/**
 Removes current key-value pair in the iteration, returns the removed value.
 If `iter` is NULL, this function will return NULL.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_iter_remove(
    ctoon_mut_obj_iter *iter);

/**
 Iterates to a specified key and returns the value.

 This function does the same thing as `ctoon_mut_obj_get()`, but is much faster
 if the ordering of the keys is known at compile-time and you are using the same
 order to look up the values. If the key exists in this object, then the
 iterator will stop at the next key, otherwise the iterator will not change and
 NULL is returned.

 @param iter The object iterator, should not be NULL.
 @param key The key, should be a UTF-8 string with null-terminator.
 @return The value to which the specified key is mapped.
    NULL if this object contains no mapping for the key or input is invalid.

 @warning This function takes a linear search time if the key is not nearby.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_iter_get(
    ctoon_mut_obj_iter *iter, const char *key);

/**
 Iterates to a specified key and returns the value.

 This function does the same thing as `ctoon_mut_obj_getn()` but is much faster
 if the ordering of the keys is known at compile-time and you are using the same
 order to look up the values. If the key exists in this object, then the
 iterator will stop at the next key, otherwise the iterator will not change and
 NULL is returned.

 @param iter The object iterator, should not be NULL.
 @param key The key, should be a UTF-8 string, null-terminator is not required.
 @param key_len The the length of `key`, in bytes.
 @return The value to which the specified key is mapped.
    NULL if this object contains no mapping for the key or input is invalid.

 @warning This function takes a linear search time if the key is not nearby.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_iter_getn(
    ctoon_mut_obj_iter *iter, const char *key, size_t key_len);

/**
 Macro for iterating over an object.
 It works like iterator, but with a more intuitive API.

 @warning You should not modify the object while iterating over it.

 @b Example
 @code
    size_t idx, max;
    ctoon_mut_val *key, *val;
    ctoon_mut_obj_foreach(obj, idx, max, key, val) {
        your_func(key, val);
    }
 @endcode
 */
#define ctoon_mut_obj_foreach(obj, idx, max, key, val) \
    for ((idx) = 0, \
        (max) = ctoon_mut_obj_size(obj), \
        (key) = (max) ? ((ctoon_mut_val *)(obj)->uni.ptr)->next->next : NULL, \
        (val) = (key) ? (key)->next : NULL; \
        (idx) < (max); \
        (idx)++, \
        (key) = (val)->next, \
        (val) = (key)->next)



/*==============================================================================
 * MARK: - Mutable TOON Object Creation API
 *============================================================================*/

/** Creates and returns a mutable object, returns NULL on error. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj(ctoon_mut_doc *doc);

/**
 Creates and returns a mutable object with keys and values, returns NULL on
 error. The keys and values are not copied. The strings should be a
 null-terminated UTF-8 string.

 @warning The input string is not copied, you should keep this string
    unmodified for the lifetime of this TOON document.

 @b Example
 @code
    const char *keys[2] = { "id", "name" };
    const char *vals[2] = { "01", "Harry" };
    ctoon_mut_val *obj = ctoon_mut_obj_with_str(doc, keys, vals, 2);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_with_str(ctoon_mut_doc *doc,
                                                          const char **keys,
                                                          const char **vals,
                                                          size_t count);

/**
 Creates and returns a mutable object with key-value pairs and pair count,
 returns NULL on error. The keys and values are not copied. The strings should
 be a null-terminated UTF-8 string.

 @warning The input string is not copied, you should keep this string
    unmodified for the lifetime of this TOON document.

 @b Example
 @code
    const char *kv_pairs[4] = { "id", "01", "name", "Harry" };
    ctoon_mut_val *obj = ctoon_mut_obj_with_kv(doc, kv_pairs, 2);
 @endcode
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_with_kv(ctoon_mut_doc *doc,
                                                         const char **kv_pairs,
                                                         size_t pair_count);



/*==============================================================================
 * MARK: - Mutable TOON Object Modification API
 *============================================================================*/

/**
 Adds a key-value pair at the end of the object.
 This function allows duplicated key in one object.
 @param obj The object to which the new key-value pair is to be added.
 @param key The key, should be a string which is created by `ctoon_mut_str()`,
    `ctoon_mut_strn()`, `ctoon_mut_strcpy()` or `ctoon_mut_strncpy()`.
 @param val The value to add to the object.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_obj_add(ctoon_mut_val *obj,
                                          ctoon_mut_val *key,
                                          ctoon_mut_val *val);
/**
 Sets a key-value pair at the end of the object.
 This function may remove all key-value pairs for the given key before add.
 @param obj The object to which the new key-value pair is to be added.
 @param key The key, should be a string which is created by `ctoon_mut_str()`,
    `ctoon_mut_strn()`, `ctoon_mut_strcpy()` or `ctoon_mut_strncpy()`.
 @param val The value to add to the object. If this value is null, the behavior
    is same as `ctoon_mut_obj_remove()`.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_obj_put(ctoon_mut_val *obj,
                                          ctoon_mut_val *key,
                                          ctoon_mut_val *val);

/**
 Inserts a key-value pair to the object at the given position.
 This function allows duplicated key in one object.
 @param obj The object to which the new key-value pair is to be added.
 @param key The key, should be a string which is created by `ctoon_mut_str()`,
    `ctoon_mut_strn()`, `ctoon_mut_strcpy()` or `ctoon_mut_strncpy()`.
 @param val The value to add to the object.
 @param idx The index to which to insert the new pair.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_obj_insert(ctoon_mut_val *obj,
                                             ctoon_mut_val *key,
                                             ctoon_mut_val *val,
                                             size_t idx);

/**
 Removes all key-value pair from the object with given key.
 @param obj The object from which the key-value pair is to be removed.
 @param key The key, should be a string value.
 @return The first matched value, or NULL if no matched value.
 @warning This function takes a linear search time.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_remove(ctoon_mut_val *obj,
                                                        ctoon_mut_val *key);

/**
 Removes all key-value pair from the object with given key.
 @param obj The object from which the key-value pair is to be removed.
 @param key The key, should be a UTF-8 string with null-terminator.
 @return The first matched value, or NULL if no matched value.
 @warning This function takes a linear search time.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_remove_key(
    ctoon_mut_val *obj, const char *key);

/**
 Removes all key-value pair from the object with given key.
 @param obj The object from which the key-value pair is to be removed.
 @param key The key, should be a UTF-8 string, null-terminator is not required.
 @param key_len The length of the key.
 @return The first matched value, or NULL if no matched value.
 @warning This function takes a linear search time.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_remove_keyn(
    ctoon_mut_val *obj, const char *key, size_t key_len);

/**
 Removes all key-value pairs in this object.
 @param obj The object from which all of the values are to be removed.
 @return Whether successful.
 */
ctoon_api_inline bool ctoon_mut_obj_clear(ctoon_mut_val *obj);

/**
 Replaces value from the object with given key.
 If the key is not exist, or the value is NULL, it will fail.
 @param obj The object to which the value is to be replaced.
 @param key The key, should be a string value.
 @param val The value to replace into the object.
 @return Whether successful.
 @warning This function takes a linear search time.
 */
ctoon_api_inline bool ctoon_mut_obj_replace(ctoon_mut_val *obj,
                                              ctoon_mut_val *key,
                                              ctoon_mut_val *val);

/**
 Rotates key-value pairs in the object for the given number of times.
 For example: `{"a":1,"b":2,"c":3,"d":4}` rotate 1 is
 `{"b":2,"c":3,"d":4,"a":1}`.
 @param obj The object to be rotated.
 @param idx Index (or times) to rotate.
 @return Whether successful.
 @warning This function takes a linear search time.
 */
ctoon_api_inline bool ctoon_mut_obj_rotate(ctoon_mut_val *obj,
                                             size_t idx);



/*==============================================================================
 * MARK: - Mutable TOON Object Modification Convenience API
 *============================================================================*/

/** Adds a `null` value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_null(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *key);

/** Adds a `true` value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_true(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *key);

/** Adds a `false` value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_false(ctoon_mut_doc *doc,
                                                ctoon_mut_val *obj,
                                                const char *key);

/** Adds a bool value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_bool(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *key, bool val);

/** Adds an unsigned integer value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_uint(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *key, uint64_t val);

/** Adds a signed integer value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_sint(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *key, int64_t val);

/** Adds an int value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_int(ctoon_mut_doc *doc,
                                              ctoon_mut_val *obj,
                                              const char *key, int64_t val);

/** Adds a float value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_float(ctoon_mut_doc *doc,
                                                ctoon_mut_val *obj,
                                                const char *key, float val);

/** Adds a double value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_double(ctoon_mut_doc *doc,
                                                 ctoon_mut_val *obj,
                                                 const char *key, double val);

/** Adds a real value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_real(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *key, double val);

/** Adds a string value at the end of the object.
    The `key` and `val` should be null-terminated UTF-8 strings.
    This function allows duplicated key in one object.

    @warning The key/value strings are not copied, you should keep these strings
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_str(ctoon_mut_doc *doc,
                                              ctoon_mut_val *obj,
                                              const char *key, const char *val);

/** Adds a string value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    The `val` should be a UTF-8 string, null-terminator is not required.
    The `len` should be the length of the `val`, in bytes.
    This function allows duplicated key in one object.

    @warning The key/value strings are not copied, you should keep these strings
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_strn(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *key,
                                               const char *val, size_t len);

/** Adds a string value at the end of the object.
    The `key` and `val` should be null-terminated UTF-8 strings.
    The value string is copied.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_strcpy(ctoon_mut_doc *doc,
                                                 ctoon_mut_val *obj,
                                                 const char *key,
                                                 const char *val);

/** Adds a string value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    The `val` should be a UTF-8 string, null-terminator is not required.
    The `len` should be the length of the `val`, in bytes.
    This function allows duplicated key in one object.

    @warning The key strings are not copied, you should keep these strings
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_strncpy(ctoon_mut_doc *doc,
                                                  ctoon_mut_val *obj,
                                                  const char *key,
                                                  const char *val, size_t len);

/**
 Creates and adds a new array to the target object.
 The `key` should be a null-terminated UTF-8 string.
 This function allows duplicated key in one object.

 @warning The key string is not copied, you should keep these strings
          unmodified for the lifetime of this TOON document.
 @return The new array, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_add_arr(ctoon_mut_doc *doc,
                                                         ctoon_mut_val *obj,
                                                         const char *key);

/**
 Creates and adds a new object to the target object.
 The `key` should be a null-terminated UTF-8 string.
 This function allows duplicated key in one object.

 @warning The key string is not copied, you should keep these strings
          unmodified for the lifetime of this TOON document.
 @return The new object, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_add_obj(ctoon_mut_doc *doc,
                                                         ctoon_mut_val *obj,
                                                         const char *key);

/** Adds a TOON value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.

    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this TOON document. */
ctoon_api_inline bool ctoon_mut_obj_add_val(ctoon_mut_doc *doc,
                                              ctoon_mut_val *obj,
                                              const char *key,
                                              ctoon_mut_val *val);

/** Removes all key-value pairs for the given key.
    Returns the first value to which the specified key is mapped or NULL if this
    object contains no mapping for the key.
    The `key` should be a null-terminated UTF-8 string.

    @warning This function takes a linear search time. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_remove_str(
    ctoon_mut_val *obj, const char *key);

/** Removes all key-value pairs for the given key.
    Returns the first value to which the specified key is mapped or NULL if this
    object contains no mapping for the key.
    The `key` should be a UTF-8 string, null-terminator is not required.
    The `len` should be the length of the key, in bytes.

    @warning This function takes a linear search time. */
ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_remove_strn(
    ctoon_mut_val *obj, const char *key, size_t len);

/** Replaces all matching keys with the new key.
    Returns true if at least one key was renamed.
    The `key` and `new_key` should be a null-terminated UTF-8 string.
    The `new_key` is copied and held by doc.

    @warning This function takes a linear search time.
    If `new_key` already exists, it will cause duplicate keys.
 */
ctoon_api_inline bool ctoon_mut_obj_rename_key(ctoon_mut_doc *doc,
                                                 ctoon_mut_val *obj,
                                                 const char *key,
                                                 const char *new_key);

/** Replaces all matching keys with the new key.
    Returns true if at least one key was renamed.
    The `key` and `new_key` should be a UTF-8 string,
    null-terminator is not required. The `new_key` is copied and held by doc.

    @warning This function takes a linear search time.
    If `new_key` already exists, it will cause duplicate keys.
 */
ctoon_api_inline bool ctoon_mut_obj_rename_keyn(ctoon_mut_doc *doc,
                                                  ctoon_mut_val *obj,
                                                  const char *key,
                                                  size_t len,
                                                  const char *new_key,
                                                  size_t new_len);



#if !defined(CTOON_DISABLE_UTILS) || !CTOON_DISABLE_UTILS

/*==============================================================================
 * MARK: - TOON Pointer API (RFC 6901)
 * https://tools.ietf.org/html/rfc6901
 *============================================================================*/

/** TOON Pointer error code. */
typedef uint32_t ctoon_ptr_code;

/** No TOON pointer error. */
static const ctoon_ptr_code CTOON_PTR_ERR_NONE = 0;

/** Invalid input parameter, such as NULL input. */
static const ctoon_ptr_code CTOON_PTR_ERR_PARAMETER = 1;

/** TOON pointer syntax error, such as invalid escape, token no prefix. */
static const ctoon_ptr_code CTOON_PTR_ERR_SYNTAX = 2;

/** TOON pointer resolve failed, such as index out of range, key not found. */
static const ctoon_ptr_code CTOON_PTR_ERR_RESOLVE = 3;

/** Document's root is NULL, but it is required for the function call. */
static const ctoon_ptr_code CTOON_PTR_ERR_NULL_ROOT = 4;

/** Cannot set root as the target is not a document. */
static const ctoon_ptr_code CTOON_PTR_ERR_SET_ROOT = 5;

/** The memory allocation failed and a new value could not be created. */
static const ctoon_ptr_code CTOON_PTR_ERR_MEMORY_ALLOCATION = 6;

/** Error information for TOON pointer. */
typedef struct ctoon_ptr_err {
    /** Error code, see `ctoon_ptr_code` for all possible values. */
    ctoon_ptr_code code;
    /** Error message, constant, no need to free (NULL if no error). */
    const char *msg;
    /** Error byte position for input TOON pointer (0 if no error). */
    size_t pos;
} ctoon_ptr_err;

/**
 A context for TOON pointer operation.

 This struct stores the context of TOON Pointer operation result. The struct
 can be used with three helper functions: `ctx_append()`, `ctx_replace()`, and
 `ctx_remove()`, which perform the corresponding operations on the container
 without re-parsing the TOON Pointer.

 For example:
 @code
    // doc before: {"a":[0,1,null]}
    // ptr: "/a/2"
    val = ctoon_mut_doc_ptr_getx(doc, ptr, strlen(ptr), &ctx, &err);
    if (ctoon_is_null(val)) {
        ctoon_ptr_ctx_remove(&ctx);
    }
    // doc after: {"a":[0,1]}
 @endcode
 */
typedef struct ctoon_ptr_ctx {
    /**
     The container (parent) of the target value. It can be either an array or
     an object. If the target location has no value, but all its parent
     containers exist, and the target location can be used to insert a new
     value, then `ctn` is the parent container of the target location.
     Otherwise, `ctn` is NULL.
     */
    ctoon_mut_val *ctn;
    /**
     The previous sibling of the target value. It can be either a value in an
     array or a key in an object. As the container is a `circular linked list`
     of elements, `pre` is the previous node of the target value. If the
     operation is `add` or `set`, then `pre` is the previous node of the new
     value, not the original target value. If the target value does not exist,
     `pre` is NULL.
     */
    ctoon_mut_val *pre;
    /**
     The removed value if the operation is `set`, `replace` or `remove`. It can
     be used to restore the original state of the document if needed.
     */
    ctoon_mut_val *old;
} ctoon_ptr_ctx;

/**
 Get value by a TOON Pointer.
 @param doc The TOON document to be queried.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @return The value referenced by the TOON pointer.
    NULL if `doc` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_val *ctoon_doc_ptr_get(ctoon_doc *doc,
                                                 const char *ptr);

/**
 Get value by a TOON Pointer.
 @param doc The TOON document to be queried.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The value referenced by the TOON pointer.
    NULL if `doc` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_val *ctoon_doc_ptr_getn(ctoon_doc *doc,
                                                  const char *ptr, size_t len);

/**
 Get value by a TOON Pointer.
 @param doc The TOON document to be queried.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The value referenced by the TOON pointer.
    NULL if `doc` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_val *ctoon_doc_ptr_getx(ctoon_doc *doc,
                                                  const char *ptr, size_t len,
                                                  ctoon_ptr_err *err);

/**
 Get value by a TOON Pointer.
 @param val The TOON value to be queried.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @return The value referenced by the TOON pointer.
    NULL if `val` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_val *ctoon_ptr_get(ctoon_val *val,
                                             const char *ptr);

/**
 Get value by a TOON Pointer.
 @param val The TOON value to be queried.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The value referenced by the TOON pointer.
    NULL if `val` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_val *ctoon_ptr_getn(ctoon_val *val,
                                              const char *ptr, size_t len);

/**
 Get value by a TOON Pointer.
 @param val The TOON value to be queried.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The value referenced by the TOON pointer.
    NULL if `val` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_val *ctoon_ptr_getx(ctoon_val *val,
                                              const char *ptr, size_t len,
                                              ctoon_ptr_err *err);

/**
 Get value by a TOON Pointer.
 @param doc The TOON document to be queried.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @return The value referenced by the TOON pointer.
    NULL if `doc` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_get(ctoon_mut_doc *doc,
                                                         const char *ptr);

/**
 Get value by a TOON Pointer.
 @param doc The TOON document to be queried.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The value referenced by the TOON pointer.
    NULL if `doc` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_getn(ctoon_mut_doc *doc,
                                                          const char *ptr,
                                                          size_t len);

/**
 Get value by a TOON Pointer.
 @param doc The TOON document to be queried.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The value referenced by the TOON pointer.
    NULL if `doc` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_getx(ctoon_mut_doc *doc,
                                                          const char *ptr,
                                                          size_t len,
                                                          ctoon_ptr_ctx *ctx,
                                                          ctoon_ptr_err *err);

/**
 Get value by a TOON Pointer.
 @param val The TOON value to be queried.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @return The value referenced by the TOON pointer.
    NULL if `val` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_get(ctoon_mut_val *val,
                                                     const char *ptr);

/**
 Get value by a TOON Pointer.
 @param val The TOON value to be queried.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The value referenced by the TOON pointer.
    NULL if `val` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_getn(ctoon_mut_val *val,
                                                      const char *ptr,
                                                      size_t len);

/**
 Get value by a TOON Pointer.
 @param val The TOON value to be queried.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The value referenced by the TOON pointer.
    NULL if `val` or `ptr` is NULL, or the TOON pointer cannot be resolved.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_getx(ctoon_mut_val *val,
                                                      const char *ptr,
                                                      size_t len,
                                                      ctoon_ptr_ctx *ctx,
                                                      ctoon_ptr_err *err);

/**
 Add (insert) value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @param new_val The value to be added.
 @return true if TOON pointer is valid and new value is added, false otherwise.
 @note The parent nodes will be created if they do not exist.
 */
ctoon_api_inline bool ctoon_mut_doc_ptr_add(ctoon_mut_doc *doc,
                                              const char *ptr,
                                              ctoon_mut_val *new_val);

/**
 Add (insert) value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be added.
 @return true if TOON pointer is valid and new value is added, false otherwise.
 @note The parent nodes will be created if they do not exist.
 */
ctoon_api_inline bool ctoon_mut_doc_ptr_addn(ctoon_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               ctoon_mut_val *new_val);

/**
 Add (insert) value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be added.
 @param create_parent Whether to create parent nodes if not exist.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return true if TOON pointer is valid and new value is added, false otherwise.
 */
ctoon_api_inline bool ctoon_mut_doc_ptr_addx(ctoon_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               ctoon_mut_val *new_val,
                                               bool create_parent,
                                               ctoon_ptr_ctx *ctx,
                                               ctoon_ptr_err *err);

/**
 Add (insert) value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @param doc Only used to create new values when needed.
 @param new_val The value to be added.
 @return true if TOON pointer is valid and new value is added, false otherwise.
 @note The parent nodes will be created if they do not exist.
 */
ctoon_api_inline bool ctoon_mut_ptr_add(ctoon_mut_val *val,
                                          const char *ptr,
                                          ctoon_mut_val *new_val,
                                          ctoon_mut_doc *doc);

/**
 Add (insert) value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param doc Only used to create new values when needed.
 @param new_val The value to be added.
 @return true if TOON pointer is valid and new value is added, false otherwise.
 @note The parent nodes will be created if they do not exist.
 */
ctoon_api_inline bool ctoon_mut_ptr_addn(ctoon_mut_val *val,
                                           const char *ptr, size_t len,
                                           ctoon_mut_val *new_val,
                                           ctoon_mut_doc *doc);

/**
 Add (insert) value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param doc Only used to create new values when needed.
 @param new_val The value to be added.
 @param create_parent Whether to create parent nodes if not exist.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return true if TOON pointer is valid and new value is added, false otherwise.
 */
ctoon_api_inline bool ctoon_mut_ptr_addx(ctoon_mut_val *val,
                                           const char *ptr, size_t len,
                                           ctoon_mut_val *new_val,
                                           ctoon_mut_doc *doc,
                                           bool create_parent,
                                           ctoon_ptr_ctx *ctx,
                                           ctoon_ptr_err *err);

/**
 Set value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @param new_val The value to be set, pass NULL to remove.
 @return true if TOON pointer is valid and new value is set, false otherwise.
 @note The parent nodes will be created if they do not exist.
    If the target value already exists, it will be replaced by the new value.
 */
ctoon_api_inline bool ctoon_mut_doc_ptr_set(ctoon_mut_doc *doc,
                                              const char *ptr,
                                              ctoon_mut_val *new_val);

/**
 Set value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be set, pass NULL to remove.
 @return true if TOON pointer is valid and new value is set, false otherwise.
 @note The parent nodes will be created if they do not exist.
    If the target value already exists, it will be replaced by the new value.
 */
ctoon_api_inline bool ctoon_mut_doc_ptr_setn(ctoon_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               ctoon_mut_val *new_val);

/**
 Set value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be set, pass NULL to remove.
 @param create_parent Whether to create parent nodes if not exist.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return true if TOON pointer is valid and new value is set, false otherwise.
 @note If the target value already exists, it will be replaced by the new value.
 */
ctoon_api_inline bool ctoon_mut_doc_ptr_setx(ctoon_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               ctoon_mut_val *new_val,
                                               bool create_parent,
                                               ctoon_ptr_ctx *ctx,
                                               ctoon_ptr_err *err);

/**
 Set value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @param new_val The value to be set, pass NULL to remove.
 @param doc Only used to create new values when needed.
 @return true if TOON pointer is valid and new value is set, false otherwise.
 @note The parent nodes will be created if they do not exist.
    If the target value already exists, it will be replaced by the new value.
 */
ctoon_api_inline bool ctoon_mut_ptr_set(ctoon_mut_val *val,
                                          const char *ptr,
                                          ctoon_mut_val *new_val,
                                          ctoon_mut_doc *doc);

/**
 Set value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be set, pass NULL to remove.
 @param doc Only used to create new values when needed.
 @return true if TOON pointer is valid and new value is set, false otherwise.
 @note The parent nodes will be created if they do not exist.
    If the target value already exists, it will be replaced by the new value.
 */
ctoon_api_inline bool ctoon_mut_ptr_setn(ctoon_mut_val *val,
                                           const char *ptr, size_t len,
                                           ctoon_mut_val *new_val,
                                           ctoon_mut_doc *doc);

/**
 Set value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be set, pass NULL to remove.
 @param doc Only used to create new values when needed.
 @param create_parent Whether to create parent nodes if not exist.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return true if TOON pointer is valid and new value is set, false otherwise.
 @note If the target value already exists, it will be replaced by the new value.
 */
ctoon_api_inline bool ctoon_mut_ptr_setx(ctoon_mut_val *val,
                                           const char *ptr, size_t len,
                                           ctoon_mut_val *new_val,
                                           ctoon_mut_doc *doc,
                                           bool create_parent,
                                           ctoon_ptr_ctx *ctx,
                                           ctoon_ptr_err *err);

/**
 Replace value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @param new_val The new value to replace the old one.
 @return The old value that was replaced, or NULL if not found.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_replace(
    ctoon_mut_doc *doc, const char *ptr, ctoon_mut_val *new_val);

/**
 Replace value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The new value to replace the old one.
 @return The old value that was replaced, or NULL if not found.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_replacen(
    ctoon_mut_doc *doc, const char *ptr, size_t len, ctoon_mut_val *new_val);

/**
 Replace value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The new value to replace the old one.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The old value that was replaced, or NULL if not found.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_replacex(
    ctoon_mut_doc *doc, const char *ptr, size_t len, ctoon_mut_val *new_val,
    ctoon_ptr_ctx *ctx, ctoon_ptr_err *err);

/**
 Replace value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @param new_val The new value to replace the old one.
 @return The old value that was replaced, or NULL if not found.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_replace(
    ctoon_mut_val *val, const char *ptr, ctoon_mut_val *new_val);

/**
 Replace value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The new value to replace the old one.
 @return The old value that was replaced, or NULL if not found.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_replacen(
    ctoon_mut_val *val, const char *ptr, size_t len, ctoon_mut_val *new_val);

/**
 Replace value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The new value to replace the old one.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The old value that was replaced, or NULL if not found.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_replacex(
    ctoon_mut_val *val, const char *ptr, size_t len, ctoon_mut_val *new_val,
    ctoon_ptr_ctx *ctx, ctoon_ptr_err *err);

/**
 Remove value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @return The removed value, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_remove(
    ctoon_mut_doc *doc, const char *ptr);

/**
 Remove value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The removed value, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_removen(
    ctoon_mut_doc *doc, const char *ptr, size_t len);

/**
 Remove value by a TOON pointer.
 @param doc The target TOON document.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The removed value, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_removex(
    ctoon_mut_doc *doc, const char *ptr, size_t len,
    ctoon_ptr_ctx *ctx, ctoon_ptr_err *err);

/**
 Remove value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8 with null-terminator).
 @return The removed value, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_remove(ctoon_mut_val *val,
                                                        const char *ptr);

/**
 Remove value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The removed value, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_removen(ctoon_mut_val *val,
                                                         const char *ptr,
                                                         size_t len);

/**
 Remove value by a TOON pointer.
 @param val The target TOON value.
 @param ptr The TOON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The removed value, or NULL on error.
 */
ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_removex(ctoon_mut_val *val,
                                                         const char *ptr,
                                                         size_t len,
                                                         ctoon_ptr_ctx *ctx,
                                                         ctoon_ptr_err *err);

/**
 Append value by TOON pointer context.
 @param ctx The context from the `ctoon_mut_ptr_xxx()` calls.
 @param key New key if `ctx->ctn` is object, or NULL if `ctx->ctn` is array.
 @param val New value to be added.
 @return true on success or false on fail.
 */
ctoon_api_inline bool ctoon_ptr_ctx_append(ctoon_ptr_ctx *ctx,
                                             ctoon_mut_val *key,
                                             ctoon_mut_val *val);

/**
 Replace value by TOON pointer context.
 @param ctx The context from the `ctoon_mut_ptr_xxx()` calls.
 @param val New value to be replaced.
 @return true on success or false on fail.
 @note If success, the old value will be returned via `ctx->old`.
 */
ctoon_api_inline bool ctoon_ptr_ctx_replace(ctoon_ptr_ctx *ctx,
                                              ctoon_mut_val *val);

/**
 Remove value by TOON pointer context.
 @param ctx The context from the `ctoon_mut_ptr_xxx()` calls.
 @return true on success or false on fail.
 @note If success, the old value will be returned via `ctx->old`.
 */
ctoon_api_inline bool ctoon_ptr_ctx_remove(ctoon_ptr_ctx *ctx);



/*==============================================================================
 * MARK: - TOON Patch API (RFC 6902)
 * https://tools.ietf.org/html/rfc6902
 *============================================================================*/

/** Result code for TOON patch. */
typedef uint32_t ctoon_patch_code;

/** Success, no error. */
static const ctoon_patch_code CTOON_PATCH_SUCCESS = 0;

/** Invalid parameter, such as NULL input or non-array patch. */
static const ctoon_patch_code CTOON_PATCH_ERROR_INVALID_PARAMETER = 1;

/** Memory allocation failure occurs. */
static const ctoon_patch_code CTOON_PATCH_ERROR_MEMORY_ALLOCATION = 2;

/** TOON patch operation is not object type. */
static const ctoon_patch_code CTOON_PATCH_ERROR_INVALID_OPERATION = 3;

/** TOON patch operation is missing a required key. */
static const ctoon_patch_code CTOON_PATCH_ERROR_MISSING_KEY = 4;

/** TOON patch operation member is invalid. */
static const ctoon_patch_code CTOON_PATCH_ERROR_INVALID_MEMBER = 5;

/** TOON patch operation `test` not equal. */
static const ctoon_patch_code CTOON_PATCH_ERROR_EQUAL = 6;

/** TOON patch operation failed on TOON pointer. */
static const ctoon_patch_code CTOON_PATCH_ERROR_POINTER = 7;

/** Error information for TOON patch. */
typedef struct ctoon_patch_err {
    /** Error code, see `ctoon_patch_code` for all possible values. */
    ctoon_patch_code code;
    /** Index of the error operation (0 if no error). */
    size_t idx;
    /** Error message, constant, no need to free (NULL if no error). */
    const char *msg;
    /** TOON pointer error if `code == CTOON_PATCH_ERROR_POINTER`. */
    ctoon_ptr_err ptr;
} ctoon_patch_err;

/**
 Creates and returns a patched TOON value (RFC 6902).
 The memory of the returned value is allocated by the `doc`.
 The `err` is used to receive error information, pass NULL if not needed.
 Returns NULL if the patch could not be applied.
 */
ctoon_api ctoon_mut_val *ctoon_patch(ctoon_mut_doc *doc,
                                        ctoon_val *orig,
                                        ctoon_val *patch,
                                        ctoon_patch_err *err);

/**
 Creates and returns a patched TOON value (RFC 6902).
 The memory of the returned value is allocated by the `doc`.
 The `err` is used to receive error information, pass NULL if not needed.
 Returns NULL if the patch could not be applied.
 */
ctoon_api ctoon_mut_val *ctoon_mut_patch(ctoon_mut_doc *doc,
                                            ctoon_mut_val *orig,
                                            ctoon_mut_val *patch,
                                            ctoon_patch_err *err);



/*==============================================================================
 * MARK: - TOON Merge-Patch API (RFC 7386)
 * https://tools.ietf.org/html/rfc7386
 *============================================================================*/

/**
 Creates and returns a merge-patched TOON value (RFC 7386).
 The memory of the returned value is allocated by the `doc`.
 Returns NULL if the patch could not be applied.

 @warning This function is recursive and may cause a stack overflow if the
    object level is too deep.
 */
ctoon_api ctoon_mut_val *ctoon_merge_patch(ctoon_mut_doc *doc,
                                              ctoon_val *orig,
                                              ctoon_val *patch);

/**
 Creates and returns a merge-patched TOON value (RFC 7386).
 The memory of the returned value is allocated by the `doc`.
 Returns NULL if the patch could not be applied.

 @warning This function is recursive and may cause a stack overflow if the
    object level is too deep.
 */
ctoon_api ctoon_mut_val *ctoon_mut_merge_patch(ctoon_mut_doc *doc,
                                                  ctoon_mut_val *orig,
                                                  ctoon_mut_val *patch);

#endif /* CTOON_DISABLE_UTILS */



/*==============================================================================
 * MARK: - TOON Structure (Implementation)
 *============================================================================*/

/** Payload of a TOON value (8 bytes). */
typedef union ctoon_val_uni {
    uint64_t    u64;
    int64_t     i64;
    double      f64;
    const char *str;
    void       *ptr;
    size_t      ofs;
} ctoon_val_uni;

/**
 Immutable TOON value, 16 bytes.
 */
struct ctoon_val {
    uint64_t tag; /**< type, subtype and length */
    ctoon_val_uni uni; /**< payload */
};

struct ctoon_doc {
    /** Root value of the document (nonnull). */
    ctoon_val *root;
    /** Allocator used by document (nonnull). */
    ctoon_alc alc;
    /** The total number of bytes read when parsing TOON (nonzero). */
    size_t dat_read;
    /** The total number of value read when parsing TOON (nonzero). */
    size_t val_read;
    /** The string pool used by TOON values (nullable). */
    char *str_pool;
};



/*==============================================================================
 * MARK: - Unsafe TOON Value API (Implementation)
 *============================================================================*/

/*
 Whether the string does not need to be escaped for serialization.
 This function is used to optimize the writing speed of small constant strings.
 This function works only if the compiler can evaluate it at compile time.

 Clang supports it since v8.0,
    earlier versions do not support constant_p(strlen) and return false.
 GCC supports it since at least v4.4,
    earlier versions may compile it as run-time instructions.
 ICC supports it since at least v16,
    earlier versions are uncertain.

 @param str The C string.
 @param len The returnd value from strlen(str).
 */
ctoon_api_inline bool unsafe_ctoon_is_str_noesc(const char *str, size_t len) {
#if CTOON_HAS_CONSTANT_P && \
    (!CTOON_IS_REAL_GCC || ctoon_gcc_available(4, 4, 0))
    if (ctoon_constant_p(len) && len <= 32) {
        /*
         Same as the following loop:

         for (size_t i = 0; i < len; i++) {
             char c = str[i];
             if (c < ' ' || c > '~' || c == '"' || c == '\\') return false;
         }

         GCC evaluates it at compile time only if the string length is within 17
         and -O3 (which turns on the -fpeel-loops flag) is used.
         So the loop is unrolled for GCC.
         */
#       define ctoon_repeat32_incr(x) \
            x(0)  x(1)  x(2)  x(3)  x(4)  x(5)  x(6)  x(7)  \
            x(8)  x(9)  x(10) x(11) x(12) x(13) x(14) x(15) \
            x(16) x(17) x(18) x(19) x(20) x(21) x(22) x(23) \
            x(24) x(25) x(26) x(27) x(28) x(29) x(30) x(31)
#       define ctoon_check_char_noesc(i) \
            if (i < len) { \
                char c = str[i]; \
                if (c < ' ' || c > '~' || c == '"' || c == '\\') return false; }
        ctoon_repeat32_incr(ctoon_check_char_noesc)
#       undef ctoon_repeat32_incr
#       undef ctoon_check_char_noesc
        return true;
    }
#else
    (void)str;
    (void)len;
#endif
    return false;
}

ctoon_api_inline double unsafe_ctoon_u64_to_f64(uint64_t num) {
#if CTOON_U64_TO_F64_NO_IMPL
        uint64_t msb = ((uint64_t)1) << 63;
        if ((num & msb) == 0) {
            return (double)(int64_t)num;
        } else {
            return ((double)(int64_t)((num >> 1) | (num & 1))) * (double)2.0;
        }
#else
        return (double)num;
#endif
}

ctoon_api_inline ctoon_type unsafe_ctoon_get_type(void *val) {
    uint8_t tag = (uint8_t)((ctoon_val *)val)->tag;
    return (ctoon_type)(tag & CTOON_TYPE_MASK);
}

ctoon_api_inline ctoon_subtype unsafe_ctoon_get_subtype(void *val) {
    uint8_t tag = (uint8_t)((ctoon_val *)val)->tag;
    return (ctoon_subtype)(tag & CTOON_SUBTYPE_MASK);
}

ctoon_api_inline uint8_t unsafe_ctoon_get_tag(void *val) {
    uint8_t tag = (uint8_t)((ctoon_val *)val)->tag;
    return (uint8_t)(tag & CTOON_TAG_MASK);
}

ctoon_api_inline bool unsafe_ctoon_is_raw(void *val) {
    return unsafe_ctoon_get_type(val) == CTOON_TYPE_RAW;
}

ctoon_api_inline bool unsafe_ctoon_is_null(void *val) {
    return unsafe_ctoon_get_type(val) == CTOON_TYPE_NULL;
}

ctoon_api_inline bool unsafe_ctoon_is_bool(void *val) {
    return unsafe_ctoon_get_type(val) == CTOON_TYPE_BOOL;
}

ctoon_api_inline bool unsafe_ctoon_is_num(void *val) {
    return unsafe_ctoon_get_type(val) == CTOON_TYPE_NUM;
}

ctoon_api_inline bool unsafe_ctoon_is_str(void *val) {
    return unsafe_ctoon_get_type(val) == CTOON_TYPE_STR;
}

ctoon_api_inline bool unsafe_ctoon_is_arr(void *val) {
    return unsafe_ctoon_get_type(val) == CTOON_TYPE_ARR;
}

ctoon_api_inline bool unsafe_ctoon_is_obj(void *val) {
    return unsafe_ctoon_get_type(val) == CTOON_TYPE_OBJ;
}

ctoon_api_inline bool unsafe_ctoon_is_ctn(void *val) {
    uint8_t mask = CTOON_TYPE_ARR & CTOON_TYPE_OBJ;
    return (unsafe_ctoon_get_tag(val) & mask) == mask;
}

ctoon_api_inline bool unsafe_ctoon_is_uint(void *val) {
    const uint8_t patt = CTOON_TYPE_NUM | CTOON_SUBTYPE_UINT;
    return unsafe_ctoon_get_tag(val) == patt;
}

ctoon_api_inline bool unsafe_ctoon_is_sint(void *val) {
    const uint8_t patt = CTOON_TYPE_NUM | CTOON_SUBTYPE_SINT;
    return unsafe_ctoon_get_tag(val) == patt;
}

ctoon_api_inline bool unsafe_ctoon_is_int(void *val) {
    const uint8_t mask = CTOON_TAG_MASK & (~CTOON_SUBTYPE_SINT);
    const uint8_t patt = CTOON_TYPE_NUM | CTOON_SUBTYPE_UINT;
    return (unsafe_ctoon_get_tag(val) & mask) == patt;
}

ctoon_api_inline bool unsafe_ctoon_is_real(void *val) {
    const uint8_t patt = CTOON_TYPE_NUM | CTOON_SUBTYPE_REAL;
    return unsafe_ctoon_get_tag(val) == patt;
}

ctoon_api_inline bool unsafe_ctoon_is_true(void *val) {
    const uint8_t patt = CTOON_TYPE_BOOL | CTOON_SUBTYPE_TRUE;
    return unsafe_ctoon_get_tag(val) == patt;
}

ctoon_api_inline bool unsafe_ctoon_is_false(void *val) {
    const uint8_t patt = CTOON_TYPE_BOOL | CTOON_SUBTYPE_FALSE;
    return unsafe_ctoon_get_tag(val) == patt;
}

ctoon_api_inline bool unsafe_ctoon_arr_is_flat(ctoon_val *val) {
    size_t ofs = val->uni.ofs;
    size_t len = (size_t)(val->tag >> CTOON_TAG_BIT);
    return len * sizeof(ctoon_val) + sizeof(ctoon_val) == ofs;
}

ctoon_api_inline const char *unsafe_ctoon_get_raw(void *val) {
    return ((ctoon_val *)val)->uni.str;
}

ctoon_api_inline bool unsafe_ctoon_get_bool(void *val) {
    uint8_t tag = unsafe_ctoon_get_tag(val);
    return (bool)((tag & CTOON_SUBTYPE_MASK) >> CTOON_TYPE_BIT);
}

ctoon_api_inline uint64_t unsafe_ctoon_get_uint(void *val) {
    return ((ctoon_val *)val)->uni.u64;
}

ctoon_api_inline int64_t unsafe_ctoon_get_sint(void *val) {
    return ((ctoon_val *)val)->uni.i64;
}

ctoon_api_inline int unsafe_ctoon_get_int(void *val) {
    return (int)((ctoon_val *)val)->uni.i64;
}

ctoon_api_inline double unsafe_ctoon_get_real(void *val) {
    return ((ctoon_val *)val)->uni.f64;
}

ctoon_api_inline double unsafe_ctoon_get_num(void *val) {
    uint8_t tag = unsafe_ctoon_get_tag(val);
    if (tag == (CTOON_TYPE_NUM | CTOON_SUBTYPE_REAL)) {
        return ((ctoon_val *)val)->uni.f64;
    } else if (tag == (CTOON_TYPE_NUM | CTOON_SUBTYPE_SINT)) {
        return (double)((ctoon_val *)val)->uni.i64;
    } else if (tag == (CTOON_TYPE_NUM | CTOON_SUBTYPE_UINT)) {
        return unsafe_ctoon_u64_to_f64(((ctoon_val *)val)->uni.u64);
    }
    return 0.0;
}

ctoon_api_inline const char *unsafe_ctoon_get_str(void *val) {
    return ((ctoon_val *)val)->uni.str;
}

ctoon_api_inline size_t unsafe_ctoon_get_len(void *val) {
    return (size_t)(((ctoon_val *)val)->tag >> CTOON_TAG_BIT);
}

ctoon_api_inline ctoon_val *unsafe_ctoon_get_first(ctoon_val *ctn) {
    return ctn + 1;
}

ctoon_api_inline ctoon_val *unsafe_ctoon_get_next(ctoon_val *val) {
    bool is_ctn = unsafe_ctoon_is_ctn(val);
    size_t ctn_ofs = val->uni.ofs;
    size_t ofs = (is_ctn ? ctn_ofs : sizeof(ctoon_val));
    return (ctoon_val *)(void *)((uint8_t *)val + ofs);
}

ctoon_api_inline bool unsafe_ctoon_equals_strn(void *val, const char *str,
                                                 size_t len) {
    return unsafe_ctoon_get_len(val) == len &&
           memcmp(((ctoon_val *)val)->uni.str, str, len) == 0;
}

ctoon_api_inline bool unsafe_ctoon_equals_str(void *val, const char *str) {
    return unsafe_ctoon_equals_strn(val, str, strlen(str));
}

ctoon_api_inline void unsafe_ctoon_set_type(void *val, ctoon_type type,
                                              ctoon_subtype subtype) {
    uint8_t tag = (type | subtype);
    uint64_t new_tag = ((ctoon_val *)val)->tag;
    new_tag = (new_tag & (~(uint64_t)CTOON_TAG_MASK)) | (uint64_t)tag;
    ((ctoon_val *)val)->tag = new_tag;
}

ctoon_api_inline void unsafe_ctoon_set_len(void *val, size_t len) {
    uint64_t tag = ((ctoon_val *)val)->tag & CTOON_TAG_MASK;
    tag |= (uint64_t)len << CTOON_TAG_BIT;
    ((ctoon_val *)val)->tag = tag;
}

ctoon_api_inline void unsafe_ctoon_set_tag(void *val, ctoon_type type,
                                             ctoon_subtype subtype,
                                             size_t len) {
    uint64_t tag = (uint64_t)len << CTOON_TAG_BIT;
    tag |= (type | subtype);
    ((ctoon_val *)val)->tag = tag;
}

ctoon_api_inline void unsafe_ctoon_inc_len(void *val) {
    uint64_t tag = ((ctoon_val *)val)->tag;
    tag += (uint64_t)(1 << CTOON_TAG_BIT);
    ((ctoon_val *)val)->tag = tag;
}

ctoon_api_inline void unsafe_ctoon_set_raw(void *val, const char *raw,
                                             size_t len) {
    unsafe_ctoon_set_tag(val, CTOON_TYPE_RAW, CTOON_SUBTYPE_NONE, len);
    ((ctoon_val *)val)->uni.str = raw;
}

ctoon_api_inline void unsafe_ctoon_set_null(void *val) {
    unsafe_ctoon_set_tag(val, CTOON_TYPE_NULL, CTOON_SUBTYPE_NONE, 0);
}

ctoon_api_inline void unsafe_ctoon_set_bool(void *val, bool num) {
    ctoon_subtype subtype = num ? CTOON_SUBTYPE_TRUE : CTOON_SUBTYPE_FALSE;
    unsafe_ctoon_set_tag(val, CTOON_TYPE_BOOL, subtype, 0);
}

ctoon_api_inline void unsafe_ctoon_set_uint(void *val, uint64_t num) {
    unsafe_ctoon_set_tag(val, CTOON_TYPE_NUM, CTOON_SUBTYPE_UINT, 0);
    ((ctoon_val *)val)->uni.u64 = num;
}

ctoon_api_inline void unsafe_ctoon_set_sint(void *val, int64_t num) {
    unsafe_ctoon_set_tag(val, CTOON_TYPE_NUM, CTOON_SUBTYPE_SINT, 0);
    ((ctoon_val *)val)->uni.i64 = num;
}

ctoon_api_inline void unsafe_ctoon_set_fp_to_fixed(void *val, int prec) {
    ((ctoon_val *)val)->tag &= ~((uint64_t)CTOON_WRITE_FP_TO_FIXED(15) << 32);
    ((ctoon_val *)val)->tag |= (uint64_t)CTOON_WRITE_FP_TO_FIXED(prec) << 32;
}

ctoon_api_inline void unsafe_ctoon_set_fp_to_float(void *val, bool flt) {
    uint64_t flag = (uint64_t)CTOON_WRITE_FP_TO_FLOAT << 32;
    if (flt) ((ctoon_val *)val)->tag |= flag;
    else ((ctoon_val *)val)->tag &= ~flag;
}

ctoon_api_inline void unsafe_ctoon_set_float(void *val, float num) {
    unsafe_ctoon_set_tag(val, CTOON_TYPE_NUM, CTOON_SUBTYPE_REAL, 0);
    ((ctoon_val *)val)->tag |= (uint64_t)CTOON_WRITE_FP_TO_FLOAT << 32;
    ((ctoon_val *)val)->uni.f64 = (double)num;
}

ctoon_api_inline void unsafe_ctoon_set_double(void *val, double num) {
    unsafe_ctoon_set_tag(val, CTOON_TYPE_NUM, CTOON_SUBTYPE_REAL, 0);
    ((ctoon_val *)val)->uni.f64 = num;
}

ctoon_api_inline void unsafe_ctoon_set_real(void *val, double num) {
    unsafe_ctoon_set_tag(val, CTOON_TYPE_NUM, CTOON_SUBTYPE_REAL, 0);
    ((ctoon_val *)val)->uni.f64 = num;
}

ctoon_api_inline void unsafe_ctoon_set_str_noesc(void *val, bool noesc) {
    ((ctoon_val *)val)->tag &= ~(uint64_t)CTOON_SUBTYPE_MASK;
    if (noesc) ((ctoon_val *)val)->tag |= (uint64_t)CTOON_SUBTYPE_NOESC;
}

ctoon_api_inline void unsafe_ctoon_set_strn(void *val, const char *str,
                                              size_t len) {
    unsafe_ctoon_set_tag(val, CTOON_TYPE_STR, CTOON_SUBTYPE_NONE, len);
    ((ctoon_val *)val)->uni.str = str;
}

ctoon_api_inline void unsafe_ctoon_set_str(void *val, const char *str) {
    size_t len = strlen(str);
    bool noesc = unsafe_ctoon_is_str_noesc(str, len);
    ctoon_subtype subtype = noesc ? CTOON_SUBTYPE_NOESC : CTOON_SUBTYPE_NONE;
    unsafe_ctoon_set_tag(val, CTOON_TYPE_STR, subtype, len);
    ((ctoon_val *)val)->uni.str = str;
}

ctoon_api_inline void unsafe_ctoon_set_arr(void *val, size_t size) {
    unsafe_ctoon_set_tag(val, CTOON_TYPE_ARR, CTOON_SUBTYPE_NONE, size);
}

ctoon_api_inline void unsafe_ctoon_set_obj(void *val, size_t size) {
    unsafe_ctoon_set_tag(val, CTOON_TYPE_OBJ, CTOON_SUBTYPE_NONE, size);
}



/*==============================================================================
 * MARK: - TOON Document API (Implementation)
 *============================================================================*/

ctoon_api_inline ctoon_val *ctoon_doc_get_root(ctoon_doc *doc) {
    return doc ? doc->root : NULL;
}

ctoon_api_inline size_t ctoon_doc_get_read_size(ctoon_doc *doc) {
    return doc ? doc->dat_read : 0;
}

ctoon_api_inline size_t ctoon_doc_get_val_count(ctoon_doc *doc) {
    return doc ? doc->val_read : 0;
}

ctoon_api_inline void ctoon_doc_free(ctoon_doc *doc) {
    if (doc) {
        ctoon_alc alc = doc->alc;
        memset(&doc->alc, 0, sizeof(alc));
        if (doc->str_pool) alc.free(alc.ctx, doc->str_pool);
        alc.free(alc.ctx, doc);
    }
}



/*==============================================================================
 * MARK: - TOON Value Type API (Implementation)
 *============================================================================*/

ctoon_api_inline bool ctoon_is_raw(ctoon_val *val) {
    return val ? unsafe_ctoon_is_raw(val) : false;
}

ctoon_api_inline bool ctoon_is_null(ctoon_val *val) {
    return val ? unsafe_ctoon_is_null(val) : false;
}

ctoon_api_inline bool ctoon_is_true(ctoon_val *val) {
    return val ? unsafe_ctoon_is_true(val) : false;
}

ctoon_api_inline bool ctoon_is_false(ctoon_val *val) {
    return val ? unsafe_ctoon_is_false(val) : false;
}

ctoon_api_inline bool ctoon_is_bool(ctoon_val *val) {
    return val ? unsafe_ctoon_is_bool(val) : false;
}

ctoon_api_inline bool ctoon_is_uint(ctoon_val *val) {
    return val ? unsafe_ctoon_is_uint(val) : false;
}

ctoon_api_inline bool ctoon_is_sint(ctoon_val *val) {
    return val ? unsafe_ctoon_is_sint(val) : false;
}

ctoon_api_inline bool ctoon_is_int(ctoon_val *val) {
    return val ? unsafe_ctoon_is_int(val) : false;
}

ctoon_api_inline bool ctoon_is_real(ctoon_val *val) {
    return val ? unsafe_ctoon_is_real(val) : false;
}

ctoon_api_inline bool ctoon_is_num(ctoon_val *val) {
    return val ? unsafe_ctoon_is_num(val) : false;
}

ctoon_api_inline bool ctoon_is_str(ctoon_val *val) {
    return val ? unsafe_ctoon_is_str(val) : false;
}

ctoon_api_inline bool ctoon_is_arr(ctoon_val *val) {
    return val ? unsafe_ctoon_is_arr(val) : false;
}

ctoon_api_inline bool ctoon_is_obj(ctoon_val *val) {
    return val ? unsafe_ctoon_is_obj(val) : false;
}

ctoon_api_inline bool ctoon_is_ctn(ctoon_val *val) {
    return val ? unsafe_ctoon_is_ctn(val) : false;
}



/*==============================================================================
 * MARK: - TOON Value Content API (Implementation)
 *============================================================================*/

ctoon_api_inline ctoon_type ctoon_get_type(ctoon_val *val) {
    return val ? unsafe_ctoon_get_type(val) : CTOON_TYPE_NONE;
}

ctoon_api_inline ctoon_subtype ctoon_get_subtype(ctoon_val *val) {
    return val ? unsafe_ctoon_get_subtype(val) : CTOON_SUBTYPE_NONE;
}

ctoon_api_inline uint8_t ctoon_get_tag(ctoon_val *val) {
    return val ? unsafe_ctoon_get_tag(val) : 0;
}

ctoon_api_inline const char *ctoon_get_type_desc(ctoon_val *val) {
    switch (ctoon_get_tag(val)) {
        case CTOON_TYPE_RAW  | CTOON_SUBTYPE_NONE:  return "raw";
        case CTOON_TYPE_NULL | CTOON_SUBTYPE_NONE:  return "null";
        case CTOON_TYPE_STR  | CTOON_SUBTYPE_NONE:  return "string";
        case CTOON_TYPE_STR  | CTOON_SUBTYPE_NOESC: return "string";
        case CTOON_TYPE_ARR  | CTOON_SUBTYPE_NONE:  return "array";
        case CTOON_TYPE_OBJ  | CTOON_SUBTYPE_NONE:  return "object";
        case CTOON_TYPE_BOOL | CTOON_SUBTYPE_TRUE:  return "true";
        case CTOON_TYPE_BOOL | CTOON_SUBTYPE_FALSE: return "false";
        case CTOON_TYPE_NUM  | CTOON_SUBTYPE_UINT:  return "uint";
        case CTOON_TYPE_NUM  | CTOON_SUBTYPE_SINT:  return "sint";
        case CTOON_TYPE_NUM  | CTOON_SUBTYPE_REAL:  return "real";
        default:                                      return "unknown";
    }
}

ctoon_api_inline const char *ctoon_get_raw(ctoon_val *val) {
    return ctoon_is_raw(val) ? unsafe_ctoon_get_raw(val) : NULL;
}

ctoon_api_inline bool ctoon_get_bool(ctoon_val *val) {
    return ctoon_is_bool(val) ? unsafe_ctoon_get_bool(val) : false;
}

ctoon_api_inline uint64_t ctoon_get_uint(ctoon_val *val) {
    return ctoon_is_int(val) ? unsafe_ctoon_get_uint(val) : 0;
}

ctoon_api_inline int64_t ctoon_get_sint(ctoon_val *val) {
    return ctoon_is_int(val) ? unsafe_ctoon_get_sint(val) : 0;
}

ctoon_api_inline int ctoon_get_int(ctoon_val *val) {
    return ctoon_is_int(val) ? unsafe_ctoon_get_int(val) : 0;
}

ctoon_api_inline double ctoon_get_real(ctoon_val *val) {
    return ctoon_is_real(val) ? unsafe_ctoon_get_real(val) : 0.0;
}

ctoon_api_inline double ctoon_get_num(ctoon_val *val) {
    return val ? unsafe_ctoon_get_num(val) : 0.0;
}

ctoon_api_inline const char *ctoon_get_str(ctoon_val *val) {
    return ctoon_is_str(val) ? unsafe_ctoon_get_str(val) : NULL;
}

ctoon_api_inline size_t ctoon_get_len(ctoon_val *val) {
    return val ? unsafe_ctoon_get_len(val) : 0;
}

ctoon_api_inline bool ctoon_equals_str(ctoon_val *val, const char *str) {
    if (ctoon_likely(val && str)) {
        return unsafe_ctoon_is_str(val) &&
               unsafe_ctoon_equals_str(val, str);
    }
    return false;
}

ctoon_api_inline bool ctoon_equals_strn(ctoon_val *val, const char *str,
                                          size_t len) {
    if (ctoon_likely(val && str)) {
        return unsafe_ctoon_is_str(val) &&
               unsafe_ctoon_equals_strn(val, str, len);
    }
    return false;
}

ctoon_api bool unsafe_ctoon_equals(ctoon_val *lhs, ctoon_val *rhs);

ctoon_api_inline bool ctoon_equals(ctoon_val *lhs, ctoon_val *rhs) {
    if (ctoon_unlikely(!lhs || !rhs)) return false;
    return unsafe_ctoon_equals(lhs, rhs);
}

ctoon_api_inline bool ctoon_set_raw(ctoon_val *val,
                                      const char *raw, size_t len) {
    if (ctoon_unlikely(!val || unsafe_ctoon_is_ctn(val))) return false;
    unsafe_ctoon_set_raw(val, raw, len);
    return true;
}

ctoon_api_inline bool ctoon_set_null(ctoon_val *val) {
    if (ctoon_unlikely(!val || unsafe_ctoon_is_ctn(val))) return false;
    unsafe_ctoon_set_null(val);
    return true;
}

ctoon_api_inline bool ctoon_set_bool(ctoon_val *val, bool num) {
    if (ctoon_unlikely(!val || unsafe_ctoon_is_ctn(val))) return false;
    unsafe_ctoon_set_bool(val, num);
    return true;
}

ctoon_api_inline bool ctoon_set_uint(ctoon_val *val, uint64_t num) {
    if (ctoon_unlikely(!val || unsafe_ctoon_is_ctn(val))) return false;
    unsafe_ctoon_set_uint(val, num);
    return true;
}

ctoon_api_inline bool ctoon_set_sint(ctoon_val *val, int64_t num) {
    if (ctoon_unlikely(!val || unsafe_ctoon_is_ctn(val))) return false;
    unsafe_ctoon_set_sint(val, num);
    return true;
}

ctoon_api_inline bool ctoon_set_int(ctoon_val *val, int num) {
    if (ctoon_unlikely(!val || unsafe_ctoon_is_ctn(val))) return false;
    unsafe_ctoon_set_sint(val, (int64_t)num);
    return true;
}

ctoon_api_inline bool ctoon_set_float(ctoon_val *val, float num) {
    if (ctoon_unlikely(!val || unsafe_ctoon_is_ctn(val))) return false;
    unsafe_ctoon_set_float(val, num);
    return true;
}

ctoon_api_inline bool ctoon_set_double(ctoon_val *val, double num) {
    if (ctoon_unlikely(!val || unsafe_ctoon_is_ctn(val))) return false;
    unsafe_ctoon_set_double(val, num);
    return true;
}

ctoon_api_inline bool ctoon_set_real(ctoon_val *val, double num) {
    if (ctoon_unlikely(!val || unsafe_ctoon_is_ctn(val))) return false;
    unsafe_ctoon_set_real(val, num);
    return true;
}

ctoon_api_inline bool ctoon_set_fp_to_fixed(ctoon_val *val, int prec) {
    if (ctoon_unlikely(!ctoon_is_real(val))) return false;
    unsafe_ctoon_set_fp_to_fixed(val, prec);
    return true;
}

ctoon_api_inline bool ctoon_set_fp_to_float(ctoon_val *val, bool flt) {
    if (ctoon_unlikely(!ctoon_is_real(val))) return false;
    unsafe_ctoon_set_fp_to_float(val, flt);
    return true;
}

ctoon_api_inline bool ctoon_set_str(ctoon_val *val, const char *str) {
    if (ctoon_unlikely(!val || unsafe_ctoon_is_ctn(val))) return false;
    if (ctoon_unlikely(!str)) return false;
    unsafe_ctoon_set_str(val, str);
    return true;
}

ctoon_api_inline bool ctoon_set_strn(ctoon_val *val,
                                       const char *str, size_t len) {
    if (ctoon_unlikely(!val || unsafe_ctoon_is_ctn(val))) return false;
    if (ctoon_unlikely(!str)) return false;
    unsafe_ctoon_set_strn(val, str, len);
    return true;
}

ctoon_api_inline bool ctoon_set_str_noesc(ctoon_val *val, bool noesc) {
    if (ctoon_unlikely(!ctoon_is_str(val))) return false;
    unsafe_ctoon_set_str_noesc(val, noesc);
    return true;
}



/*==============================================================================
 * MARK: - TOON Array API (Implementation)
 *============================================================================*/

ctoon_api_inline size_t ctoon_arr_size(ctoon_val *arr) {
    return ctoon_is_arr(arr) ? unsafe_ctoon_get_len(arr) : 0;
}

ctoon_api_inline ctoon_val *ctoon_arr_get(ctoon_val *arr, size_t idx) {
    if (ctoon_likely(ctoon_is_arr(arr))) {
        if (ctoon_likely(unsafe_ctoon_get_len(arr) > idx)) {
            ctoon_val *val = unsafe_ctoon_get_first(arr);
            if (unsafe_ctoon_arr_is_flat(arr)) {
                return val + idx;
            } else {
                while (idx-- > 0) val = unsafe_ctoon_get_next(val);
                return val;
            }
        }
    }
    return NULL;
}

ctoon_api_inline ctoon_val *ctoon_arr_get_first(ctoon_val *arr) {
    if (ctoon_likely(ctoon_is_arr(arr))) {
        if (ctoon_likely(unsafe_ctoon_get_len(arr) > 0)) {
            return unsafe_ctoon_get_first(arr);
        }
    }
    return NULL;
}

ctoon_api_inline ctoon_val *ctoon_arr_get_last(ctoon_val *arr) {
    if (ctoon_likely(ctoon_is_arr(arr))) {
        size_t len = unsafe_ctoon_get_len(arr);
        if (ctoon_likely(len > 0)) {
            ctoon_val *val = unsafe_ctoon_get_first(arr);
            if (unsafe_ctoon_arr_is_flat(arr)) {
                return val + (len - 1);
            } else {
                while (len-- > 1) val = unsafe_ctoon_get_next(val);
                return val;
            }
        }
    }
    return NULL;
}



/*==============================================================================
 * MARK: - TOON Array Iterator API (Implementation)
 *============================================================================*/

ctoon_api_inline bool ctoon_arr_iter_init(ctoon_val *arr,
                                            ctoon_arr_iter *iter) {
    if (ctoon_likely(ctoon_is_arr(arr) && iter)) {
        iter->idx = 0;
        iter->max = unsafe_ctoon_get_len(arr);
        iter->cur = unsafe_ctoon_get_first(arr);
        return true;
    }
    if (iter) memset(iter, 0, sizeof(ctoon_arr_iter));
    return false;
}

ctoon_api_inline ctoon_arr_iter ctoon_arr_iter_with(ctoon_val *arr) {
    ctoon_arr_iter iter;
    ctoon_arr_iter_init(arr, &iter);
    return iter;
}

ctoon_api_inline bool ctoon_arr_iter_has_next(ctoon_arr_iter *iter) {
    return iter ? iter->idx < iter->max : false;
}

ctoon_api_inline ctoon_val *ctoon_arr_iter_next(ctoon_arr_iter *iter) {
    ctoon_val *val;
    if (iter && iter->idx < iter->max) {
        val = iter->cur;
        iter->cur = unsafe_ctoon_get_next(val);
        iter->idx++;
        return val;
    }
    return NULL;
}



/*==============================================================================
 * MARK: - TOON Object API (Implementation)
 *============================================================================*/

ctoon_api_inline size_t ctoon_obj_size(ctoon_val *obj) {
    return ctoon_is_obj(obj) ? unsafe_ctoon_get_len(obj) : 0;
}

ctoon_api_inline ctoon_val *ctoon_obj_get(ctoon_val *obj,
                                             const char *key) {
    return ctoon_obj_getn(obj, key, key ? strlen(key) : 0);
}

ctoon_api_inline ctoon_val *ctoon_obj_getn(ctoon_val *obj,
                                              const char *_key,
                                              size_t key_len) {
    if (ctoon_likely(ctoon_is_obj(obj) && _key)) {
        size_t len = unsafe_ctoon_get_len(obj);
        ctoon_val *key = unsafe_ctoon_get_first(obj);
        while (len-- > 0) {
            if (unsafe_ctoon_equals_strn(key, _key, key_len)) return key + 1;
            key = unsafe_ctoon_get_next(key + 1);
        }
    }
    return NULL;
}



/*==============================================================================
 * MARK: - TOON Object Iterator API (Implementation)
 *============================================================================*/

ctoon_api_inline bool ctoon_obj_iter_init(ctoon_val *obj,
                                            ctoon_obj_iter *iter) {
    if (ctoon_likely(ctoon_is_obj(obj) && iter)) {
        iter->idx = 0;
        iter->max = unsafe_ctoon_get_len(obj);
        iter->cur = unsafe_ctoon_get_first(obj);
        iter->obj = obj;
        return true;
    }
    if (iter) memset(iter, 0, sizeof(ctoon_obj_iter));
    return false;
}

ctoon_api_inline ctoon_obj_iter ctoon_obj_iter_with(ctoon_val *obj) {
    ctoon_obj_iter iter;
    ctoon_obj_iter_init(obj, &iter);
    return iter;
}

ctoon_api_inline bool ctoon_obj_iter_has_next(ctoon_obj_iter *iter) {
    return iter ? iter->idx < iter->max : false;
}

ctoon_api_inline ctoon_val *ctoon_obj_iter_next(ctoon_obj_iter *iter) {
    if (iter && iter->idx < iter->max) {
        ctoon_val *key = iter->cur;
        iter->idx++;
        iter->cur = unsafe_ctoon_get_next(key + 1);
        return key;
    }
    return NULL;
}

ctoon_api_inline ctoon_val *ctoon_obj_iter_get_val(ctoon_val *key) {
    return key ? key + 1 : NULL;
}

ctoon_api_inline ctoon_val *ctoon_obj_iter_get(ctoon_obj_iter *iter,
                                                  const char *key) {
    return ctoon_obj_iter_getn(iter, key, key ? strlen(key) : 0);
}

ctoon_api_inline ctoon_val *ctoon_obj_iter_getn(ctoon_obj_iter *iter,
                                                   const char *key,
                                                   size_t key_len) {
    if (iter && key) {
        size_t idx = iter->idx;
        size_t max = iter->max;
        ctoon_val *cur = iter->cur;
        if (ctoon_unlikely(idx == max)) {
            idx = 0;
            cur = unsafe_ctoon_get_first(iter->obj);
        }
        while (idx++ < max) {
            ctoon_val *next = unsafe_ctoon_get_next(cur + 1);
            if (unsafe_ctoon_equals_strn(cur, key, key_len)) {
                iter->idx = idx;
                iter->cur = next;
                return cur + 1;
            }
            cur = next;
            if (idx == iter->max && iter->idx < iter->max) {
                idx = 0;
                max = iter->idx;
                cur = unsafe_ctoon_get_first(iter->obj);
            }
        }
    }
    return NULL;
}



/*==============================================================================
 * MARK: - Mutable TOON Structure (Implementation)
 *============================================================================*/

/**
 Mutable TOON value, 24 bytes.
 The 'tag' and 'uni' field is same as immutable value.
 The 'next' field links all elements inside the container to be a cycle.
 */
struct ctoon_mut_val {
    uint64_t tag; /**< type, subtype and length */
    ctoon_val_uni uni; /**< payload */
    ctoon_mut_val *next; /**< the next value in circular linked list */
};

/**
 A memory chunk in string memory pool.
 */
typedef struct ctoon_str_chunk {
    struct ctoon_str_chunk *next; /* next chunk linked list */
    size_t chunk_size; /* chunk size in bytes */
    /* char str[]; flexible array member */
} ctoon_str_chunk;

/**
 A memory pool to hold all strings in a mutable document.
 */
typedef struct ctoon_str_pool {
    char *cur; /* cursor inside current chunk */
    char *end; /* the end of current chunk */
    size_t chunk_size; /* chunk size in bytes while creating new chunk */
    size_t chunk_size_max; /* maximum chunk size in bytes */
    ctoon_str_chunk *chunks; /* a linked list of chunks, nullable */
} ctoon_str_pool;

/**
 A memory chunk in value memory pool.
 `sizeof(ctoon_val_chunk)` should not larger than `sizeof(ctoon_mut_val)`.
 */
typedef struct ctoon_val_chunk {
    struct ctoon_val_chunk *next; /* next chunk linked list */
    size_t chunk_size; /* chunk size in bytes */
    /* char pad[sizeof(ctoon_mut_val) - sizeof(ctoon_val_chunk)]; padding */
    /* ctoon_mut_val vals[]; flexible array member */
} ctoon_val_chunk;

/**
 A memory pool to hold all values in a mutable document.
 */
typedef struct ctoon_val_pool {
    ctoon_mut_val *cur; /* cursor inside current chunk */
    ctoon_mut_val *end; /* the end of current chunk */
    size_t chunk_size; /* chunk size in bytes while creating new chunk */
    size_t chunk_size_max; /* maximum chunk size in bytes */
    ctoon_val_chunk *chunks; /* a linked list of chunks, nullable */
} ctoon_val_pool;

struct ctoon_mut_doc {
    ctoon_mut_val *root; /**< root value of the TOON document, nullable */
    ctoon_alc alc; /**< a valid allocator, nonnull */
    ctoon_str_pool str_pool; /**< string memory pool */
    ctoon_val_pool val_pool; /**< value memory pool */
};

/* Ensures the capacity to at least equal to the specified byte length. */
ctoon_api bool unsafe_ctoon_str_pool_grow(ctoon_str_pool *pool,
                                            const ctoon_alc *alc,
                                            size_t len);

/* Ensures the capacity to at least equal to the specified value count. */
ctoon_api bool unsafe_ctoon_val_pool_grow(ctoon_val_pool *pool,
                                            const ctoon_alc *alc,
                                            size_t count);

/* Allocate memory for string. */
ctoon_api_inline char *unsafe_ctoon_mut_str_alc(ctoon_mut_doc *doc,
                                                  size_t len) {
    char *mem;
    const ctoon_alc *alc = &doc->alc;
    ctoon_str_pool *pool = &doc->str_pool;
    if (ctoon_unlikely((size_t)(pool->end - pool->cur) <= len)) {
        if (ctoon_unlikely(!unsafe_ctoon_str_pool_grow(pool, alc, len + 1))) {
            return NULL;
        }
    }
    mem = pool->cur;
    pool->cur = mem + len + 1;
    return mem;
}

ctoon_api_inline char *unsafe_ctoon_mut_strncpy(ctoon_mut_doc *doc,
                                                  const char *str, size_t len) {
    char *mem = unsafe_ctoon_mut_str_alc(doc, len);
    if (ctoon_unlikely(!mem)) return NULL;
    memcpy((void *)mem, (const void *)str, len);
    mem[len] = '\0';
    return mem;
}

ctoon_api_inline ctoon_mut_val *unsafe_ctoon_mut_val(ctoon_mut_doc *doc,
                                                        size_t count) {
    ctoon_mut_val *val;
    ctoon_alc *alc = &doc->alc;
    ctoon_val_pool *pool = &doc->val_pool;
    if (ctoon_unlikely((size_t)(pool->end - pool->cur) < count)) {
        if (ctoon_unlikely(!unsafe_ctoon_val_pool_grow(pool, alc, count))) {
            return NULL;
        }
    }
    val = pool->cur;
    pool->cur += count;
    return val;
}



/*==============================================================================
 * MARK: - Mutable TOON Document API (Implementation)
 *============================================================================*/

ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_get_root(ctoon_mut_doc *doc) {
    return doc ? doc->root : NULL;
}

ctoon_api_inline void ctoon_mut_doc_set_root(ctoon_mut_doc *doc,
                                               ctoon_mut_val *root) {
    if (doc) doc->root = root;
}



/*==============================================================================
 * MARK: - Mutable TOON Value Type API (Implementation)
 *============================================================================*/

ctoon_api_inline bool ctoon_mut_is_raw(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_raw(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_null(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_null(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_true(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_true(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_false(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_false(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_bool(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_bool(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_uint(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_uint(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_sint(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_sint(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_int(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_int(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_real(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_real(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_num(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_num(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_str(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_str(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_arr(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_arr(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_obj(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_obj(val) : false;
}

ctoon_api_inline bool ctoon_mut_is_ctn(ctoon_mut_val *val) {
    return val ? unsafe_ctoon_is_ctn(val) : false;
}



/*==============================================================================
 * MARK: - Mutable TOON Value Content API (Implementation)
 *============================================================================*/

ctoon_api_inline ctoon_type ctoon_mut_get_type(ctoon_mut_val *val) {
    return ctoon_get_type((ctoon_val *)val);
}

ctoon_api_inline ctoon_subtype ctoon_mut_get_subtype(ctoon_mut_val *val) {
    return ctoon_get_subtype((ctoon_val *)val);
}

ctoon_api_inline uint8_t ctoon_mut_get_tag(ctoon_mut_val *val) {
    return ctoon_get_tag((ctoon_val *)val);
}

ctoon_api_inline const char *ctoon_mut_get_type_desc(ctoon_mut_val *val) {
    return ctoon_get_type_desc((ctoon_val *)val);
}

ctoon_api_inline const char *ctoon_mut_get_raw(ctoon_mut_val *val) {
    return ctoon_get_raw((ctoon_val *)val);
}

ctoon_api_inline bool ctoon_mut_get_bool(ctoon_mut_val *val) {
    return ctoon_get_bool((ctoon_val *)val);
}

ctoon_api_inline uint64_t ctoon_mut_get_uint(ctoon_mut_val *val) {
    return ctoon_get_uint((ctoon_val *)val);
}

ctoon_api_inline int64_t ctoon_mut_get_sint(ctoon_mut_val *val) {
    return ctoon_get_sint((ctoon_val *)val);
}

ctoon_api_inline int ctoon_mut_get_int(ctoon_mut_val *val) {
    return ctoon_get_int((ctoon_val *)val);
}

ctoon_api_inline double ctoon_mut_get_real(ctoon_mut_val *val) {
    return ctoon_get_real((ctoon_val *)val);
}

ctoon_api_inline double ctoon_mut_get_num(ctoon_mut_val *val) {
    return ctoon_get_num((ctoon_val *)val);
}

ctoon_api_inline const char *ctoon_mut_get_str(ctoon_mut_val *val) {
    return ctoon_get_str((ctoon_val *)val);
}

ctoon_api_inline size_t ctoon_mut_get_len(ctoon_mut_val *val) {
    return ctoon_get_len((ctoon_val *)val);
}

ctoon_api_inline bool ctoon_mut_equals_str(ctoon_mut_val *val,
                                             const char *str) {
    return ctoon_equals_str((ctoon_val *)val, str);
}

ctoon_api_inline bool ctoon_mut_equals_strn(ctoon_mut_val *val,
                                              const char *str, size_t len) {
    return ctoon_equals_strn((ctoon_val *)val, str, len);
}

ctoon_api bool unsafe_ctoon_mut_equals(ctoon_mut_val *lhs,
                                         ctoon_mut_val *rhs);

ctoon_api_inline bool ctoon_mut_equals(ctoon_mut_val *lhs,
                                         ctoon_mut_val *rhs) {
    if (ctoon_unlikely(!lhs || !rhs)) return false;
    return unsafe_ctoon_mut_equals(lhs, rhs);
}

ctoon_api_inline bool ctoon_mut_set_raw(ctoon_mut_val *val,
                                          const char *raw, size_t len) {
    if (ctoon_unlikely(!val || !raw)) return false;
    unsafe_ctoon_set_raw(val, raw, len);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_null(ctoon_mut_val *val) {
    if (ctoon_unlikely(!val)) return false;
    unsafe_ctoon_set_null(val);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_bool(ctoon_mut_val *val, bool num) {
    if (ctoon_unlikely(!val)) return false;
    unsafe_ctoon_set_bool(val, num);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_uint(ctoon_mut_val *val, uint64_t num) {
    if (ctoon_unlikely(!val)) return false;
    unsafe_ctoon_set_uint(val, num);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_sint(ctoon_mut_val *val, int64_t num) {
    if (ctoon_unlikely(!val)) return false;
    unsafe_ctoon_set_sint(val, num);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_int(ctoon_mut_val *val, int num) {
    if (ctoon_unlikely(!val)) return false;
    unsafe_ctoon_set_sint(val, (int64_t)num);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_float(ctoon_mut_val *val, float num) {
    if (ctoon_unlikely(!val)) return false;
    unsafe_ctoon_set_float(val, num);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_double(ctoon_mut_val *val, double num) {
    if (ctoon_unlikely(!val)) return false;
    unsafe_ctoon_set_double(val, num);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_real(ctoon_mut_val *val, double num) {
    if (ctoon_unlikely(!val)) return false;
    unsafe_ctoon_set_real(val, num);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_fp_to_fixed(ctoon_mut_val *val,
                                                  int prec) {
    if (ctoon_unlikely(!ctoon_mut_is_real(val))) return false;
    unsafe_ctoon_set_fp_to_fixed(val, prec);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_fp_to_float(ctoon_mut_val *val,
                                                  bool flt) {
    if (ctoon_unlikely(!ctoon_mut_is_real(val))) return false;
    unsafe_ctoon_set_fp_to_float(val, flt);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_str(ctoon_mut_val *val,
                                          const char *str) {
    if (ctoon_unlikely(!val || !str)) return false;
    unsafe_ctoon_set_str(val, str);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_strn(ctoon_mut_val *val,
                                           const char *str, size_t len) {
    if (ctoon_unlikely(!val || !str)) return false;
    unsafe_ctoon_set_strn(val, str, len);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_str_noesc(ctoon_mut_val *val,
                                                bool noesc) {
    if (ctoon_unlikely(!ctoon_mut_is_str(val))) return false;
    unsafe_ctoon_set_str_noesc(val, noesc);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_arr(ctoon_mut_val *val) {
    if (ctoon_unlikely(!val)) return false;
    unsafe_ctoon_set_arr(val, 0);
    return true;
}

ctoon_api_inline bool ctoon_mut_set_obj(ctoon_mut_val *val) {
    if (ctoon_unlikely(!val)) return false;
    unsafe_ctoon_set_obj(val, 0);
    return true;
}



/*==============================================================================
 * MARK: - Mutable TOON Value Creation API (Implementation)
 *============================================================================*/

#define ctoon_mut_val_one(func) \
    if (ctoon_likely(doc)) { \
        ctoon_mut_val *val = unsafe_ctoon_mut_val(doc, 1); \
        if (ctoon_likely(val)) { \
            func \
            return val; \
        } \
    } \
    return NULL

#define ctoon_mut_val_one_str(func) \
    if (ctoon_likely(doc && str)) { \
        ctoon_mut_val *val = unsafe_ctoon_mut_val(doc, 1); \
        if (ctoon_likely(val)) { \
            func \
            return val; \
        } \
    } \
    return NULL

ctoon_api_inline ctoon_mut_val *ctoon_mut_raw(ctoon_mut_doc *doc,
                                                 const char *str) {
    ctoon_mut_val_one_str({ unsafe_ctoon_set_raw(val, str, strlen(str)); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_rawn(ctoon_mut_doc *doc,
                                                  const char *str,
                                                  size_t len) {
    ctoon_mut_val_one_str({ unsafe_ctoon_set_raw(val, str, len); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_rawcpy(ctoon_mut_doc *doc,
                                                    const char *str) {
    ctoon_mut_val_one_str({
        size_t len = strlen(str);
        char *new_str = unsafe_ctoon_mut_strncpy(doc, str, len);
        if (ctoon_unlikely(!new_str)) return NULL;
        unsafe_ctoon_set_raw(val, new_str, len);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_rawncpy(ctoon_mut_doc *doc,
                                                     const char *str,
                                                     size_t len) {
    ctoon_mut_val_one_str({
        char *new_str = unsafe_ctoon_mut_strncpy(doc, str, len);
        if (ctoon_unlikely(!new_str)) return NULL;
        unsafe_ctoon_set_raw(val, new_str, len);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_null(ctoon_mut_doc *doc) {
    ctoon_mut_val_one({ unsafe_ctoon_set_null(val); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_true(ctoon_mut_doc *doc) {
    ctoon_mut_val_one({ unsafe_ctoon_set_bool(val, true); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_false(ctoon_mut_doc *doc) {
    ctoon_mut_val_one({ unsafe_ctoon_set_bool(val, false); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_bool(ctoon_mut_doc *doc,
                                                  bool _val) {
    ctoon_mut_val_one({ unsafe_ctoon_set_bool(val, _val); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_uint(ctoon_mut_doc *doc,
                                                  uint64_t num) {
    ctoon_mut_val_one({ unsafe_ctoon_set_uint(val, num); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_sint(ctoon_mut_doc *doc,
                                                  int64_t num) {
    ctoon_mut_val_one({ unsafe_ctoon_set_sint(val, num); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_int(ctoon_mut_doc *doc,
                                                 int64_t num) {
    ctoon_mut_val_one({ unsafe_ctoon_set_sint(val, num); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_float(ctoon_mut_doc *doc,
                                                   float num) {
    ctoon_mut_val_one({ unsafe_ctoon_set_float(val, num); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_double(ctoon_mut_doc *doc,
                                                    double num) {
    ctoon_mut_val_one({ unsafe_ctoon_set_double(val, num); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_real(ctoon_mut_doc *doc,
                                                  double num) {
    ctoon_mut_val_one({ unsafe_ctoon_set_real(val, num); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_str(ctoon_mut_doc *doc,
                                                 const char *str) {
    ctoon_mut_val_one_str({ unsafe_ctoon_set_str(val, str); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_strn(ctoon_mut_doc *doc,
                                                  const char *str,
                                                  size_t len) {
    ctoon_mut_val_one_str({ unsafe_ctoon_set_strn(val, str, len); });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_strcpy(ctoon_mut_doc *doc,
                                                    const char *str) {
    ctoon_mut_val_one_str({
        size_t len = strlen(str);
        bool noesc = unsafe_ctoon_is_str_noesc(str, len);
        ctoon_subtype sub = noesc ? CTOON_SUBTYPE_NOESC : CTOON_SUBTYPE_NONE;
        char *new_str = unsafe_ctoon_mut_strncpy(doc, str, len);
        if (ctoon_unlikely(!new_str)) return NULL;
        unsafe_ctoon_set_tag(val, CTOON_TYPE_STR, sub, len);
        val->uni.str = new_str;
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_strncpy(ctoon_mut_doc *doc,
                                                     const char *str,
                                                     size_t len) {
    ctoon_mut_val_one_str({
        char *new_str = unsafe_ctoon_mut_strncpy(doc, str, len);
        if (ctoon_unlikely(!new_str)) return NULL;
        unsafe_ctoon_set_strn(val, new_str, len);
    });
}

#undef ctoon_mut_val_one
#undef ctoon_mut_val_one_str



/*==============================================================================
 * MARK: - Mutable TOON Array API (Implementation)
 *============================================================================*/

ctoon_api_inline size_t ctoon_mut_arr_size(ctoon_mut_val *arr) {
    return ctoon_mut_is_arr(arr) ? unsafe_ctoon_get_len(arr) : 0;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_get(ctoon_mut_val *arr,
                                                     size_t idx) {
    if (ctoon_likely(idx < ctoon_mut_arr_size(arr))) {
        ctoon_mut_val *val = (ctoon_mut_val *)arr->uni.ptr;
        while (idx-- > 0) val = val->next;
        return val->next;
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_get_first(
    ctoon_mut_val *arr) {
    if (ctoon_likely(ctoon_mut_arr_size(arr) > 0)) {
        return ((ctoon_mut_val *)arr->uni.ptr)->next;
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_get_last(
    ctoon_mut_val *arr) {
    if (ctoon_likely(ctoon_mut_arr_size(arr) > 0)) {
        return ((ctoon_mut_val *)arr->uni.ptr);
    }
    return NULL;
}



/*==============================================================================
 * MARK: - Mutable TOON Array Iterator API (Implementation)
 *============================================================================*/

ctoon_api_inline bool ctoon_mut_arr_iter_init(ctoon_mut_val *arr,
                                                ctoon_mut_arr_iter *iter) {
    if (ctoon_likely(ctoon_mut_is_arr(arr) && iter)) {
        iter->idx = 0;
        iter->max = unsafe_ctoon_get_len(arr);
        iter->cur = iter->max ? (ctoon_mut_val *)arr->uni.ptr : NULL;
        iter->pre = NULL;
        iter->arr = arr;
        return true;
    }
    if (iter) memset(iter, 0, sizeof(ctoon_mut_arr_iter));
    return false;
}

ctoon_api_inline ctoon_mut_arr_iter ctoon_mut_arr_iter_with(
    ctoon_mut_val *arr) {
    ctoon_mut_arr_iter iter;
    ctoon_mut_arr_iter_init(arr, &iter);
    return iter;
}

ctoon_api_inline bool ctoon_mut_arr_iter_has_next(ctoon_mut_arr_iter *iter) {
    return iter ? iter->idx < iter->max : false;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_iter_next(
    ctoon_mut_arr_iter *iter) {
    if (iter && iter->idx < iter->max) {
        ctoon_mut_val *val = iter->cur;
        iter->pre = val;
        iter->cur = val->next;
        iter->idx++;
        return iter->cur;
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_iter_remove(
    ctoon_mut_arr_iter *iter) {
    if (ctoon_likely(iter && 0 < iter->idx && iter->idx <= iter->max)) {
        ctoon_mut_val *prev = iter->pre;
        ctoon_mut_val *cur = iter->cur;
        ctoon_mut_val *next = cur->next;
        if (ctoon_unlikely(iter->idx == iter->max)) iter->arr->uni.ptr = prev;
        iter->idx--;
        iter->max--;
        unsafe_ctoon_set_len(iter->arr, iter->max);
        prev->next = next;
        iter->cur = prev;
        return cur;
    }
    return NULL;
}



/*==============================================================================
 * MARK: - Mutable TOON Array Creation API (Implementation)
 *============================================================================*/

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr(ctoon_mut_doc *doc) {
    if (ctoon_likely(doc)) {
        ctoon_mut_val *val = unsafe_ctoon_mut_val(doc, 1);
        if (ctoon_likely(val)) {
            val->tag = CTOON_TYPE_ARR | CTOON_SUBTYPE_NONE;
            return val;
        }
    }
    return NULL;
}

#define ctoon_mut_arr_with_func(func) \
    if (ctoon_likely(doc && ((0 < count && count < \
        (~(size_t)0) / sizeof(ctoon_mut_val) && vals) || count == 0))) { \
        ctoon_mut_val *arr = unsafe_ctoon_mut_val(doc, 1 + count); \
        if (ctoon_likely(arr)) { \
            arr->tag = ((uint64_t)count << CTOON_TAG_BIT) | CTOON_TYPE_ARR; \
            if (count > 0) { \
                size_t i; \
                for (i = 0; i < count; i++) { \
                    ctoon_mut_val *val = arr + i + 1; \
                    func \
                    val->next = val + 1; \
                } \
                arr[count].next = arr + 1; \
                arr->uni.ptr = arr + count; \
            } \
            return arr; \
        } \
    } \
    return NULL

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_bool(
    ctoon_mut_doc *doc, const bool *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_bool(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_sint(
    ctoon_mut_doc *doc, const int64_t *vals, size_t count) {
    return ctoon_mut_arr_with_sint64(doc, vals, count);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_uint(
    ctoon_mut_doc *doc, const uint64_t *vals, size_t count) {
    return ctoon_mut_arr_with_uint64(doc, vals, count);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_real(
    ctoon_mut_doc *doc, const double *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_real(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_sint8(
    ctoon_mut_doc *doc, const int8_t *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_sint(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_sint16(
    ctoon_mut_doc *doc, const int16_t *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_sint(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_sint32(
    ctoon_mut_doc *doc, const int32_t *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_sint(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_sint64(
    ctoon_mut_doc *doc, const int64_t *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_sint(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_uint8(
    ctoon_mut_doc *doc, const uint8_t *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_uint(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_uint16(
    ctoon_mut_doc *doc, const uint16_t *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_uint(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_uint32(
    ctoon_mut_doc *doc, const uint32_t *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_uint(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_uint64(
    ctoon_mut_doc *doc, const uint64_t *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_uint(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_float(
    ctoon_mut_doc *doc, const float *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_float(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_double(
    ctoon_mut_doc *doc, const double *vals, size_t count) {
    ctoon_mut_arr_with_func({
        unsafe_ctoon_set_double(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_str(
    ctoon_mut_doc *doc, const char **vals, size_t count) {
    ctoon_mut_arr_with_func({
        if (ctoon_unlikely(!vals[i])) return NULL;
        unsafe_ctoon_set_str(val, vals[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_strn(
    ctoon_mut_doc *doc, const char **vals, const size_t *lens, size_t count) {
    if (ctoon_unlikely(count > 0 && !lens)) return NULL;
    ctoon_mut_arr_with_func({
        if (ctoon_unlikely(!vals[i])) return NULL;
        unsafe_ctoon_set_strn(val, vals[i], lens[i]);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_strcpy(
    ctoon_mut_doc *doc, const char **vals, size_t count) {
    size_t len;
    const char *str, *new_str;
    ctoon_mut_arr_with_func({
        str = vals[i];
        if (ctoon_unlikely(!str)) return NULL;
        len = strlen(str);
        new_str = unsafe_ctoon_mut_strncpy(doc, str, len);
        if (ctoon_unlikely(!new_str)) return NULL;
        unsafe_ctoon_set_strn(val, new_str, len);
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_with_strncpy(
    ctoon_mut_doc *doc, const char **vals, const size_t *lens, size_t count) {
    size_t len;
    const char *str, *new_str;
    if (ctoon_unlikely(count > 0 && !lens)) return NULL;
    ctoon_mut_arr_with_func({
        str = vals[i];
        if (ctoon_unlikely(!str)) return NULL;
        len = lens[i];
        new_str = unsafe_ctoon_mut_strncpy(doc, str, len);
        if (ctoon_unlikely(!new_str)) return NULL;
        unsafe_ctoon_set_strn(val, new_str, len);
    });
}

#undef ctoon_mut_arr_with_func



/*==============================================================================
 * MARK: - Mutable TOON Array Modification API (Implementation)
 *============================================================================*/

ctoon_api_inline bool ctoon_mut_arr_insert(ctoon_mut_val *arr,
                                             ctoon_mut_val *val, size_t idx) {
    if (ctoon_likely(ctoon_mut_is_arr(arr) && val)) {
        size_t len = unsafe_ctoon_get_len(arr);
        if (ctoon_likely(idx <= len)) {
            unsafe_ctoon_set_len(arr, len + 1);
            if (len == 0) {
                val->next = val;
                arr->uni.ptr = val;
            } else {
                ctoon_mut_val *prev = ((ctoon_mut_val *)arr->uni.ptr);
                ctoon_mut_val *next = prev->next;
                if (idx == len) {
                    prev->next = val;
                    val->next = next;
                    arr->uni.ptr = val;
                } else {
                    while (idx-- > 0) {
                        prev = next;
                        next = next->next;
                    }
                    prev->next = val;
                    val->next = next;
                }
            }
            return true;
        }
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_append(ctoon_mut_val *arr,
                                             ctoon_mut_val *val) {
    if (ctoon_likely(ctoon_mut_is_arr(arr) && val)) {
        size_t len = unsafe_ctoon_get_len(arr);
        unsafe_ctoon_set_len(arr, len + 1);
        if (len == 0) {
            val->next = val;
        } else {
            ctoon_mut_val *prev = ((ctoon_mut_val *)arr->uni.ptr);
            ctoon_mut_val *next = prev->next;
            prev->next = val;
            val->next = next;
        }
        arr->uni.ptr = val;
        return true;
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_prepend(ctoon_mut_val *arr,
                                              ctoon_mut_val *val) {
    if (ctoon_likely(ctoon_mut_is_arr(arr) && val)) {
        size_t len = unsafe_ctoon_get_len(arr);
        unsafe_ctoon_set_len(arr, len + 1);
        if (len == 0) {
            val->next = val;
            arr->uni.ptr = val;
        } else {
            ctoon_mut_val *prev = ((ctoon_mut_val *)arr->uni.ptr);
            ctoon_mut_val *next = prev->next;
            prev->next = val;
            val->next = next;
        }
        return true;
    }
    return false;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_replace(ctoon_mut_val *arr,
                                                         size_t idx,
                                                         ctoon_mut_val *val) {
    if (ctoon_likely(ctoon_mut_is_arr(arr) && val)) {
        size_t len = unsafe_ctoon_get_len(arr);
        if (ctoon_likely(idx < len)) {
            if (ctoon_likely(len > 1)) {
                ctoon_mut_val *prev = ((ctoon_mut_val *)arr->uni.ptr);
                ctoon_mut_val *next = prev->next;
                while (idx-- > 0) {
                    prev = next;
                    next = next->next;
                }
                prev->next = val;
                val->next = next->next;
                if ((void *)next == arr->uni.ptr) arr->uni.ptr = val;
                return next;
            } else {
                ctoon_mut_val *prev = ((ctoon_mut_val *)arr->uni.ptr);
                val->next = val;
                arr->uni.ptr = val;
                return prev;
            }
        }
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_remove(ctoon_mut_val *arr,
                                                        size_t idx) {
    if (ctoon_likely(ctoon_mut_is_arr(arr))) {
        size_t len = unsafe_ctoon_get_len(arr);
        if (ctoon_likely(idx < len)) {
            unsafe_ctoon_set_len(arr, len - 1);
            if (ctoon_likely(len > 1)) {
                ctoon_mut_val *prev = ((ctoon_mut_val *)arr->uni.ptr);
                ctoon_mut_val *next = prev->next;
                while (idx-- > 0) {
                    prev = next;
                    next = next->next;
                }
                prev->next = next->next;
                if ((void *)next == arr->uni.ptr) arr->uni.ptr = prev;
                return next;
            } else {
                return ((ctoon_mut_val *)arr->uni.ptr);
            }
        }
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_remove_first(
    ctoon_mut_val *arr) {
    if (ctoon_likely(ctoon_mut_is_arr(arr))) {
        size_t len = unsafe_ctoon_get_len(arr);
        if (len > 1) {
            ctoon_mut_val *prev = ((ctoon_mut_val *)arr->uni.ptr);
            ctoon_mut_val *next = prev->next;
            prev->next = next->next;
            unsafe_ctoon_set_len(arr, len - 1);
            return next;
        } else if (len == 1) {
            ctoon_mut_val *prev = ((ctoon_mut_val *)arr->uni.ptr);
            unsafe_ctoon_set_len(arr, 0);
            return prev;
        }
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_remove_last(
    ctoon_mut_val *arr) {
    if (ctoon_likely(ctoon_mut_is_arr(arr))) {
        size_t len = unsafe_ctoon_get_len(arr);
        if (ctoon_likely(len > 1)) {
            ctoon_mut_val *prev = ((ctoon_mut_val *)arr->uni.ptr);
            ctoon_mut_val *next = prev->next;
            unsafe_ctoon_set_len(arr, len - 1);
            while (--len > 0) prev = prev->next;
            prev->next = next;
            next = (ctoon_mut_val *)arr->uni.ptr;
            arr->uni.ptr = prev;
            return next;
        } else if (len == 1) {
            ctoon_mut_val *prev = ((ctoon_mut_val *)arr->uni.ptr);
            unsafe_ctoon_set_len(arr, 0);
            return prev;
        }
    }
    return NULL;
}

ctoon_api_inline bool ctoon_mut_arr_remove_range(ctoon_mut_val *arr,
                                                   size_t _idx, size_t _len) {
    if (ctoon_likely(ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *prev, *next;
        bool tail_removed;
        size_t len = unsafe_ctoon_get_len(arr);
        if (ctoon_unlikely(_idx + _len > len)) return false;
        if (ctoon_unlikely(_len == 0)) return true;
        unsafe_ctoon_set_len(arr, len - _len);
        if (ctoon_unlikely(len == _len)) return true;
        tail_removed = (_idx + _len == len);
        prev = ((ctoon_mut_val *)arr->uni.ptr);
        while (_idx-- > 0) prev = prev->next;
        next = prev->next;
        while (_len-- > 0) next = next->next;
        prev->next = next;
        if (ctoon_unlikely(tail_removed)) arr->uni.ptr = prev;
        return true;
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_clear(ctoon_mut_val *arr) {
    if (ctoon_likely(ctoon_mut_is_arr(arr))) {
        unsafe_ctoon_set_len(arr, 0);
        return true;
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_rotate(ctoon_mut_val *arr,
                                             size_t idx) {
    if (ctoon_likely(ctoon_mut_is_arr(arr) &&
                      unsafe_ctoon_get_len(arr) > idx)) {
        ctoon_mut_val *val = (ctoon_mut_val *)arr->uni.ptr;
        while (idx-- > 0) val = val->next;
        arr->uni.ptr = (void *)val;
        return true;
    }
    return false;
}



/*==============================================================================
 * MARK: - Mutable TOON Array Modification Convenience API (Implementation)
 *============================================================================*/

ctoon_api_inline bool ctoon_mut_arr_add_val(ctoon_mut_val *arr,
                                              ctoon_mut_val *val) {
    return ctoon_mut_arr_append(arr, val);
}

ctoon_api_inline bool ctoon_mut_arr_add_null(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_null(doc);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_true(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_true(doc);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_false(ctoon_mut_doc *doc,
                                                ctoon_mut_val *arr) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_false(doc);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_bool(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr,
                                               bool _val) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_bool(doc, _val);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_uint(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr,
                                               uint64_t num) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_uint(doc, num);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_sint(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr,
                                               int64_t num) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_sint(doc, num);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_int(ctoon_mut_doc *doc,
                                              ctoon_mut_val *arr,
                                              int64_t num) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_sint(doc, num);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_float(ctoon_mut_doc *doc,
                                                ctoon_mut_val *arr,
                                                float num) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_float(doc, num);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_double(ctoon_mut_doc *doc,
                                                 ctoon_mut_val *arr,
                                                 double num) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_double(doc, num);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_real(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr,
                                               double num) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_real(doc, num);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_str(ctoon_mut_doc *doc,
                                              ctoon_mut_val *arr,
                                              const char *str) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_str(doc, str);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_strn(ctoon_mut_doc *doc,
                                               ctoon_mut_val *arr,
                                               const char *str, size_t len) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_strn(doc, str, len);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_strcpy(ctoon_mut_doc *doc,
                                                 ctoon_mut_val *arr,
                                                 const char *str) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_strcpy(doc, str);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_arr_add_strncpy(ctoon_mut_doc *doc,
                                                  ctoon_mut_val *arr,
                                                  const char *str, size_t len) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_strncpy(doc, str, len);
        return ctoon_mut_arr_append(arr, val);
    }
    return false;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_add_arr(ctoon_mut_doc *doc,
                                                         ctoon_mut_val *arr) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_arr(doc);
        return ctoon_mut_arr_append(arr, val) ? val : NULL;
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_arr_add_obj(ctoon_mut_doc *doc,
                                                         ctoon_mut_val *arr) {
    if (ctoon_likely(doc && ctoon_mut_is_arr(arr))) {
        ctoon_mut_val *val = ctoon_mut_obj(doc);
        return ctoon_mut_arr_append(arr, val) ? val : NULL;
    }
    return NULL;
}



/*==============================================================================
 * MARK: - Mutable TOON Object API (Implementation)
 *============================================================================*/

ctoon_api_inline size_t ctoon_mut_obj_size(ctoon_mut_val *obj) {
    return ctoon_mut_is_obj(obj) ? unsafe_ctoon_get_len(obj) : 0;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_get(ctoon_mut_val *obj,
                                                     const char *key) {
    return ctoon_mut_obj_getn(obj, key, key ? strlen(key) : 0);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_getn(ctoon_mut_val *obj,
                                                      const char *_key,
                                                      size_t key_len) {
    size_t len = ctoon_mut_obj_size(obj);
    if (ctoon_likely(len && _key)) {
        ctoon_mut_val *key = ((ctoon_mut_val *)obj->uni.ptr)->next->next;
        while (len-- > 0) {
            if (unsafe_ctoon_equals_strn(key, _key, key_len)) return key->next;
            key = key->next->next;
        }
    }
    return NULL;
}



/*==============================================================================
 * MARK: - Mutable TOON Object Iterator API (Implementation)
 *============================================================================*/

ctoon_api_inline bool ctoon_mut_obj_iter_init(ctoon_mut_val *obj,
                                                ctoon_mut_obj_iter *iter) {
    if (ctoon_likely(ctoon_mut_is_obj(obj) && iter)) {
        iter->idx = 0;
        iter->max = unsafe_ctoon_get_len(obj);
        iter->cur = iter->max ? (ctoon_mut_val *)obj->uni.ptr : NULL;
        iter->pre = NULL;
        iter->obj = obj;
        return true;
    }
    if (iter) memset(iter, 0, sizeof(ctoon_mut_obj_iter));
    return false;
}

ctoon_api_inline ctoon_mut_obj_iter ctoon_mut_obj_iter_with(
    ctoon_mut_val *obj) {
    ctoon_mut_obj_iter iter;
    ctoon_mut_obj_iter_init(obj, &iter);
    return iter;
}

ctoon_api_inline bool ctoon_mut_obj_iter_has_next(ctoon_mut_obj_iter *iter) {
    return iter ? iter->idx < iter->max : false;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_iter_next(
    ctoon_mut_obj_iter *iter) {
    if (iter && iter->idx < iter->max) {
        ctoon_mut_val *key = iter->cur;
        iter->pre = key;
        iter->cur = key->next->next;
        iter->idx++;
        return iter->cur;
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_iter_get_val(
    ctoon_mut_val *key) {
    return key ? key->next : NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_iter_remove(
    ctoon_mut_obj_iter *iter) {
    if (ctoon_likely(iter && 0 < iter->idx && iter->idx <= iter->max)) {
        ctoon_mut_val *prev = iter->pre;
        ctoon_mut_val *cur = iter->cur;
        ctoon_mut_val *next = cur->next->next;
        if (ctoon_unlikely(iter->idx == iter->max)) iter->obj->uni.ptr = prev;
        iter->idx--;
        iter->max--;
        unsafe_ctoon_set_len(iter->obj, iter->max);
        prev->next->next = next;
        iter->cur = prev;
        return cur->next;
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_iter_get(
    ctoon_mut_obj_iter *iter, const char *key) {
    return ctoon_mut_obj_iter_getn(iter, key, key ? strlen(key) : 0);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_iter_getn(
    ctoon_mut_obj_iter *iter, const char *key, size_t key_len) {
    if (iter && key) {
        size_t idx = 0;
        size_t max = iter->max;
        ctoon_mut_val *pre, *cur = iter->cur;
        while (idx++ < max) {
            pre = cur;
            cur = cur->next->next;
            if (unsafe_ctoon_equals_strn(cur, key, key_len)) {
                iter->idx += idx;
                if (iter->idx > max) iter->idx -= max + 1;
                iter->pre = pre;
                iter->cur = cur;
                return cur->next;
            }
        }
    }
    return NULL;
}



/*==============================================================================
 * MARK: - Mutable TOON Object Creation API (Implementation)
 *============================================================================*/

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj(ctoon_mut_doc *doc) {
    if (ctoon_likely(doc)) {
        ctoon_mut_val *val = unsafe_ctoon_mut_val(doc, 1);
        if (ctoon_likely(val)) {
            val->tag = CTOON_TYPE_OBJ | CTOON_SUBTYPE_NONE;
            return val;
        }
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_with_str(ctoon_mut_doc *doc,
                                                          const char **keys,
                                                          const char **vals,
                                                          size_t count) {
    if (ctoon_likely(doc && ((count > 0 && keys && vals) || (count == 0)))) {
        ctoon_mut_val *obj = unsafe_ctoon_mut_val(doc, 1 + count * 2);
        if (ctoon_likely(obj)) {
            obj->tag = ((uint64_t)count << CTOON_TAG_BIT) | CTOON_TYPE_OBJ;
            if (count > 0) {
                size_t i;
                for (i = 0; i < count; i++) {
                    ctoon_mut_val *key = obj + (i * 2 + 1);
                    ctoon_mut_val *val = obj + (i * 2 + 2);
                    uint64_t key_len = (uint64_t)strlen(keys[i]);
                    uint64_t val_len = (uint64_t)strlen(vals[i]);
                    key->tag = (key_len << CTOON_TAG_BIT) | CTOON_TYPE_STR;
                    val->tag = (val_len << CTOON_TAG_BIT) | CTOON_TYPE_STR;
                    key->uni.str = keys[i];
                    val->uni.str = vals[i];
                    key->next = val;
                    val->next = val + 1;
                }
                obj[count * 2].next = obj + 1;
                obj->uni.ptr = obj + (count * 2 - 1);
            }
            return obj;
        }
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_with_kv(ctoon_mut_doc *doc,
                                                         const char **pairs,
                                                         size_t count) {
    if (ctoon_likely(doc && ((count > 0 && pairs) || (count == 0)))) {
        ctoon_mut_val *obj = unsafe_ctoon_mut_val(doc, 1 + count * 2);
        if (ctoon_likely(obj)) {
            obj->tag = ((uint64_t)count << CTOON_TAG_BIT) | CTOON_TYPE_OBJ;
            if (count > 0) {
                size_t i;
                for (i = 0; i < count; i++) {
                    ctoon_mut_val *key = obj + (i * 2 + 1);
                    ctoon_mut_val *val = obj + (i * 2 + 2);
                    const char *key_str = pairs[i * 2 + 0];
                    const char *val_str = pairs[i * 2 + 1];
                    uint64_t key_len = (uint64_t)strlen(key_str);
                    uint64_t val_len = (uint64_t)strlen(val_str);
                    key->tag = (key_len << CTOON_TAG_BIT) | CTOON_TYPE_STR;
                    val->tag = (val_len << CTOON_TAG_BIT) | CTOON_TYPE_STR;
                    key->uni.str = key_str;
                    val->uni.str = val_str;
                    key->next = val;
                    val->next = val + 1;
                }
                obj[count * 2].next = obj + 1;
                obj->uni.ptr = obj + (count * 2 - 1);
            }
            return obj;
        }
    }
    return NULL;
}



/*==============================================================================
 * MARK: - Mutable TOON Object Modification API (Implementation)
 *============================================================================*/

ctoon_api_inline void unsafe_ctoon_mut_obj_add(ctoon_mut_val *obj,
                                                 ctoon_mut_val *key,
                                                 ctoon_mut_val *val,
                                                 size_t len) {
    if (ctoon_likely(len)) {
        ctoon_mut_val *prev_val = ((ctoon_mut_val *)obj->uni.ptr)->next;
        ctoon_mut_val *next_key = prev_val->next;
        prev_val->next = key;
        val->next = next_key;
    } else {
        val->next = key;
    }
    key->next = val;
    obj->uni.ptr = (void *)key;
    unsafe_ctoon_set_len(obj, len + 1);
}

ctoon_api_inline ctoon_mut_val *unsafe_ctoon_mut_obj_remove(
    ctoon_mut_val *obj, const char *key, size_t key_len) {
    size_t obj_len = unsafe_ctoon_get_len(obj);
    if (obj_len) {
        ctoon_mut_val *pre_key = (ctoon_mut_val *)obj->uni.ptr;
        ctoon_mut_val *cur_key = pre_key->next->next;
        ctoon_mut_val *removed_item = NULL;
        size_t i;
        for (i = 0; i < obj_len; i++) {
            if (unsafe_ctoon_equals_strn(cur_key, key, key_len)) {
                if (!removed_item) removed_item = cur_key->next;
                cur_key = cur_key->next->next;
                pre_key->next->next = cur_key;
                if (i + 1 == obj_len) obj->uni.ptr = pre_key;
                i--;
                obj_len--;
            } else {
                pre_key = cur_key;
                cur_key = cur_key->next->next;
            }
        }
        unsafe_ctoon_set_len(obj, obj_len);
        return removed_item;
    } else {
        return NULL;
    }
}

ctoon_api_inline bool unsafe_ctoon_mut_obj_replace(ctoon_mut_val *obj,
                                                     ctoon_mut_val *key,
                                                     ctoon_mut_val *val) {
    size_t key_len = unsafe_ctoon_get_len(key);
    size_t obj_len = unsafe_ctoon_get_len(obj);
    if (obj_len) {
        ctoon_mut_val *pre_key = (ctoon_mut_val *)obj->uni.ptr;
        ctoon_mut_val *cur_key = pre_key->next->next;
        size_t i;
        for (i = 0; i < obj_len; i++) {
            if (unsafe_ctoon_equals_strn(cur_key, key->uni.str, key_len)) {
                cur_key->next->tag = val->tag;
                cur_key->next->uni.u64 = val->uni.u64;
                return true;
            } else {
                cur_key = cur_key->next->next;
            }
        }
    }
    return false;
}

ctoon_api_inline void unsafe_ctoon_mut_obj_rotate(ctoon_mut_val *obj,
                                                    size_t idx) {
    ctoon_mut_val *key = (ctoon_mut_val *)obj->uni.ptr;
    while (idx-- > 0) key = key->next->next;
    obj->uni.ptr = (void *)key;
}

ctoon_api_inline bool ctoon_mut_obj_add(ctoon_mut_val *obj,
                                          ctoon_mut_val *key,
                                          ctoon_mut_val *val) {
    if (ctoon_likely(ctoon_mut_is_obj(obj) &&
                      ctoon_mut_is_str(key) && val)) {
        unsafe_ctoon_mut_obj_add(obj, key, val, unsafe_ctoon_get_len(obj));
        return true;
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_obj_put(ctoon_mut_val *obj,
                                          ctoon_mut_val *key,
                                          ctoon_mut_val *val) {
    bool replaced = false;
    size_t key_len;
    ctoon_mut_obj_iter iter;
    ctoon_mut_val *cur_key;
    if (ctoon_unlikely(!ctoon_mut_is_obj(obj) ||
                        !ctoon_mut_is_str(key))) return false;
    key_len = unsafe_ctoon_get_len(key);
    ctoon_mut_obj_iter_init(obj, &iter);
    while ((cur_key = ctoon_mut_obj_iter_next(&iter)) != 0) {
        if (unsafe_ctoon_equals_strn(cur_key, key->uni.str, key_len)) {
            if (!replaced && val) {
                replaced = true;
                val->next = cur_key->next->next;
                cur_key->next = val;
            } else {
                ctoon_mut_obj_iter_remove(&iter);
            }
        }
    }
    if (!replaced && val) unsafe_ctoon_mut_obj_add(obj, key, val, iter.max);
    return true;
}

ctoon_api_inline bool ctoon_mut_obj_insert(ctoon_mut_val *obj,
                                             ctoon_mut_val *key,
                                             ctoon_mut_val *val,
                                             size_t idx) {
    if (ctoon_likely(ctoon_mut_is_obj(obj) &&
                      ctoon_mut_is_str(key) && val)) {
        size_t len = unsafe_ctoon_get_len(obj);
        if (ctoon_likely(len >= idx)) {
            if (len > idx) {
                void *ptr = obj->uni.ptr;
                unsafe_ctoon_mut_obj_rotate(obj, idx);
                unsafe_ctoon_mut_obj_add(obj, key, val, len);
                obj->uni.ptr = ptr;
            } else {
                unsafe_ctoon_mut_obj_add(obj, key, val, len);
            }
            return true;
        }
    }
    return false;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_remove(ctoon_mut_val *obj,
    ctoon_mut_val *key) {
    if (ctoon_likely(ctoon_mut_is_obj(obj) && ctoon_mut_is_str(key))) {
        return unsafe_ctoon_mut_obj_remove(obj, key->uni.str,
                                            unsafe_ctoon_get_len(key));
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_remove_key(
    ctoon_mut_val *obj, const char *key) {
    if (ctoon_likely(ctoon_mut_is_obj(obj) && key)) {
        size_t key_len = strlen(key);
        return unsafe_ctoon_mut_obj_remove(obj, key, key_len);
    }
    return NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_remove_keyn(
    ctoon_mut_val *obj, const char *key, size_t key_len) {
    if (ctoon_likely(ctoon_mut_is_obj(obj) && key)) {
        return unsafe_ctoon_mut_obj_remove(obj, key, key_len);
    }
    return NULL;
}

ctoon_api_inline bool ctoon_mut_obj_clear(ctoon_mut_val *obj) {
    if (ctoon_likely(ctoon_mut_is_obj(obj))) {
        unsafe_ctoon_set_len(obj, 0);
        return true;
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_obj_replace(ctoon_mut_val *obj,
                                              ctoon_mut_val *key,
                                              ctoon_mut_val *val) {
    if (ctoon_likely(ctoon_mut_is_obj(obj) &&
                      ctoon_mut_is_str(key) && val)) {
        return unsafe_ctoon_mut_obj_replace(obj, key, val);
    }
    return false;
}

ctoon_api_inline bool ctoon_mut_obj_rotate(ctoon_mut_val *obj,
                                             size_t idx) {
    if (ctoon_likely(ctoon_mut_is_obj(obj) &&
                      unsafe_ctoon_get_len(obj) > idx)) {
        unsafe_ctoon_mut_obj_rotate(obj, idx);
        return true;
    }
    return false;
}



/*==============================================================================
 * MARK: - Mutable TOON Object Modification Convenience API (Implementation)
 *============================================================================*/

#define ctoon_mut_obj_add_func(func) \
    if (ctoon_likely(doc && ctoon_mut_is_obj(obj) && _key)) { \
        ctoon_mut_val *key = unsafe_ctoon_mut_val(doc, 2); \
        if (ctoon_likely(key)) { \
            size_t len = unsafe_ctoon_get_len(obj); \
            ctoon_mut_val *val = key + 1; \
            size_t key_len = strlen(_key); \
            bool noesc = unsafe_ctoon_is_str_noesc(_key, key_len); \
            key->tag = CTOON_TYPE_STR; \
            key->tag |= noesc ? CTOON_SUBTYPE_NOESC : CTOON_SUBTYPE_NONE; \
            key->tag |= (uint64_t)strlen(_key) << CTOON_TAG_BIT; \
            key->uni.str = _key; \
            func \
            unsafe_ctoon_mut_obj_add(obj, key, val, len); \
            return true; \
        } \
    } \
    return false

ctoon_api_inline bool ctoon_mut_obj_add_null(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *_key) {
    ctoon_mut_obj_add_func({ unsafe_ctoon_set_null(val); });
}

ctoon_api_inline bool ctoon_mut_obj_add_true(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *_key) {
    ctoon_mut_obj_add_func({ unsafe_ctoon_set_bool(val, true); });
}

ctoon_api_inline bool ctoon_mut_obj_add_false(ctoon_mut_doc *doc,
                                                ctoon_mut_val *obj,
                                                const char *_key) {
    ctoon_mut_obj_add_func({ unsafe_ctoon_set_bool(val, false); });
}

ctoon_api_inline bool ctoon_mut_obj_add_bool(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *_key,
                                               bool _val) {
    ctoon_mut_obj_add_func({ unsafe_ctoon_set_bool(val, _val); });
}

ctoon_api_inline bool ctoon_mut_obj_add_uint(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *_key,
                                               uint64_t _val) {
    ctoon_mut_obj_add_func({ unsafe_ctoon_set_uint(val, _val); });
}

ctoon_api_inline bool ctoon_mut_obj_add_sint(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *_key,
                                               int64_t _val) {
    ctoon_mut_obj_add_func({ unsafe_ctoon_set_sint(val, _val); });
}

ctoon_api_inline bool ctoon_mut_obj_add_int(ctoon_mut_doc *doc,
                                              ctoon_mut_val *obj,
                                              const char *_key,
                                              int64_t _val) {
    ctoon_mut_obj_add_func({ unsafe_ctoon_set_sint(val, _val); });
}

ctoon_api_inline bool ctoon_mut_obj_add_float(ctoon_mut_doc *doc,
                                                ctoon_mut_val *obj,
                                                const char *_key,
                                                float _val) {
    ctoon_mut_obj_add_func({ unsafe_ctoon_set_float(val, _val); });
}

ctoon_api_inline bool ctoon_mut_obj_add_double(ctoon_mut_doc *doc,
                                                 ctoon_mut_val *obj,
                                                 const char *_key,
                                                 double _val) {
    ctoon_mut_obj_add_func({ unsafe_ctoon_set_double(val, _val); });
}

ctoon_api_inline bool ctoon_mut_obj_add_real(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *_key,
                                               double _val) {
    ctoon_mut_obj_add_func({ unsafe_ctoon_set_real(val, _val); });
}

ctoon_api_inline bool ctoon_mut_obj_add_str(ctoon_mut_doc *doc,
                                              ctoon_mut_val *obj,
                                              const char *_key,
                                              const char *_val) {
    if (ctoon_unlikely(!_val)) return false;
    ctoon_mut_obj_add_func({
        size_t val_len = strlen(_val);
        bool val_noesc = unsafe_ctoon_is_str_noesc(_val, val_len);
        val->tag = ((uint64_t)strlen(_val) << CTOON_TAG_BIT) | CTOON_TYPE_STR;
        val->tag |= val_noesc ? CTOON_SUBTYPE_NOESC : CTOON_SUBTYPE_NONE;
        val->uni.str = _val;
    });
}

ctoon_api_inline bool ctoon_mut_obj_add_strn(ctoon_mut_doc *doc,
                                               ctoon_mut_val *obj,
                                               const char *_key,
                                               const char *_val,
                                               size_t _len) {
    if (ctoon_unlikely(!_val)) return false;
    ctoon_mut_obj_add_func({
        val->tag = ((uint64_t)_len << CTOON_TAG_BIT) | CTOON_TYPE_STR;
        val->uni.str = _val;
    });
}

ctoon_api_inline bool ctoon_mut_obj_add_strcpy(ctoon_mut_doc *doc,
                                                 ctoon_mut_val *obj,
                                                 const char *_key,
                                                 const char *_val) {
    if (ctoon_unlikely(!_val)) return false;
    ctoon_mut_obj_add_func({
        size_t _len = strlen(_val);
        val->uni.str = unsafe_ctoon_mut_strncpy(doc, _val, _len);
        if (ctoon_unlikely(!val->uni.str)) return false;
        val->tag = ((uint64_t)_len << CTOON_TAG_BIT) | CTOON_TYPE_STR;
    });
}

ctoon_api_inline bool ctoon_mut_obj_add_strncpy(ctoon_mut_doc *doc,
                                                  ctoon_mut_val *obj,
                                                  const char *_key,
                                                  const char *_val,
                                                  size_t _len) {
    if (ctoon_unlikely(!_val)) return false;
    ctoon_mut_obj_add_func({
        val->uni.str = unsafe_ctoon_mut_strncpy(doc, _val, _len);
        if (ctoon_unlikely(!val->uni.str)) return false;
        val->tag = ((uint64_t)_len << CTOON_TAG_BIT) | CTOON_TYPE_STR;
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_add_arr(ctoon_mut_doc *doc,
                                                         ctoon_mut_val *obj,
                                                         const char *_key) {
    ctoon_mut_val *key = ctoon_mut_str(doc, _key);
    ctoon_mut_val *val = ctoon_mut_arr(doc);
    return ctoon_mut_obj_add(obj, key, val) ? val : NULL;
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_add_obj(ctoon_mut_doc *doc,
                                                         ctoon_mut_val *obj,
                                                         const char *_key) {
    ctoon_mut_val *key = ctoon_mut_str(doc, _key);
    ctoon_mut_val *val = ctoon_mut_obj(doc);
    return ctoon_mut_obj_add(obj, key, val) ? val : NULL;
}

ctoon_api_inline bool ctoon_mut_obj_add_val(ctoon_mut_doc *doc,
                                              ctoon_mut_val *obj,
                                              const char *_key,
                                              ctoon_mut_val *_val) {
    if (ctoon_unlikely(!_val)) return false;
    ctoon_mut_obj_add_func({
        val = _val;
    });
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_remove_str(ctoon_mut_val *obj,
                                                            const char *key) {
    return ctoon_mut_obj_remove_strn(obj, key, key ? strlen(key) : 0);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_obj_remove_strn(
    ctoon_mut_val *obj, const char *_key, size_t _len) {
    if (ctoon_likely(ctoon_mut_is_obj(obj) && _key)) {
        ctoon_mut_val *key;
        ctoon_mut_obj_iter iter;
        ctoon_mut_val *val_removed = NULL;
        ctoon_mut_obj_iter_init(obj, &iter);
        while ((key = ctoon_mut_obj_iter_next(&iter)) != NULL) {
            if (unsafe_ctoon_equals_strn(key, _key, _len)) {
                if (!val_removed) val_removed = key->next;
                ctoon_mut_obj_iter_remove(&iter);
            }
        }
        return val_removed;
    }
    return NULL;
}

ctoon_api_inline bool ctoon_mut_obj_rename_key(ctoon_mut_doc *doc,
                                                 ctoon_mut_val *obj,
                                                 const char *key,
                                                 const char *new_key) {
    if (!key || !new_key) return false;
    return ctoon_mut_obj_rename_keyn(doc, obj, key, strlen(key),
                                      new_key, strlen(new_key));
}

ctoon_api_inline bool ctoon_mut_obj_rename_keyn(ctoon_mut_doc *doc,
                                                  ctoon_mut_val *obj,
                                                  const char *key,
                                                  size_t len,
                                                  const char *new_key,
                                                  size_t new_len) {
    char *cpy_key = NULL;
    ctoon_mut_val *old_key;
    ctoon_mut_obj_iter iter;
    if (!doc || !obj || !key || !new_key) return false;
    ctoon_mut_obj_iter_init(obj, &iter);
    while ((old_key = ctoon_mut_obj_iter_next(&iter))) {
        if (unsafe_ctoon_equals_strn((void *)old_key, key, len)) {
            if (!cpy_key) {
                cpy_key = unsafe_ctoon_mut_strncpy(doc, new_key, new_len);
                if (!cpy_key) return false;
            }
            ctoon_mut_set_strn(old_key, cpy_key, new_len);
        }
    }
    return cpy_key != NULL;
}



#if !defined(CTOON_DISABLE_UTILS) || !CTOON_DISABLE_UTILS

/*==============================================================================
 * MARK: - TOON Pointer API (Implementation)
 *============================================================================*/

#define ctoon_ptr_set_err(_code, _msg) do { \
    if (err) { \
        err->code = CTOON_PTR_ERR_##_code; \
        err->msg = _msg; \
        err->pos = 0; \
    } \
} while(false)

/* require: val != NULL, *ptr == '/', len > 0 */
ctoon_api ctoon_val *unsafe_ctoon_ptr_getx(ctoon_val *val,
                                              const char *ptr, size_t len,
                                              ctoon_ptr_err *err);

/* require: val != NULL, *ptr == '/', len > 0 */
ctoon_api ctoon_mut_val *unsafe_ctoon_mut_ptr_getx(ctoon_mut_val *val,
                                                      const char *ptr,
                                                      size_t len,
                                                      ctoon_ptr_ctx *ctx,
                                                      ctoon_ptr_err *err);

/* require: val/new_val/doc != NULL, *ptr == '/', len > 0 */
ctoon_api bool unsafe_ctoon_mut_ptr_putx(ctoon_mut_val *val,
                                           const char *ptr, size_t len,
                                           ctoon_mut_val *new_val,
                                           ctoon_mut_doc *doc,
                                           bool create_parent, bool insert_new,
                                           ctoon_ptr_ctx *ctx,
                                           ctoon_ptr_err *err);

/* require: val/err != NULL, *ptr == '/', len > 0 */
ctoon_api ctoon_mut_val *unsafe_ctoon_mut_ptr_replacex(
    ctoon_mut_val *val, const char *ptr, size_t len, ctoon_mut_val *new_val,
    ctoon_ptr_ctx *ctx, ctoon_ptr_err *err);

/* require: val/err != NULL, *ptr == '/', len > 0 */
ctoon_api ctoon_mut_val *unsafe_ctoon_mut_ptr_removex(ctoon_mut_val *val,
                                                         const char *ptr,
                                                         size_t len,
                                                         ctoon_ptr_ctx *ctx,
                                                         ctoon_ptr_err *err);

ctoon_api_inline ctoon_val *ctoon_doc_ptr_get(ctoon_doc *doc,
                                                 const char *ptr) {
    if (ctoon_unlikely(!ptr)) return NULL;
    return ctoon_doc_ptr_getn(doc, ptr, strlen(ptr));
}

ctoon_api_inline ctoon_val *ctoon_doc_ptr_getn(ctoon_doc *doc,
                                                  const char *ptr, size_t len) {
    return ctoon_doc_ptr_getx(doc, ptr, len, NULL);
}

ctoon_api_inline ctoon_val *ctoon_doc_ptr_getx(ctoon_doc *doc,
                                                  const char *ptr, size_t len,
                                                  ctoon_ptr_err *err) {
    ctoon_ptr_set_err(NONE, NULL);
    if (ctoon_unlikely(!doc || !ptr)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (ctoon_unlikely(!doc->root)) {
        ctoon_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return NULL;
    }
    if (ctoon_unlikely(len == 0)) {
        return doc->root;
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_ctoon_ptr_getx(doc->root, ptr, len, err);
}

ctoon_api_inline ctoon_val *ctoon_ptr_get(ctoon_val *val,
                                             const char *ptr) {
    if (ctoon_unlikely(!ptr)) return NULL;
    return ctoon_ptr_getn(val, ptr, strlen(ptr));
}

ctoon_api_inline ctoon_val *ctoon_ptr_getn(ctoon_val *val,
                                              const char *ptr, size_t len) {
    return ctoon_ptr_getx(val, ptr, len, NULL);
}

ctoon_api_inline ctoon_val *ctoon_ptr_getx(ctoon_val *val,
                                              const char *ptr, size_t len,
                                              ctoon_ptr_err *err) {
    ctoon_ptr_set_err(NONE, NULL);
    if (ctoon_unlikely(!val || !ptr)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (ctoon_unlikely(len == 0)) {
        return val;
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_ctoon_ptr_getx(val, ptr, len, err);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_get(ctoon_mut_doc *doc,
                                                         const char *ptr) {
    if (!ptr) return NULL;
    return ctoon_mut_doc_ptr_getn(doc, ptr, strlen(ptr));
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_getn(ctoon_mut_doc *doc,
                                                          const char *ptr,
                                                          size_t len) {
    return ctoon_mut_doc_ptr_getx(doc, ptr, len, NULL, NULL);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_getx(ctoon_mut_doc *doc,
                                                          const char *ptr,
                                                          size_t len,
                                                          ctoon_ptr_ctx *ctx,
                                                          ctoon_ptr_err *err) {
    ctoon_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));

    if (ctoon_unlikely(!doc || !ptr)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (ctoon_unlikely(!doc->root)) {
        ctoon_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return NULL;
    }
    if (ctoon_unlikely(len == 0)) {
        return doc->root;
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_ctoon_mut_ptr_getx(doc->root, ptr, len, ctx, err);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_get(ctoon_mut_val *val,
                                                     const char *ptr) {
    if (!ptr) return NULL;
    return ctoon_mut_ptr_getn(val, ptr, strlen(ptr));
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_getn(ctoon_mut_val *val,
                                                      const char *ptr,
                                                      size_t len) {
    return ctoon_mut_ptr_getx(val, ptr, len, NULL, NULL);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_getx(ctoon_mut_val *val,
                                                      const char *ptr,
                                                      size_t len,
                                                      ctoon_ptr_ctx *ctx,
                                                      ctoon_ptr_err *err) {
    ctoon_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));

    if (ctoon_unlikely(!val || !ptr)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (ctoon_unlikely(len == 0)) {
        return val;
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_ctoon_mut_ptr_getx(val, ptr, len, ctx, err);
}

ctoon_api_inline bool ctoon_mut_doc_ptr_add(ctoon_mut_doc *doc,
                                              const char *ptr,
                                              ctoon_mut_val *new_val) {
    if (ctoon_unlikely(!ptr)) return false;
    return ctoon_mut_doc_ptr_addn(doc, ptr, strlen(ptr), new_val);
}

ctoon_api_inline bool ctoon_mut_doc_ptr_addn(ctoon_mut_doc *doc,
                                               const char *ptr,
                                               size_t len,
                                               ctoon_mut_val *new_val) {
    return ctoon_mut_doc_ptr_addx(doc, ptr, len, new_val, true, NULL, NULL);
}

ctoon_api_inline bool ctoon_mut_doc_ptr_addx(ctoon_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               ctoon_mut_val *new_val,
                                               bool create_parent,
                                               ctoon_ptr_ctx *ctx,
                                               ctoon_ptr_err *err) {
    ctoon_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));

    if (ctoon_unlikely(!doc || !ptr || !new_val)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return false;
    }
    if (ctoon_unlikely(len == 0)) {
        if (doc->root) {
            ctoon_ptr_set_err(SET_ROOT, "cannot set document's root");
            return false;
        } else {
            doc->root = new_val;
            return true;
        }
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return false;
    }
    if (ctoon_unlikely(!doc->root && !create_parent)) {
        ctoon_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return false;
    }
    if (ctoon_unlikely(!doc->root)) {
        ctoon_mut_val *root = ctoon_mut_obj(doc);
        if (ctoon_unlikely(!root)) {
            ctoon_ptr_set_err(MEMORY_ALLOCATION, "failed to create value");
            return false;
        }
        if (unsafe_ctoon_mut_ptr_putx(root, ptr, len, new_val, doc,
                                       create_parent, true, ctx, err)) {
            doc->root = root;
            return true;
        }
        return false;
    }
    return unsafe_ctoon_mut_ptr_putx(doc->root, ptr, len, new_val, doc,
                                      create_parent, true, ctx, err);
}

ctoon_api_inline bool ctoon_mut_ptr_add(ctoon_mut_val *val,
                                          const char *ptr,
                                          ctoon_mut_val *new_val,
                                          ctoon_mut_doc *doc) {
    if (ctoon_unlikely(!ptr)) return false;
    return ctoon_mut_ptr_addn(val, ptr, strlen(ptr), new_val, doc);
}

ctoon_api_inline bool ctoon_mut_ptr_addn(ctoon_mut_val *val,
                                           const char *ptr, size_t len,
                                           ctoon_mut_val *new_val,
                                           ctoon_mut_doc *doc) {
    return ctoon_mut_ptr_addx(val, ptr, len, new_val, doc, true, NULL, NULL);
}

ctoon_api_inline bool ctoon_mut_ptr_addx(ctoon_mut_val *val,
                                           const char *ptr, size_t len,
                                           ctoon_mut_val *new_val,
                                           ctoon_mut_doc *doc,
                                           bool create_parent,
                                           ctoon_ptr_ctx *ctx,
                                           ctoon_ptr_err *err) {
    ctoon_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));

    if (ctoon_unlikely(!val || !ptr || !new_val || !doc)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return false;
    }
    if (ctoon_unlikely(len == 0)) {
        ctoon_ptr_set_err(SET_ROOT, "cannot set root");
        return false;
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return false;
    }
    return unsafe_ctoon_mut_ptr_putx(val, ptr, len, new_val,
                                       doc, create_parent, true, ctx, err);
}

ctoon_api_inline bool ctoon_mut_doc_ptr_set(ctoon_mut_doc *doc,
                                              const char *ptr,
                                              ctoon_mut_val *new_val) {
    if (ctoon_unlikely(!ptr)) return false;
    return ctoon_mut_doc_ptr_setn(doc, ptr, strlen(ptr), new_val);
}

ctoon_api_inline bool ctoon_mut_doc_ptr_setn(ctoon_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               ctoon_mut_val *new_val) {
    return ctoon_mut_doc_ptr_setx(doc, ptr, len, new_val, true, NULL, NULL);
}

ctoon_api_inline bool ctoon_mut_doc_ptr_setx(ctoon_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               ctoon_mut_val *new_val,
                                               bool create_parent,
                                               ctoon_ptr_ctx *ctx,
                                               ctoon_ptr_err *err) {
    ctoon_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));

    if (ctoon_unlikely(!doc || !ptr)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return false;
    }
    if (ctoon_unlikely(len == 0)) {
        if (ctx) ctx->old = doc->root;
        doc->root = new_val;
        return true;
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return false;
    }
    if (!new_val) {
        if (!doc->root) {
            ctoon_ptr_set_err(RESOLVE, "TOON pointer cannot be resolved");
            return false;
        }
        return !!unsafe_ctoon_mut_ptr_removex(doc->root, ptr, len, ctx, err);
    }
    if (ctoon_unlikely(!doc->root && !create_parent)) {
        ctoon_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return false;
    }
    if (ctoon_unlikely(!doc->root)) {
        ctoon_mut_val *root = ctoon_mut_obj(doc);
        if (ctoon_unlikely(!root)) {
            ctoon_ptr_set_err(MEMORY_ALLOCATION, "failed to create value");
            return false;
        }
        if (unsafe_ctoon_mut_ptr_putx(root, ptr, len, new_val, doc,
                                       create_parent, false, ctx, err)) {
            doc->root = root;
            return true;
        }
        return false;
    }
    return unsafe_ctoon_mut_ptr_putx(doc->root, ptr, len, new_val, doc,
                                      create_parent, false, ctx, err);
}

ctoon_api_inline bool ctoon_mut_ptr_set(ctoon_mut_val *val,
                                          const char *ptr,
                                          ctoon_mut_val *new_val,
                                          ctoon_mut_doc *doc) {
    if (ctoon_unlikely(!ptr)) return false;
    return ctoon_mut_ptr_setn(val, ptr, strlen(ptr), new_val, doc);
}

ctoon_api_inline bool ctoon_mut_ptr_setn(ctoon_mut_val *val,
                                           const char *ptr, size_t len,
                                           ctoon_mut_val *new_val,
                                           ctoon_mut_doc *doc) {
    return ctoon_mut_ptr_setx(val, ptr, len, new_val, doc, true, NULL, NULL);
}

ctoon_api_inline bool ctoon_mut_ptr_setx(ctoon_mut_val *val,
                                           const char *ptr, size_t len,
                                           ctoon_mut_val *new_val,
                                           ctoon_mut_doc *doc,
                                           bool create_parent,
                                           ctoon_ptr_ctx *ctx,
                                           ctoon_ptr_err *err) {
    ctoon_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));

    if (ctoon_unlikely(!val || !ptr || !doc)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return false;
    }
    if (ctoon_unlikely(len == 0)) {
        ctoon_ptr_set_err(SET_ROOT, "cannot set root");
        return false;
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return false;
    }
    if (!new_val) {
        return !!unsafe_ctoon_mut_ptr_removex(val, ptr, len, ctx, err);
    }
    return unsafe_ctoon_mut_ptr_putx(val, ptr, len, new_val, doc,
                                      create_parent, false, ctx, err);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_replace(
    ctoon_mut_doc *doc, const char *ptr, ctoon_mut_val *new_val) {
    if (!ptr) return NULL;
    return ctoon_mut_doc_ptr_replacen(doc, ptr, strlen(ptr), new_val);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_replacen(
    ctoon_mut_doc *doc, const char *ptr, size_t len, ctoon_mut_val *new_val) {
    return ctoon_mut_doc_ptr_replacex(doc, ptr, len, new_val, NULL, NULL);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_replacex(
    ctoon_mut_doc *doc, const char *ptr, size_t len, ctoon_mut_val *new_val,
    ctoon_ptr_ctx *ctx, ctoon_ptr_err *err) {

    ctoon_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));

    if (ctoon_unlikely(!doc || !ptr || !new_val)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (ctoon_unlikely(len == 0)) {
        ctoon_mut_val *root = doc->root;
        if (ctoon_unlikely(!root)) {
            ctoon_ptr_set_err(RESOLVE, "TOON pointer cannot be resolved");
            return NULL;
        }
        if (ctx) ctx->old = root;
        doc->root = new_val;
        return root;
    }
    if (ctoon_unlikely(!doc->root)) {
        ctoon_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return NULL;
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_ctoon_mut_ptr_replacex(doc->root, ptr, len, new_val,
                                          ctx, err);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_replace(
    ctoon_mut_val *val, const char *ptr, ctoon_mut_val *new_val) {
    if (!ptr) return NULL;
    return ctoon_mut_ptr_replacen(val, ptr, strlen(ptr), new_val);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_replacen(
    ctoon_mut_val *val, const char *ptr, size_t len, ctoon_mut_val *new_val) {
    return ctoon_mut_ptr_replacex(val, ptr, len, new_val, NULL, NULL);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_replacex(
    ctoon_mut_val *val, const char *ptr, size_t len, ctoon_mut_val *new_val,
    ctoon_ptr_ctx *ctx, ctoon_ptr_err *err) {

    ctoon_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));

    if (ctoon_unlikely(!val || !ptr || !new_val)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (ctoon_unlikely(len == 0)) {
        ctoon_ptr_set_err(SET_ROOT, "cannot set root");
        return NULL;
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_ctoon_mut_ptr_replacex(val, ptr, len, new_val, ctx, err);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_remove(
    ctoon_mut_doc *doc, const char *ptr) {
    if (!ptr) return NULL;
    return ctoon_mut_doc_ptr_removen(doc, ptr, strlen(ptr));
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_removen(
    ctoon_mut_doc *doc, const char *ptr, size_t len) {
    return ctoon_mut_doc_ptr_removex(doc, ptr, len, NULL, NULL);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_ptr_removex(
    ctoon_mut_doc *doc, const char *ptr, size_t len,
    ctoon_ptr_ctx *ctx, ctoon_ptr_err *err) {

    ctoon_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));

    if (ctoon_unlikely(!doc || !ptr)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (ctoon_unlikely(!doc->root)) {
        ctoon_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return NULL;
    }
    if (ctoon_unlikely(len == 0)) {
        ctoon_mut_val *root = doc->root;
        if (ctx) ctx->old = root;
        doc->root = NULL;
        return root;
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_ctoon_mut_ptr_removex(doc->root, ptr, len, ctx, err);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_remove(ctoon_mut_val *val,
                                                        const char *ptr) {
    if (!ptr) return NULL;
    return ctoon_mut_ptr_removen(val, ptr, strlen(ptr));
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_removen(ctoon_mut_val *val,
                                                         const char *ptr,
                                                         size_t len) {
    return ctoon_mut_ptr_removex(val, ptr, len, NULL, NULL);
}

ctoon_api_inline ctoon_mut_val *ctoon_mut_ptr_removex(ctoon_mut_val *val,
                                                         const char *ptr,
                                                         size_t len,
                                                         ctoon_ptr_ctx *ctx,
                                                         ctoon_ptr_err *err) {
    ctoon_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));

    if (ctoon_unlikely(!val || !ptr)) {
        ctoon_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (ctoon_unlikely(len == 0)) {
        ctoon_ptr_set_err(SET_ROOT, "cannot set root");
        return NULL;
    }
    if (ctoon_unlikely(*ptr != '/')) {
        ctoon_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_ctoon_mut_ptr_removex(val, ptr, len, ctx, err);
}

ctoon_api_inline bool ctoon_ptr_ctx_append(ctoon_ptr_ctx *ctx,
                                             ctoon_mut_val *key,
                                             ctoon_mut_val *val) {
    ctoon_mut_val *ctn, *pre_key, *pre_val, *cur_key, *cur_val;
    if (!ctx || !ctx->ctn || !val) return false;
    ctn = ctx->ctn;

    if (ctoon_mut_is_obj(ctn)) {
        if (!key) return false;
        key->next = val;
        pre_key = ctx->pre;
        if (unsafe_ctoon_get_len(ctn) == 0) {
            val->next = key;
            ctn->uni.ptr = key;
            ctx->pre = key;
        } else if (!pre_key) {
            pre_key = (ctoon_mut_val *)ctn->uni.ptr;
            pre_val = pre_key->next;
            val->next = pre_val->next;
            pre_val->next = key;
            ctn->uni.ptr = key;
            ctx->pre = pre_key;
        } else {
            cur_key = pre_key->next->next;
            cur_val = cur_key->next;
            val->next = cur_val->next;
            cur_val->next = key;
            if (ctn->uni.ptr == cur_key) ctn->uni.ptr = key;
            ctx->pre = cur_key;
        }
    } else {
        pre_val = ctx->pre;
        if (unsafe_ctoon_get_len(ctn) == 0) {
            val->next = val;
            ctn->uni.ptr = val;
            ctx->pre = val;
        } else if (!pre_val) {
            pre_val = (ctoon_mut_val *)ctn->uni.ptr;
            val->next = pre_val->next;
            pre_val->next = val;
            ctn->uni.ptr = val;
            ctx->pre = pre_val;
        } else {
            cur_val = pre_val->next;
            val->next = cur_val->next;
            cur_val->next = val;
            if (ctn->uni.ptr == cur_val) ctn->uni.ptr = val;
            ctx->pre = cur_val;
        }
    }
    unsafe_ctoon_inc_len(ctn);
    return true;
}

ctoon_api_inline bool ctoon_ptr_ctx_replace(ctoon_ptr_ctx *ctx,
                                              ctoon_mut_val *val) {
    ctoon_mut_val *ctn, *pre_key, *cur_key, *pre_val, *cur_val;
    if (!ctx || !ctx->ctn || !ctx->pre || !val) return false;
    ctn = ctx->ctn;
    if (ctoon_mut_is_obj(ctn)) {
        pre_key = ctx->pre;
        pre_val = pre_key->next;
        cur_key = pre_val->next;
        cur_val = cur_key->next;
        /* replace current value */
        cur_key->next = val;
        val->next = cur_val->next;
        ctx->old = cur_val;
    } else {
        pre_val = ctx->pre;
        cur_val = pre_val->next;
        /* replace current value */
        if (pre_val != cur_val) {
            val->next = cur_val->next;
            pre_val->next = val;
            if (ctn->uni.ptr == cur_val) ctn->uni.ptr = val;
        } else {
            val->next = val;
            ctn->uni.ptr = val;
            ctx->pre = val;
        }
        ctx->old = cur_val;
    }
    return true;
}

ctoon_api_inline bool ctoon_ptr_ctx_remove(ctoon_ptr_ctx *ctx) {
    ctoon_mut_val *ctn, *pre_key, *pre_val, *cur_key, *cur_val;
    size_t len;
    if (!ctx || !ctx->ctn || !ctx->pre) return false;
    ctn = ctx->ctn;
    if (ctoon_mut_is_obj(ctn)) {
        pre_key = ctx->pre;
        pre_val = pre_key->next;
        cur_key = pre_val->next;
        cur_val = cur_key->next;
        /* remove current key-value */
        pre_val->next = cur_val->next;
        if (ctn->uni.ptr == cur_key) ctn->uni.ptr = pre_key;
        ctx->pre = NULL;
        ctx->old = cur_val;
    } else {
        pre_val = ctx->pre;
        cur_val = pre_val->next;
        /* remove current key-value */
        pre_val->next = cur_val->next;
        if (ctn->uni.ptr == cur_val) ctn->uni.ptr = pre_val;
        ctx->pre = NULL;
        ctx->old = cur_val;
    }
    len = unsafe_ctoon_get_len(ctn) - 1;
    if (len == 0) ctn->uni.ptr = NULL;
    unsafe_ctoon_set_len(ctn, len);
    return true;
}

#undef ctoon_ptr_set_err



/*==============================================================================
 * MARK: - TOON Value at Pointer API (Implementation)
 *============================================================================*/

/**
 Set provided `value` if the TOON Pointer (RFC 6901) exists and is type bool.
 Returns true if value at `ptr` exists and is the correct type, otherwise false.
 */
ctoon_api_inline bool ctoon_ptr_get_bool(
    ctoon_val *root, const char *ptr, bool *value) {
    ctoon_val *val = ctoon_ptr_get(root, ptr);
    if (value && ctoon_is_bool(val)) {
        *value = unsafe_ctoon_get_bool(val);
        return true;
    } else {
        return false;
    }
}

/**
 Set provided `value` if the TOON Pointer (RFC 6901) exists and is an integer
 that fits in `uint64_t`. Returns true if successful, otherwise false.
 */
ctoon_api_inline bool ctoon_ptr_get_uint(
    ctoon_val *root, const char *ptr, uint64_t *value) {
    ctoon_val *val = ctoon_ptr_get(root, ptr);
    if (value && val) {
        uint64_t ret = val->uni.u64;
        if (unsafe_ctoon_is_uint(val) ||
            (unsafe_ctoon_is_sint(val) && !(ret >> 63))) {
            *value = ret;
            return true;
        }
    }
    return false;
}

/**
 Set provided `value` if the TOON Pointer (RFC 6901) exists and is an integer
 that fits in `int64_t`. Returns true if successful, otherwise false.
 */
ctoon_api_inline bool ctoon_ptr_get_sint(
    ctoon_val *root, const char *ptr, int64_t *value) {
    ctoon_val *val = ctoon_ptr_get(root, ptr);
    if (value && val) {
        int64_t ret = val->uni.i64;
        if (unsafe_ctoon_is_sint(val) ||
            (unsafe_ctoon_is_uint(val) && ret >= 0)) {
            *value = ret;
            return true;
        }
    }
    return false;
}

/**
 Set provided `value` if the TOON Pointer (RFC 6901) exists and is type real.
 Returns true if value at `ptr` exists and is the correct type, otherwise false.
 */
ctoon_api_inline bool ctoon_ptr_get_real(
    ctoon_val *root, const char *ptr, double *value) {
    ctoon_val *val = ctoon_ptr_get(root, ptr);
    if (value && ctoon_is_real(val)) {
        *value = unsafe_ctoon_get_real(val);
        return true;
    } else {
        return false;
    }
}

/**
 Set provided `value` if the TOON Pointer (RFC 6901) exists and is type sint,
 uint or real.
 Returns true if value at `ptr` exists and is the correct type, otherwise false.
 */
ctoon_api_inline bool ctoon_ptr_get_num(
    ctoon_val *root, const char *ptr, double *value) {
    ctoon_val *val = ctoon_ptr_get(root, ptr);
    if (value && ctoon_is_num(val)) {
        *value = unsafe_ctoon_get_num(val);
        return true;
    } else {
        return false;
    }
}

/**
 Set provided `value` if the TOON Pointer (RFC 6901) exists and is type string.
 Returns true if value at `ptr` exists and is the correct type, otherwise false.
 */
ctoon_api_inline bool ctoon_ptr_get_str(
    ctoon_val *root, const char *ptr, const char **value) {
    ctoon_val *val = ctoon_ptr_get(root, ptr);
    if (value && ctoon_is_str(val)) {
        *value = unsafe_ctoon_get_str(val);
        return true;
    } else {
        return false;
    }
}



/*==============================================================================
 * MARK: - Deprecated
 *============================================================================*/

/** @deprecated renamed to `ctoon_doc_ptr_get` */
ctoon_deprecated("renamed to ctoon_doc_ptr_get")
ctoon_api_inline ctoon_val *ctoon_doc_get_pointer(ctoon_doc *doc,
                                                     const char *ptr) {
    return ctoon_doc_ptr_get(doc, ptr);
}

/** @deprecated renamed to `ctoon_doc_ptr_getn` */
ctoon_deprecated("renamed to ctoon_doc_ptr_getn")
ctoon_api_inline ctoon_val *ctoon_doc_get_pointern(ctoon_doc *doc,
                                                      const char *ptr,
                                                      size_t len) {
    return ctoon_doc_ptr_getn(doc, ptr, len);
}

/** @deprecated renamed to `ctoon_mut_doc_ptr_get` */
ctoon_deprecated("renamed to ctoon_mut_doc_ptr_get")
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_get_pointer(
    ctoon_mut_doc *doc, const char *ptr) {
    return ctoon_mut_doc_ptr_get(doc, ptr);
}

/** @deprecated renamed to `ctoon_mut_doc_ptr_getn` */
ctoon_deprecated("renamed to ctoon_mut_doc_ptr_getn")
ctoon_api_inline ctoon_mut_val *ctoon_mut_doc_get_pointern(
    ctoon_mut_doc *doc, const char *ptr, size_t len) {
    return ctoon_mut_doc_ptr_getn(doc, ptr, len);
}

/** @deprecated renamed to `ctoon_ptr_get` */
ctoon_deprecated("renamed to ctoon_ptr_get")
ctoon_api_inline ctoon_val *ctoon_get_pointer(ctoon_val *val,
                                                 const char *ptr) {
    return ctoon_ptr_get(val, ptr);
}

/** @deprecated renamed to `ctoon_ptr_getn` */
ctoon_deprecated("renamed to ctoon_ptr_getn")
ctoon_api_inline ctoon_val *ctoon_get_pointern(ctoon_val *val,
                                                  const char *ptr,
                                                  size_t len) {
    return ctoon_ptr_getn(val, ptr, len);
}

/** @deprecated renamed to `ctoon_mut_ptr_get` */
ctoon_deprecated("renamed to ctoon_mut_ptr_get")
ctoon_api_inline ctoon_mut_val *ctoon_mut_get_pointer(ctoon_mut_val *val,
                                                         const char *ptr) {
    return ctoon_mut_ptr_get(val, ptr);
}

/** @deprecated renamed to `ctoon_mut_ptr_getn` */
ctoon_deprecated("renamed to ctoon_mut_ptr_getn")
ctoon_api_inline ctoon_mut_val *ctoon_mut_get_pointern(ctoon_mut_val *val,
                                                          const char *ptr,
                                                          size_t len) {
    return ctoon_mut_ptr_getn(val, ptr, len);
}

/** @deprecated renamed to `ctoon_mut_ptr_getn` */
ctoon_deprecated("renamed to unsafe_ctoon_ptr_getn")
ctoon_api_inline ctoon_val *unsafe_ctoon_get_pointer(ctoon_val *val,
                                                        const char *ptr,
                                                        size_t len) {
    ctoon_ptr_err err;
    return unsafe_ctoon_ptr_getx(val, ptr, len, &err);
}

/** @deprecated renamed to `unsafe_ctoon_mut_ptr_getx` */
ctoon_deprecated("renamed to unsafe_ctoon_mut_ptr_getx")
ctoon_api_inline ctoon_mut_val *unsafe_ctoon_mut_get_pointer(
    ctoon_mut_val *val, const char *ptr, size_t len) {
    ctoon_ptr_err err;
    return unsafe_ctoon_mut_ptr_getx(val, ptr, len, NULL, &err);
}

#endif /* CTOON_DISABLE_UTILS */



/*==============================================================================
 * MARK: - Compiler Hint End
 *============================================================================*/

#ifdef __cplusplus
}
#endif /* extern "C" end */

#endif /* CTOON_H */
