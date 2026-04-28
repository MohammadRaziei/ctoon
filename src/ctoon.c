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

#include "ctoon.h"
#include <errno.h>
#include <math.h> /* for `HUGE_VAL/INFINIY/NAN` macros, no libm required */





/*==============================================================================
 * MARK: - Version (Public)
 *============================================================================*/

uint32_t ctoon_version(void) {
    return CTOON_VERSION_HEX;
}



/*==============================================================================
 * MARK: - Flags (Private)
 *============================================================================*/

/* msvc intrinsic */
#if CTOON_MSC_VER >= 1400
#   include <intrin.h>
#   if defined(_M_AMD64) || defined(_M_ARM64)
#       define MSC_HAS_BIT_SCAN_64 1
#       pragma intrinsic(_BitScanForward64)
#       pragma intrinsic(_BitScanReverse64)
#   else
#       define MSC_HAS_BIT_SCAN_64 0
#   endif
#   if defined(_M_AMD64) || defined(_M_ARM64) || \
        defined(_M_IX86) || defined(_M_ARM)
#       define MSC_HAS_BIT_SCAN 1
#       pragma intrinsic(_BitScanForward)
#       pragma intrinsic(_BitScanReverse)
#   else
#       define MSC_HAS_BIT_SCAN 0
#   endif
#   if defined(_M_AMD64)
#       define MSC_HAS_UMUL128 1
#       pragma intrinsic(_umul128)
#   else
#       define MSC_HAS_UMUL128 0
#   endif
#else
#   define MSC_HAS_BIT_SCAN_64 0
#   define MSC_HAS_BIT_SCAN 0
#   define MSC_HAS_UMUL128 0
#endif

/* gcc builtin */
#if ctoon_has_builtin(__builtin_clzll) || ctoon_gcc_available(3, 4, 0)
#   define GCC_HAS_CLZLL 1
#else
#   define GCC_HAS_CLZLL 0
#endif

#if ctoon_has_builtin(__builtin_ctzll) || ctoon_gcc_available(3, 4, 0)
#   define GCC_HAS_CTZLL 1
#else
#   define GCC_HAS_CTZLL 0
#endif

/* int128 type */
#if defined(__SIZEOF_INT128__) && (__SIZEOF_INT128__ == 16) && \
    (defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER))
#    define CTOON_HAS_INT128 1
#else
#    define CTOON_HAS_INT128 0
#endif

/* IEEE 754 floating-point binary representation */
#if defined(__STDC_IEC_559__) || defined(__STDC_IEC_60559_BFP__)
#   define CTOON_HAS_IEEE_754 1
#elif FLT_RADIX == 2 && \
    FLT_MANT_DIG == 24 && FLT_DIG == 6 && \
    FLT_MIN_EXP == -125 && FLT_MAX_EXP == 128 && \
    FLT_MIN_10_EXP == -37 && FLT_MAX_10_EXP == 38 && \
    DBL_MANT_DIG == 53 && DBL_DIG == 15 && \
    DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024 && \
    DBL_MIN_10_EXP == -307 && DBL_MAX_10_EXP == 308
#   define CTOON_HAS_IEEE_754 1
#else
#   define CTOON_HAS_IEEE_754 0
#   undef  CTOON_DISABLE_FAST_FP_CONV
#   define CTOON_DISABLE_FAST_FP_CONV 1
#endif

/*
 Correct rounding in double number computations.

 On the x86 architecture, some compilers may use x87 FPU instructions for
 floating-point arithmetic. The x87 FPU loads all floating point number as
 80-bit double-extended precision internally, then rounds the result to original
 precision, which may produce inaccurate results. For a more detailed
 explanation, see the paper: https://arxiv.org/abs/cs/0701192

 Here are some examples of double precision calculation error:

     2877.0 / 1e6   == 0.002877,  but x87 returns 0.0028770000000000002
     43683.0 * 1e21 == 4.3683e25, but x87 returns 4.3683000000000004e25

 Here are some examples of compiler flags to generate x87 instructions on x86:

     clang -m32 -mno-sse
     gcc/icc -m32 -mfpmath=387
     msvc /arch:SSE or /arch:IA32

 If we are sure that there's no similar error described above, we can define the
 CTOON_DOUBLE_MATH_CORRECT as 1 to enable the fast path calculation. This is
 not an accurate detection, it's just try to avoid the error at compile-time.
 An accurate detection can be done at run-time:

     bool is_double_math_correct(void) {
         volatile double r = 43683.0;
         r *= 1e21;
         return r == 4.3683e25;
     }

 See also: utils.h in https://github.com/google/double-conversion/
 */
#if !defined(FLT_EVAL_METHOD) && defined(__FLT_EVAL_METHOD__)
#    define FLT_EVAL_METHOD __FLT_EVAL_METHOD__
#endif

#if defined(FLT_EVAL_METHOD) && FLT_EVAL_METHOD != 0 && FLT_EVAL_METHOD != 1
#    define CTOON_DOUBLE_MATH_CORRECT 0
#elif defined(i386) || defined(__i386) || defined(__i386__) || \
    defined(_X86_) || defined(__X86__) || defined(_M_IX86) || \
    defined(__I86__) || defined(__IA32__) || defined(__THW_INTEL)
#   if (defined(_MSC_VER) && defined(_M_IX86_FP) && _M_IX86_FP == 2) || \
        (defined(__SSE2_MATH__) && __SSE2_MATH__)
#       define CTOON_DOUBLE_MATH_CORRECT 1
#   else
#       define CTOON_DOUBLE_MATH_CORRECT 0
#   endif
#elif defined(__mc68000__) || defined(__pnacl__) || defined(__native_client__)
#   define CTOON_DOUBLE_MATH_CORRECT 0
#else
#   define CTOON_DOUBLE_MATH_CORRECT 1
#endif

/*
 Detect the endianness at compile-time.
 CTOON_ENDIAN == CTOON_BIG_ENDIAN
 CTOON_ENDIAN == CTOON_LITTLE_ENDIAN
 */
#define CTOON_BIG_ENDIAN       4321
#define CTOON_LITTLE_ENDIAN    1234

#if ctoon_has_include(<sys/types.h>)
#    include <sys/types.h> /* POSIX */
#endif
#if ctoon_has_include(<endian.h>)
#    include <endian.h> /* Linux */
#elif ctoon_has_include(<sys/endian.h>)
#    include <sys/endian.h> /* BSD, Android */
#elif ctoon_has_include(<machine/endian.h>)
#    include <machine/endian.h> /* BSD, Darwin */
#endif

#if defined(BYTE_ORDER) && BYTE_ORDER
#   if defined(BIG_ENDIAN) && (BYTE_ORDER == BIG_ENDIAN)
#       define CTOON_ENDIAN CTOON_BIG_ENDIAN
#   elif defined(LITTLE_ENDIAN) && (BYTE_ORDER == LITTLE_ENDIAN)
#       define CTOON_ENDIAN CTOON_LITTLE_ENDIAN
#   endif
#elif defined(__BYTE_ORDER) && __BYTE_ORDER
#   if defined(__BIG_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)
#       define CTOON_ENDIAN CTOON_BIG_ENDIAN
#   elif defined(__LITTLE_ENDIAN) && (__BYTE_ORDER == __LITTLE_ENDIAN)
#       define CTOON_ENDIAN CTOON_LITTLE_ENDIAN
#   endif
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__
#   if defined(__ORDER_BIG_ENDIAN__) && \
        (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#       define CTOON_ENDIAN CTOON_BIG_ENDIAN
#   elif defined(__ORDER_LITTLE_ENDIAN__) && \
        (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#       define CTOON_ENDIAN CTOON_LITTLE_ENDIAN
#   endif
#elif (defined(__LITTLE_ENDIAN__) && __LITTLE_ENDIAN__ == 1) || \
    defined(__i386) || defined(__i386__) || \
    defined(_X86_) || defined(__X86__) || \
    defined(_M_IX86) || defined(__THW_INTEL__) || \
    defined(__x86_64) || defined(__x86_64__) || \
    defined(__amd64) || defined(__amd64__) || \
    defined(_M_AMD64) || defined(_M_X64) || \
    defined(_M_ARM) || defined(_M_ARM64) || \
    defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || \
    defined(__EMSCRIPTEN__) || defined(__wasm__) || \
    defined(__loongarch__)
#   define CTOON_ENDIAN CTOON_LITTLE_ENDIAN
#elif (defined(__BIG_ENDIAN__) && __BIG_ENDIAN__ == 1) || \
    defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || \
    defined(_MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__) || \
    defined(__or1k__) || defined(__OR1K__)
#   define CTOON_ENDIAN CTOON_BIG_ENDIAN
#else
#   define CTOON_ENDIAN 0 /* unknown endian, detect at run-time */
#endif

/*
 This macro controls how ctoon handles unaligned memory accesses.

 By default, ctoon uses `memcpy()` for memory copying. This allows the compiler
 to optimize the code and emit unaligned memory access instructions when
 supported by the target architecture.

 However, on some older compilers or architectures where `memcpy()` is not
 well-optimized and may result in unnecessary function calls, defining this
 macro as 1 may help. In such cases, ctoon switches to manual byte-by-byte
 access, which can potentially improve performance.

 An example of the generated assembly code for ARM can be found here:
 https://godbolt.org/z/334jjhxPT

 This flag is already enabled for common architectures in the following code,
 so manual configuration is usually unnecessary. If unsure, you can check the
 generated assembly or run benchmarks to make an informed decision.
 */
#ifndef CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
#   if defined(__ia64) || defined(_IA64) || defined(__IA64__) ||  \
        defined(__ia64__) || defined(_M_IA64) || defined(__itanium__)
#       define CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS 1 /* Itanium */
#   elif (defined(__arm__) || defined(__arm64__) || defined(__aarch64__)) && \
        (defined(__GNUC__) || defined(__clang__)) && \
        (!defined(__ARM_FEATURE_UNALIGNED) || !__ARM_FEATURE_UNALIGNED)
#       define CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS 1 /* ARM */
#   elif defined(__sparc) || defined(__sparc__)
#       define CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS 1 /* SPARC */
#   elif defined(__mips) || defined(__mips__) || defined(__MIPS__)
#       define CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS 1 /* MIPS */
#   elif defined(__m68k__) || defined(M68000)
#       define CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS 1 /* M68K */
#   else
#       define CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS 0
#   endif
#endif

/*
 Estimated initial ratio of the TOON data (data_size / value_count).
 For example:

    data:        {"id":12345678,"name":"Harry"}
    data_size:   30
    value_count: 5
    ratio:       6

 ctoon uses dynamic memory with a growth factor of 1.5 when reading and writing
 TOON, the ratios below are used to determine the initial memory size.

 A too large ratio will waste memory, and a too small ratio will cause multiple
 memory growths and degrade performance. Currently, these ratios are generated
 with some commonly used TOON datasets.
 */
#define CTOON_READER_ESTIMATED_PRETTY_RATIO 16
#define CTOON_READER_ESTIMATED_MINIFY_RATIO 6
#define CTOON_WRITER_ESTIMATED_PRETTY_RATIO 32
#define CTOON_WRITER_ESTIMATED_MINIFY_RATIO 18

/* The initial and maximum size of the memory pool's chunk in ctoon_mut_doc. */
#define CTOON_MUT_DOC_STR_POOL_INIT_SIZE   0x100
#define CTOON_MUT_DOC_STR_POOL_MAX_SIZE    0x10000000
#define CTOON_MUT_DOC_VAL_POOL_INIT_SIZE   (0x10 * sizeof(ctoon_mut_val))
#define CTOON_MUT_DOC_VAL_POOL_MAX_SIZE    (0x1000000 * sizeof(ctoon_mut_val))

/* The minimum size of the dynamic allocator's chunk. */
#define CTOON_ALC_DYN_MIN_SIZE             0x1000

/* Default value for compile-time options. */
#ifndef CTOON_DISABLE_READER
#define CTOON_DISABLE_READER 0
#endif
#ifndef CTOON_DISABLE_WRITER
#define CTOON_DISABLE_WRITER 0
#endif
#ifndef CTOON_DISABLE_INCR_READER
#define CTOON_DISABLE_INCR_READER 0
#endif
#ifndef CTOON_DISABLE_UTILS
#define CTOON_DISABLE_UTILS 0
#endif
#ifndef CTOON_DISABLE_FAST_FP_CONV
#define CTOON_DISABLE_FAST_FP_CONV 0
#endif
#ifndef CTOON_DISABLE_NON_STANDARD
#define CTOON_DISABLE_NON_STANDARD 0
#endif
#ifndef CTOON_DISABLE_UTF8_VALIDATION
#define CTOON_DISABLE_UTF8_VALIDATION 0
#endif
#ifndef CTOON_READER_DEPTH_LIMIT
#define CTOON_READER_DEPTH_LIMIT 0
#endif

/*==============================================================================
 * MARK: - Macros (Private)
 *============================================================================*/

/* Macros used for loop unrolling and other purpose. */
#define repeat2(x)  { x x }
#define repeat4(x)  { x x x x }
#define repeat8(x)  { x x x x x x x x }
#define repeat16(x) { x x x x x x x x x x x x x x x x }

#define repeat2_incr(x)   { x(0)  x(1) }
#define repeat4_incr(x)   { x(0)  x(1)  x(2)  x(3) }
#define repeat8_incr(x)   { x(0)  x(1)  x(2)  x(3)  x(4)  x(5)  x(6)  x(7)  }
#define repeat16_incr(x)  { x(0)  x(1)  x(2)  x(3)  x(4)  x(5)  x(6)  x(7)  \
                            x(8)  x(9)  x(10) x(11) x(12) x(13) x(14) x(15) }
#define repeat_in_1_18(x) { x(1)  x(2)  x(3)  x(4)  x(5)  x(6)  x(7)  x(8)  \
                            x(9)  x(10) x(11) x(12) x(13) x(14) x(15) x(16) \
                            x(17) x(18) }

/* Macros used to provide branch prediction information for compiler. */
#undef  likely
#define likely(x)       ctoon_likely(x)
#undef  unlikely
#define unlikely(x)     ctoon_unlikely(x)

/* Macros used to provide inline information for compiler. */
#undef  static_inline
#define static_inline   static ctoon_inline
#undef  static_noinline
#define static_noinline static ctoon_noinline

/* Macros for min and max. */
#undef  ctoon_min
#define ctoon_min(x, y) ((x) < (y) ? (x) : (y))
#undef  ctoon_max
#define ctoon_max(x, y) ((x) > (y) ? (x) : (y))

/* Used to write u64 literal for C89 which doesn't support "ULL" suffix. */
#undef  U64
#define U64(hi, lo) ((((u64)hi##UL) << 32U) + lo##UL)
#undef  U32
#define U32(hi) ((u32)(hi##UL))

/* Used to cast away (remove) const qualifier. */
#define constcast(type) (type)(void *)(size_t)(const void *)

/*
 Compiler barriers for single variables.

 These macros inform GCC that a read or write access to the given memory
 location will occur, preventing certain compiler optimizations or reordering
 around the access to 'val'. They do not emit any actual instructions.

 This is useful when GCC's default optimization strategies are suboptimal and
 precise control over memory access patterns is required.
 These barriers are not needed when using Clang or MSVC.
 */
#if CTOON_IS_REAL_GCC
#   define gcc_load_barrier(val)   __asm__ volatile(""::"m"(val))
#   define gcc_store_barrier(val)  __asm__ volatile("":"=m"(val))
#   define gcc_full_barrier(val)   __asm__ volatile("":"=m"(val):"m"(val))
#else
#   define gcc_load_barrier(val)
#   define gcc_store_barrier(val)
#   define gcc_full_barrier(val)
#endif



/*==============================================================================
 * MARK: - Constants (Private)
 *============================================================================*/

/* Common error messages. */
#define MSG_FOPEN       "failed to open file"
#define MSG_FREAD       "failed to read file"
#define MSG_FWRITE      "failed to write file"
#define MSG_FCLOSE      "failed to close file"
#define MSG_MALLOC      "failed to allocate memory"
#define MSG_CHAR_T      "invalid literal, expected 'true'"
#define MSG_CHAR_F      "invalid literal, expected 'false'"
#define MSG_CHAR_N      "invalid literal, expected 'null'"
#define MSG_CHAR        "unexpected character, expected a TOON value"
#define MSG_ARR_END     "unexpected character, expected ',' or ']'"
#define MSG_OBJ_KEY     "unexpected character, expected a string key"
#define MSG_OBJ_SEP     "unexpected character, expected ':' after key"
#define MSG_OBJ_END     "unexpected character, expected ',' or '}'"
#define MSG_GARBAGE     "unexpected content after document"
#define MSG_NOT_END     "unexpected end of data"
#define MSG_COMMENT     "unclosed multiline comment"
#define MSG_COMMA       "trailing comma is not allowed"
#define MSG_NAN_INF     "nan or inf number is not allowed"
#define MSG_ERR_TYPE    "invalid TOON value type"
#define MSG_ERR_BOM     "UTF-8 byte order mark (BOM) is not supported"
#define MSG_ERR_UTF8    "invalid utf-8 encoding in string"
#define MSG_ERR_UTF16   "UTF-16 encoding is not supported"
#define MSG_ERR_UTF32   "UTF-32 encoding is not supported"
#define MSG_DEPTH       "depth limit exceeded"

/* U64 constant values */
#undef  U64_MAX
#define U64_MAX         U64(0xFFFFFFFF, 0xFFFFFFFF)
#undef  I64_MAX
#define I64_MAX         U64(0x7FFFFFFF, 0xFFFFFFFF)
#undef  USIZE_MAX
#define USIZE_MAX       ((usize)(~(usize)0))

/* Maximum number of digits for reading u32/u64/usize safety (not overflow). */
#undef  U32_SAFE_DIG
#define U32_SAFE_DIG    9   /* u32 max is 4294967295, 10 digits */
#undef  U64_SAFE_DIG
#define U64_SAFE_DIG    19  /* u64 max is 18446744073709551615, 20 digits */
#undef  USIZE_SAFE_DIG
#define USIZE_SAFE_DIG  (sizeof(usize) == 8 ? U64_SAFE_DIG : U32_SAFE_DIG)

/* Inf bits (positive) */
#define F64_BITS_INF U64(0x7FF00000, 0x00000000)

/* NaN bits (quiet NaN, no payload, no sign) */
#if defined(__hppa__) || (defined(__mips__) && !defined(__mips_nan2008))
#define F64_BITS_NAN U64(0x7FF7FFFF, 0xFFFFFFFF)
#else
#define F64_BITS_NAN U64(0x7FF80000, 0x00000000)
#endif

/* maximum significant digits count in decimal when reading double number */
#define F64_MAX_DEC_DIG 768

/* maximum decimal power of double number (1.7976931348623157e308) */
#define F64_MAX_DEC_EXP 308

/* minimum decimal power of double number (4.9406564584124654e-324) */
#define F64_MIN_DEC_EXP (-324)

/* maximum binary power of double number */
#define F64_MAX_BIN_EXP 1024

/* minimum binary power of double number */
#define F64_MIN_BIN_EXP (-1021)

/* float/double number bits */
#define F32_BITS 32
#define F64_BITS 64

/* float/double number exponent part bits */
#define F32_EXP_BITS 8
#define F64_EXP_BITS 11

/* float/double number significand part bits */
#define F32_SIG_BITS 23
#define F64_SIG_BITS 52

/* float/double number significand part bits (with 1 hidden bit) */
#define F32_SIG_FULL_BITS 24
#define F64_SIG_FULL_BITS 53

/* float/double number significand bit mask */
#define F32_SIG_MASK U32(0x007FFFFF)
#define F64_SIG_MASK U64(0x000FFFFF, 0xFFFFFFFF)

/* float/double number exponent bit mask */
#define F32_EXP_MASK U32(0x7F800000)
#define F64_EXP_MASK U64(0x7FF00000, 0x00000000)

/* float/double number exponent bias */
#define F32_EXP_BIAS 127
#define F64_EXP_BIAS 1023

/* float/double number significant digits count in decimal */
#define F32_DEC_DIG 9
#define F64_DEC_DIG 17

/* buffer length required for float/double number writer */
#define FP_BUF_LEN 40

/* maximum length of a number in incremental parsing */
#define INCR_NUM_MAX_LEN 1024



/*==============================================================================
 * MARK: - Types (Private)
 *============================================================================*/

/** Type define for primitive types. */
typedef float       f32;
typedef double      f64;
typedef int8_t      i8;
typedef uint8_t     u8;
typedef int16_t     i16;
typedef uint16_t    u16;
typedef int32_t     i32;
typedef uint32_t    u32;
typedef int64_t     i64;
typedef uint64_t    u64;
typedef size_t      usize;

/** 128-bit integer, used by floating-point number reader and writer. */
#if CTOON_HAS_INT128
__extension__ typedef __int128          i128;
__extension__ typedef unsigned __int128 u128;
#endif

/** 16/32/64-bit vector */
typedef struct v16 { char c[2]; } v16;
typedef struct v32 { char c[4]; } v32;
typedef struct v64 { char c[8]; } v64;

/** 16/32/64-bit vector union */
typedef union v16_uni { v16 v; u16 u; } v16_uni;
typedef union v32_uni { v32 v; u32 u; } v32_uni;
typedef union v64_uni { v64 v; u64 u; } v64_uni;



/*==============================================================================
 * MARK: - Load/Store Utils (Private)
 *============================================================================*/

#define byte_move_idx(x) ((char *)dst)[x] = ((const char *)src)[x];
#define byte_move_src(x) ((char *)tmp)[x] = ((const char *)src)[x];
#define byte_move_dst(x) ((char *)dst)[x] = ((const char *)tmp)[x];

/** Same as `memcpy(dst, src, 2)`, no overlap. */
static_inline void byte_copy_2(void *dst, const void *src) {
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    memcpy(dst, src, 2);
#else
    repeat2_incr(byte_move_idx)
#endif
}

/** Same as `memcpy(dst, src, 4)`, no overlap. */
static_inline void byte_copy_4(void *dst, const void *src) {
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    memcpy(dst, src, 4);
#else
    repeat4_incr(byte_move_idx)
#endif
}

/** Same as `memcpy(dst, src, 8)`, no overlap. */
static_inline void byte_copy_8(void *dst, const void *src) {
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    memcpy(dst, src, 8);
#else
    repeat8_incr(byte_move_idx)
#endif
}

/** Same as `memcpy(dst, src, 16)`, no overlap. */
static_inline void byte_copy_16(void *dst, const void *src) {
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    memcpy(dst, src, 16);
#else
    repeat16_incr(byte_move_idx)
#endif
}

/** Same as `memmove(dst, src, 2)`, allows overlap. */
static_inline void byte_move_2(void *dst, const void *src) {
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    u16 tmp;
    memcpy(&tmp, src, 2);
    memcpy(dst, &tmp, 2);
#else
    char tmp[2];
    repeat2_incr(byte_move_src)
    repeat2_incr(byte_move_dst)
#endif
}

/** Same as `memmove(dst, src, 4)`, allows overlap. */
static_inline void byte_move_4(void *dst, const void *src) {
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    u32 tmp;
    memcpy(&tmp, src, 4);
    memcpy(dst, &tmp, 4);
#else
    char tmp[4];
    repeat4_incr(byte_move_src)
    repeat4_incr(byte_move_dst)
#endif
}

/** Same as `memmove(dst, src, 8)`, allows overlap. */
static_inline void byte_move_8(void *dst, const void *src) {
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    u64 tmp;
    memcpy(&tmp, src, 8);
    memcpy(dst, &tmp, 8);
#else
    char tmp[8];
    repeat8_incr(byte_move_src)
    repeat8_incr(byte_move_dst)
#endif
}

/** Same as `memmove(dst, src, 16)`, allows overlap. */
static_inline void byte_move_16(void *dst, const void *src) {
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    char *pdst = (char *)dst;
    const char *psrc = (const char *)src;
    u64 tmp1, tmp2;
    memcpy(&tmp1, psrc, 8);
    memcpy(&tmp2, psrc + 8, 8);
    memcpy(pdst, &tmp1, 8);
    memcpy(pdst + 8, &tmp2, 8);
#else
    char tmp[16];
    repeat16_incr(byte_move_src)
    repeat16_incr(byte_move_dst)
#endif
}

/** Same as `memmove(dst, src, n)`, but only `dst <= src` and `n <= 16`. */
static_inline void byte_move_forward(void *dst, void *src, usize n) {
    char *d = (char *)dst, *s = (char *)src;
    n += (n % 2); /* round up to even */
    if (n == 16) { byte_move_16(d, s); return; }
    if (n >= 8) { byte_move_8(d, s); n -= 8; d += 8; s += 8; }
    if (n >= 4) { byte_move_4(d, s); n -= 4; d += 4; s += 4; }
    if (n >= 2) { byte_move_2(d, s); }
}

/** Same as `memcmp(buf, pat, 2) == 0`. */
static_inline bool byte_match_2(void *buf, const char *pat) {
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    v16_uni u1, u2;
    memcpy(&u1, buf, 2);
    memcpy(&u2, pat, 2);
    return u1.u == u2.u;
#else
    return ((char *)buf)[0] == ((const char *)pat)[0] &&
           ((char *)buf)[1] == ((const char *)pat)[1];
#endif
}

/** Same as `memcmp(buf, pat, 4) == 0`. */
static_inline bool byte_match_4(void *buf, const char *pat) {
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    v32_uni u1, u2;
    memcpy(&u1, buf, 4);
    memcpy(&u2, pat, 4);
    return u1.u == u2.u;
#else
    return ((char *)buf)[0] == ((const char *)pat)[0] &&
           ((char *)buf)[1] == ((const char *)pat)[1] &&
           ((char *)buf)[2] == ((const char *)pat)[2] &&
           ((char *)buf)[3] == ((const char *)pat)[3];
#endif
}

/** Loads 2 bytes from `src` as a u16 (native-endian). */
static_inline u16 byte_load_2(const void *src) {
    v16_uni uni;
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    memcpy(&uni, src, 2);
#else
    uni.v.c[0] = ((const char *)src)[0];
    uni.v.c[1] = ((const char *)src)[1];
#endif
    return uni.u;
}

/** Loads 3 bytes from `src` as a u32 (native-endian). */
static_inline u32 byte_load_3(const void *src) {
    v32_uni uni;
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    memcpy(&uni, src, 2);
    uni.v.c[2] = ((const char *)src)[2];
    uni.v.c[3] = 0;
#else
    uni.v.c[0] = ((const char *)src)[0];
    uni.v.c[1] = ((const char *)src)[1];
    uni.v.c[2] = ((const char *)src)[2];
    uni.v.c[3] = 0;
#endif
    return uni.u;
}

/** Loads 4 bytes from `src` as a u32 (native-endian). */
static_inline u32 byte_load_4(const void *src) {
    v32_uni uni;
#if !CTOON_DISABLE_UNALIGNED_MEMORY_ACCESS
    memcpy(&uni, src, 4);
#else
    uni.v.c[0] = ((const char *)src)[0];
    uni.v.c[1] = ((const char *)src)[1];
    uni.v.c[2] = ((const char *)src)[2];
    uni.v.c[3] = ((const char *)src)[3];
#endif
    return uni.u;
}



/*==============================================================================
 * MARK: - Character Utils (Private)
 * These lookup tables were generated by `misc/make_tables.c`.
 *============================================================================*/









/** Match UTF-8 byte order mark: EF BB BF. */
static_inline bool is_utf8_bom(const u8 *cur) {
    return byte_load_3(cur) == byte_load_3("\xEF\xBB\xBF");
}
/*==============================================================================
 * MARK: - UTF8 Validation (Private)
 * Each Unicode code point is encoded using 1 to 4 bytes in UTF-8.
 * Validation is performed using a 4-byte mask and pattern-based approach,
 * which requires the input data to be padded with four zero bytes at the end.
 *============================================================================*/

/* Macro for concatenating four u8 into a u32 and keeping the byte order. */
#if CTOON_ENDIAN == CTOON_LITTLE_ENDIAN
#   define utf8_seq_def(name, a, b, c, d) \
        static const u32 utf8_seq_##name = 0x##d##c##b##a##UL;
#   define utf8_seq(name) utf8_seq_##name
#elif CTOON_ENDIAN == CTOON_BIG_ENDIAN
#   define utf8_seq_def(name, a, b, c, d) \
        static const u32 utf8_seq_##name = 0x##a##b##c##d##UL;
#   define utf8_seq(name) utf8_seq_##name
#else
#   define utf8_seq_def(name, a, b, c, d) \
        static const v32_uni utf8_uni_##name = {{ 0x##a, 0x##b, 0x##c, 0x##d }};
#   define utf8_seq(name) utf8_uni_##name.u
#endif

/*
 1-byte sequence (U+0000 to U+007F)
 bit min        [.......0] (U+0000)
 bit max        [.1111111] (U+007F)
 bit mask       [x.......] (80)
 bit pattern    [0.......] (00)
 */



/*==============================================================================
 * MARK: - Power10 Lookup Table (Private)
 * These data are used by the floating-point number reader and writer.
 *============================================================================*/

#if !CTOON_DISABLE_FAST_FP_CONV

/** Maximum pow10 exponent that can be represented exactly as a float64. */
#define F64_POW10_MAX_EXACT_EXP 22

#if CTOON_DOUBLE_MATH_CORRECT
/** Cached pow10 table. */

/** Maximum pow10 exponent that can be represented exactly as a uint64. */
#endif /* !CTOON_DISABLE_READER */
#define U64_POW10_MAX_EXACT_EXP 19

/** Table: [ 10^0, ..., 10^19 ] (generate with misc/make_tables.c) */

/** Minimum decimal exponent in pow10_sig_table. */
#define POW10_SIG_TABLE_MIN_EXP -343

/** Maximum decimal exponent in pow10_sig_table. */
#define POW10_SIG_TABLE_MAX_EXP 324

/** Minimum exact decimal exponent in pow10_sig_table */
#define POW10_SIG_TABLE_MIN_EXACT_EXP 0

/** Maximum exact decimal exponent in pow10_sig_table */
#define POW10_SIG_TABLE_MAX_EXACT_EXP 55

/** Normalized significant 128 bits of pow10, no rounded up (size: 10.4KB).
    This lookup table is used by both the double number reader and writer.
    (generate with misc/make_tables.c) */
static const u64 pow10_sig_table[] = {
    U64(0xBF29DCAB, 0xA82FDEAE), U64(0x7432EE87, 0x3880FC33), /* ~= 10^-343 */
    U64(0xEEF453D6, 0x923BD65A), U64(0x113FAA29, 0x06A13B3F), /* ~= 10^-342 */
    U64(0x9558B466, 0x1B6565F8), U64(0x4AC7CA59, 0xA424C507), /* ~= 10^-341 */
    U64(0xBAAEE17F, 0xA23EBF76), U64(0x5D79BCF0, 0x0D2DF649), /* ~= 10^-340 */
    U64(0xE95A99DF, 0x8ACE6F53), U64(0xF4D82C2C, 0x107973DC), /* ~= 10^-339 */
    U64(0x91D8A02B, 0xB6C10594), U64(0x79071B9B, 0x8A4BE869), /* ~= 10^-338 */
    U64(0xB64EC836, 0xA47146F9), U64(0x9748E282, 0x6CDEE284), /* ~= 10^-337 */
    U64(0xE3E27A44, 0x4D8D98B7), U64(0xFD1B1B23, 0x08169B25), /* ~= 10^-336 */
    U64(0x8E6D8C6A, 0xB0787F72), U64(0xFE30F0F5, 0xE50E20F7), /* ~= 10^-335 */
    U64(0xB208EF85, 0x5C969F4F), U64(0xBDBD2D33, 0x5E51A935), /* ~= 10^-334 */
    U64(0xDE8B2B66, 0xB3BC4723), U64(0xAD2C7880, 0x35E61382), /* ~= 10^-333 */
    U64(0x8B16FB20, 0x3055AC76), U64(0x4C3BCB50, 0x21AFCC31), /* ~= 10^-332 */
    U64(0xADDCB9E8, 0x3C6B1793), U64(0xDF4ABE24, 0x2A1BBF3D), /* ~= 10^-331 */
    U64(0xD953E862, 0x4B85DD78), U64(0xD71D6DAD, 0x34A2AF0D), /* ~= 10^-330 */
    U64(0x87D4713D, 0x6F33AA6B), U64(0x8672648C, 0x40E5AD68), /* ~= 10^-329 */
    U64(0xA9C98D8C, 0xCB009506), U64(0x680EFDAF, 0x511F18C2), /* ~= 10^-328 */
    U64(0xD43BF0EF, 0xFDC0BA48), U64(0x0212BD1B, 0x2566DEF2), /* ~= 10^-327 */
    U64(0x84A57695, 0xFE98746D), U64(0x014BB630, 0xF7604B57), /* ~= 10^-326 */
    U64(0xA5CED43B, 0x7E3E9188), U64(0x419EA3BD, 0x35385E2D), /* ~= 10^-325 */
    U64(0xCF42894A, 0x5DCE35EA), U64(0x52064CAC, 0x828675B9), /* ~= 10^-324 */
    U64(0x818995CE, 0x7AA0E1B2), U64(0x7343EFEB, 0xD1940993), /* ~= 10^-323 */
    U64(0xA1EBFB42, 0x19491A1F), U64(0x1014EBE6, 0xC5F90BF8), /* ~= 10^-322 */
    U64(0xCA66FA12, 0x9F9B60A6), U64(0xD41A26E0, 0x77774EF6), /* ~= 10^-321 */
    U64(0xFD00B897, 0x478238D0), U64(0x8920B098, 0x955522B4), /* ~= 10^-320 */
    U64(0x9E20735E, 0x8CB16382), U64(0x55B46E5F, 0x5D5535B0), /* ~= 10^-319 */
    U64(0xC5A89036, 0x2FDDBC62), U64(0xEB2189F7, 0x34AA831D), /* ~= 10^-318 */
    U64(0xF712B443, 0xBBD52B7B), U64(0xA5E9EC75, 0x01D523E4), /* ~= 10^-317 */
    U64(0x9A6BB0AA, 0x55653B2D), U64(0x47B233C9, 0x2125366E), /* ~= 10^-316 */
    U64(0xC1069CD4, 0xEABE89F8), U64(0x999EC0BB, 0x696E840A), /* ~= 10^-315 */
    U64(0xF148440A, 0x256E2C76), U64(0xC00670EA, 0x43CA250D), /* ~= 10^-314 */
    U64(0x96CD2A86, 0x5764DBCA), U64(0x38040692, 0x6A5E5728), /* ~= 10^-313 */
    U64(0xBC807527, 0xED3E12BC), U64(0xC6050837, 0x04F5ECF2), /* ~= 10^-312 */
    U64(0xEBA09271, 0xE88D976B), U64(0xF7864A44, 0xC633682E), /* ~= 10^-311 */
    U64(0x93445B87, 0x31587EA3), U64(0x7AB3EE6A, 0xFBE0211D), /* ~= 10^-310 */
    U64(0xB8157268, 0xFDAE9E4C), U64(0x5960EA05, 0xBAD82964), /* ~= 10^-309 */
    U64(0xE61ACF03, 0x3D1A45DF), U64(0x6FB92487, 0x298E33BD), /* ~= 10^-308 */
    U64(0x8FD0C162, 0x06306BAB), U64(0xA5D3B6D4, 0x79F8E056), /* ~= 10^-307 */
    U64(0xB3C4F1BA, 0x87BC8696), U64(0x8F48A489, 0x9877186C), /* ~= 10^-306 */
    U64(0xE0B62E29, 0x29ABA83C), U64(0x331ACDAB, 0xFE94DE87), /* ~= 10^-305 */
    U64(0x8C71DCD9, 0xBA0B4925), U64(0x9FF0C08B, 0x7F1D0B14), /* ~= 10^-304 */
    U64(0xAF8E5410, 0x288E1B6F), U64(0x07ECF0AE, 0x5EE44DD9), /* ~= 10^-303 */
    U64(0xDB71E914, 0x32B1A24A), U64(0xC9E82CD9, 0xF69D6150), /* ~= 10^-302 */
    U64(0x892731AC, 0x9FAF056E), U64(0xBE311C08, 0x3A225CD2), /* ~= 10^-301 */
    U64(0xAB70FE17, 0xC79AC6CA), U64(0x6DBD630A, 0x48AAF406), /* ~= 10^-300 */
    U64(0xD64D3D9D, 0xB981787D), U64(0x092CBBCC, 0xDAD5B108), /* ~= 10^-299 */
    U64(0x85F04682, 0x93F0EB4E), U64(0x25BBF560, 0x08C58EA5), /* ~= 10^-298 */
    U64(0xA76C5823, 0x38ED2621), U64(0xAF2AF2B8, 0x0AF6F24E), /* ~= 10^-297 */
    U64(0xD1476E2C, 0x07286FAA), U64(0x1AF5AF66, 0x0DB4AEE1), /* ~= 10^-296 */
    U64(0x82CCA4DB, 0x847945CA), U64(0x50D98D9F, 0xC890ED4D), /* ~= 10^-295 */
    U64(0xA37FCE12, 0x6597973C), U64(0xE50FF107, 0xBAB528A0), /* ~= 10^-294 */
    U64(0xCC5FC196, 0xFEFD7D0C), U64(0x1E53ED49, 0xA96272C8), /* ~= 10^-293 */
    U64(0xFF77B1FC, 0xBEBCDC4F), U64(0x25E8E89C, 0x13BB0F7A), /* ~= 10^-292 */
    U64(0x9FAACF3D, 0xF73609B1), U64(0x77B19161, 0x8C54E9AC), /* ~= 10^-291 */
    U64(0xC795830D, 0x75038C1D), U64(0xD59DF5B9, 0xEF6A2417), /* ~= 10^-290 */
    U64(0xF97AE3D0, 0xD2446F25), U64(0x4B057328, 0x6B44AD1D), /* ~= 10^-289 */
    U64(0x9BECCE62, 0x836AC577), U64(0x4EE367F9, 0x430AEC32), /* ~= 10^-288 */
    U64(0xC2E801FB, 0x244576D5), U64(0x229C41F7, 0x93CDA73F), /* ~= 10^-287 */
    U64(0xF3A20279, 0xED56D48A), U64(0x6B435275, 0x78C1110F), /* ~= 10^-286 */
    U64(0x9845418C, 0x345644D6), U64(0x830A1389, 0x6B78AAA9), /* ~= 10^-285 */
    U64(0xBE5691EF, 0x416BD60C), U64(0x23CC986B, 0xC656D553), /* ~= 10^-284 */
    U64(0xEDEC366B, 0x11C6CB8F), U64(0x2CBFBE86, 0xB7EC8AA8), /* ~= 10^-283 */
    U64(0x94B3A202, 0xEB1C3F39), U64(0x7BF7D714, 0x32F3D6A9), /* ~= 10^-282 */
    U64(0xB9E08A83, 0xA5E34F07), U64(0xDAF5CCD9, 0x3FB0CC53), /* ~= 10^-281 */
    U64(0xE858AD24, 0x8F5C22C9), U64(0xD1B3400F, 0x8F9CFF68), /* ~= 10^-280 */
    U64(0x91376C36, 0xD99995BE), U64(0x23100809, 0xB9C21FA1), /* ~= 10^-279 */
    U64(0xB5854744, 0x8FFFFB2D), U64(0xABD40A0C, 0x2832A78A), /* ~= 10^-278 */
    U64(0xE2E69915, 0xB3FFF9F9), U64(0x16C90C8F, 0x323F516C), /* ~= 10^-277 */
    U64(0x8DD01FAD, 0x907FFC3B), U64(0xAE3DA7D9, 0x7F6792E3), /* ~= 10^-276 */
    U64(0xB1442798, 0xF49FFB4A), U64(0x99CD11CF, 0xDF41779C), /* ~= 10^-275 */
    U64(0xDD95317F, 0x31C7FA1D), U64(0x40405643, 0xD711D583), /* ~= 10^-274 */
    U64(0x8A7D3EEF, 0x7F1CFC52), U64(0x482835EA, 0x666B2572), /* ~= 10^-273 */
    U64(0xAD1C8EAB, 0x5EE43B66), U64(0xDA324365, 0x0005EECF), /* ~= 10^-272 */
    U64(0xD863B256, 0x369D4A40), U64(0x90BED43E, 0x40076A82), /* ~= 10^-271 */
    U64(0x873E4F75, 0xE2224E68), U64(0x5A7744A6, 0xE804A291), /* ~= 10^-270 */
    U64(0xA90DE353, 0x5AAAE202), U64(0x711515D0, 0xA205CB36), /* ~= 10^-269 */
    U64(0xD3515C28, 0x31559A83), U64(0x0D5A5B44, 0xCA873E03), /* ~= 10^-268 */
    U64(0x8412D999, 0x1ED58091), U64(0xE858790A, 0xFE9486C2), /* ~= 10^-267 */
    U64(0xA5178FFF, 0x668AE0B6), U64(0x626E974D, 0xBE39A872), /* ~= 10^-266 */
    U64(0xCE5D73FF, 0x402D98E3), U64(0xFB0A3D21, 0x2DC8128F), /* ~= 10^-265 */
    U64(0x80FA687F, 0x881C7F8E), U64(0x7CE66634, 0xBC9D0B99), /* ~= 10^-264 */
    U64(0xA139029F, 0x6A239F72), U64(0x1C1FFFC1, 0xEBC44E80), /* ~= 10^-263 */
    U64(0xC9874347, 0x44AC874E), U64(0xA327FFB2, 0x66B56220), /* ~= 10^-262 */
    U64(0xFBE91419, 0x15D7A922), U64(0x4BF1FF9F, 0x0062BAA8), /* ~= 10^-261 */
    U64(0x9D71AC8F, 0xADA6C9B5), U64(0x6F773FC3, 0x603DB4A9), /* ~= 10^-260 */
    U64(0xC4CE17B3, 0x99107C22), U64(0xCB550FB4, 0x384D21D3), /* ~= 10^-259 */
    U64(0xF6019DA0, 0x7F549B2B), U64(0x7E2A53A1, 0x46606A48), /* ~= 10^-258 */
    U64(0x99C10284, 0x4F94E0FB), U64(0x2EDA7444, 0xCBFC426D), /* ~= 10^-257 */
    U64(0xC0314325, 0x637A1939), U64(0xFA911155, 0xFEFB5308), /* ~= 10^-256 */
    U64(0xF03D93EE, 0xBC589F88), U64(0x793555AB, 0x7EBA27CA), /* ~= 10^-255 */
    U64(0x96267C75, 0x35B763B5), U64(0x4BC1558B, 0x2F3458DE), /* ~= 10^-254 */
    U64(0xBBB01B92, 0x83253CA2), U64(0x9EB1AAED, 0xFB016F16), /* ~= 10^-253 */
    U64(0xEA9C2277, 0x23EE8BCB), U64(0x465E15A9, 0x79C1CADC), /* ~= 10^-252 */
    U64(0x92A1958A, 0x7675175F), U64(0x0BFACD89, 0xEC191EC9), /* ~= 10^-251 */
    U64(0xB749FAED, 0x14125D36), U64(0xCEF980EC, 0x671F667B), /* ~= 10^-250 */
    U64(0xE51C79A8, 0x5916F484), U64(0x82B7E127, 0x80E7401A), /* ~= 10^-249 */
    U64(0x8F31CC09, 0x37AE58D2), U64(0xD1B2ECB8, 0xB0908810), /* ~= 10^-248 */
    U64(0xB2FE3F0B, 0x8599EF07), U64(0x861FA7E6, 0xDCB4AA15), /* ~= 10^-247 */
    U64(0xDFBDCECE, 0x67006AC9), U64(0x67A791E0, 0x93E1D49A), /* ~= 10^-246 */
    U64(0x8BD6A141, 0x006042BD), U64(0xE0C8BB2C, 0x5C6D24E0), /* ~= 10^-245 */
    U64(0xAECC4991, 0x4078536D), U64(0x58FAE9F7, 0x73886E18), /* ~= 10^-244 */
    U64(0xDA7F5BF5, 0x90966848), U64(0xAF39A475, 0x506A899E), /* ~= 10^-243 */
    U64(0x888F9979, 0x7A5E012D), U64(0x6D8406C9, 0x52429603), /* ~= 10^-242 */
    U64(0xAAB37FD7, 0xD8F58178), U64(0xC8E5087B, 0xA6D33B83), /* ~= 10^-241 */
    U64(0xD5605FCD, 0xCF32E1D6), U64(0xFB1E4A9A, 0x90880A64), /* ~= 10^-240 */
    U64(0x855C3BE0, 0xA17FCD26), U64(0x5CF2EEA0, 0x9A55067F), /* ~= 10^-239 */
    U64(0xA6B34AD8, 0xC9DFC06F), U64(0xF42FAA48, 0xC0EA481E), /* ~= 10^-238 */
    U64(0xD0601D8E, 0xFC57B08B), U64(0xF13B94DA, 0xF124DA26), /* ~= 10^-237 */
    U64(0x823C1279, 0x5DB6CE57), U64(0x76C53D08, 0xD6B70858), /* ~= 10^-236 */
    U64(0xA2CB1717, 0xB52481ED), U64(0x54768C4B, 0x0C64CA6E), /* ~= 10^-235 */
    U64(0xCB7DDCDD, 0xA26DA268), U64(0xA9942F5D, 0xCF7DFD09), /* ~= 10^-234 */
    U64(0xFE5D5415, 0x0B090B02), U64(0xD3F93B35, 0x435D7C4C), /* ~= 10^-233 */
    U64(0x9EFA548D, 0x26E5A6E1), U64(0xC47BC501, 0x4A1A6DAF), /* ~= 10^-232 */
    U64(0xC6B8E9B0, 0x709F109A), U64(0x359AB641, 0x9CA1091B), /* ~= 10^-231 */
    U64(0xF867241C, 0x8CC6D4C0), U64(0xC30163D2, 0x03C94B62), /* ~= 10^-230 */
    U64(0x9B407691, 0xD7FC44F8), U64(0x79E0DE63, 0x425DCF1D), /* ~= 10^-229 */
    U64(0xC2109436, 0x4DFB5636), U64(0x985915FC, 0x12F542E4), /* ~= 10^-228 */
    U64(0xF294B943, 0xE17A2BC4), U64(0x3E6F5B7B, 0x17B2939D), /* ~= 10^-227 */
    U64(0x979CF3CA, 0x6CEC5B5A), U64(0xA705992C, 0xEECF9C42), /* ~= 10^-226 */
    U64(0xBD8430BD, 0x08277231), U64(0x50C6FF78, 0x2A838353), /* ~= 10^-225 */
    U64(0xECE53CEC, 0x4A314EBD), U64(0xA4F8BF56, 0x35246428), /* ~= 10^-224 */
    U64(0x940F4613, 0xAE5ED136), U64(0x871B7795, 0xE136BE99), /* ~= 10^-223 */
    U64(0xB9131798, 0x99F68584), U64(0x28E2557B, 0x59846E3F), /* ~= 10^-222 */
    U64(0xE757DD7E, 0xC07426E5), U64(0x331AEADA, 0x2FE589CF), /* ~= 10^-221 */
    U64(0x9096EA6F, 0x3848984F), U64(0x3FF0D2C8, 0x5DEF7621), /* ~= 10^-220 */
    U64(0xB4BCA50B, 0x065ABE63), U64(0x0FED077A, 0x756B53A9), /* ~= 10^-219 */
    U64(0xE1EBCE4D, 0xC7F16DFB), U64(0xD3E84959, 0x12C62894), /* ~= 10^-218 */
    U64(0x8D3360F0, 0x9CF6E4BD), U64(0x64712DD7, 0xABBBD95C), /* ~= 10^-217 */
    U64(0xB080392C, 0xC4349DEC), U64(0xBD8D794D, 0x96AACFB3), /* ~= 10^-216 */
    U64(0xDCA04777, 0xF541C567), U64(0xECF0D7A0, 0xFC5583A0), /* ~= 10^-215 */
    U64(0x89E42CAA, 0xF9491B60), U64(0xF41686C4, 0x9DB57244), /* ~= 10^-214 */
    U64(0xAC5D37D5, 0xB79B6239), U64(0x311C2875, 0xC522CED5), /* ~= 10^-213 */
    U64(0xD77485CB, 0x25823AC7), U64(0x7D633293, 0x366B828B), /* ~= 10^-212 */
    U64(0x86A8D39E, 0xF77164BC), U64(0xAE5DFF9C, 0x02033197), /* ~= 10^-211 */
    U64(0xA8530886, 0xB54DBDEB), U64(0xD9F57F83, 0x0283FDFC), /* ~= 10^-210 */
    U64(0xD267CAA8, 0x62A12D66), U64(0xD072DF63, 0xC324FD7B), /* ~= 10^-209 */
    U64(0x8380DEA9, 0x3DA4BC60), U64(0x4247CB9E, 0x59F71E6D), /* ~= 10^-208 */
    U64(0xA4611653, 0x8D0DEB78), U64(0x52D9BE85, 0xF074E608), /* ~= 10^-207 */
    U64(0xCD795BE8, 0x70516656), U64(0x67902E27, 0x6C921F8B), /* ~= 10^-206 */
    U64(0x806BD971, 0x4632DFF6), U64(0x00BA1CD8, 0xA3DB53B6), /* ~= 10^-205 */
    U64(0xA086CFCD, 0x97BF97F3), U64(0x80E8A40E, 0xCCD228A4), /* ~= 10^-204 */
    U64(0xC8A883C0, 0xFDAF7DF0), U64(0x6122CD12, 0x8006B2CD), /* ~= 10^-203 */
    U64(0xFAD2A4B1, 0x3D1B5D6C), U64(0x796B8057, 0x20085F81), /* ~= 10^-202 */
    U64(0x9CC3A6EE, 0xC6311A63), U64(0xCBE33036, 0x74053BB0), /* ~= 10^-201 */
    U64(0xC3F490AA, 0x77BD60FC), U64(0xBEDBFC44, 0x11068A9C), /* ~= 10^-200 */
    U64(0xF4F1B4D5, 0x15ACB93B), U64(0xEE92FB55, 0x15482D44), /* ~= 10^-199 */
    U64(0x99171105, 0x2D8BF3C5), U64(0x751BDD15, 0x2D4D1C4A), /* ~= 10^-198 */
    U64(0xBF5CD546, 0x78EEF0B6), U64(0xD262D45A, 0x78A0635D), /* ~= 10^-197 */
    U64(0xEF340A98, 0x172AACE4), U64(0x86FB8971, 0x16C87C34), /* ~= 10^-196 */
    U64(0x9580869F, 0x0E7AAC0E), U64(0xD45D35E6, 0xAE3D4DA0), /* ~= 10^-195 */
    U64(0xBAE0A846, 0xD2195712), U64(0x89748360, 0x59CCA109), /* ~= 10^-194 */
    U64(0xE998D258, 0x869FACD7), U64(0x2BD1A438, 0x703FC94B), /* ~= 10^-193 */
    U64(0x91FF8377, 0x5423CC06), U64(0x7B6306A3, 0x4627DDCF), /* ~= 10^-192 */
    U64(0xB67F6455, 0x292CBF08), U64(0x1A3BC84C, 0x17B1D542), /* ~= 10^-191 */
    U64(0xE41F3D6A, 0x7377EECA), U64(0x20CABA5F, 0x1D9E4A93), /* ~= 10^-190 */
    U64(0x8E938662, 0x882AF53E), U64(0x547EB47B, 0x7282EE9C), /* ~= 10^-189 */
    U64(0xB23867FB, 0x2A35B28D), U64(0xE99E619A, 0x4F23AA43), /* ~= 10^-188 */
    U64(0xDEC681F9, 0xF4C31F31), U64(0x6405FA00, 0xE2EC94D4), /* ~= 10^-187 */
    U64(0x8B3C113C, 0x38F9F37E), U64(0xDE83BC40, 0x8DD3DD04), /* ~= 10^-186 */
    U64(0xAE0B158B, 0x4738705E), U64(0x9624AB50, 0xB148D445), /* ~= 10^-185 */
    U64(0xD98DDAEE, 0x19068C76), U64(0x3BADD624, 0xDD9B0957), /* ~= 10^-184 */
    U64(0x87F8A8D4, 0xCFA417C9), U64(0xE54CA5D7, 0x0A80E5D6), /* ~= 10^-183 */
    U64(0xA9F6D30A, 0x038D1DBC), U64(0x5E9FCF4C, 0xCD211F4C), /* ~= 10^-182 */
    U64(0xD47487CC, 0x8470652B), U64(0x7647C320, 0x0069671F), /* ~= 10^-181 */
    U64(0x84C8D4DF, 0xD2C63F3B), U64(0x29ECD9F4, 0x0041E073), /* ~= 10^-180 */
    U64(0xA5FB0A17, 0xC777CF09), U64(0xF4681071, 0x00525890), /* ~= 10^-179 */
    U64(0xCF79CC9D, 0xB955C2CC), U64(0x7182148D, 0x4066EEB4), /* ~= 10^-178 */
    U64(0x81AC1FE2, 0x93D599BF), U64(0xC6F14CD8, 0x48405530), /* ~= 10^-177 */
    U64(0xA21727DB, 0x38CB002F), U64(0xB8ADA00E, 0x5A506A7C), /* ~= 10^-176 */
    U64(0xCA9CF1D2, 0x06FDC03B), U64(0xA6D90811, 0xF0E4851C), /* ~= 10^-175 */
    U64(0xFD442E46, 0x88BD304A), U64(0x908F4A16, 0x6D1DA663), /* ~= 10^-174 */
    U64(0x9E4A9CEC, 0x15763E2E), U64(0x9A598E4E, 0x043287FE), /* ~= 10^-173 */
    U64(0xC5DD4427, 0x1AD3CDBA), U64(0x40EFF1E1, 0x853F29FD), /* ~= 10^-172 */
    U64(0xF7549530, 0xE188C128), U64(0xD12BEE59, 0xE68EF47C), /* ~= 10^-171 */
    U64(0x9A94DD3E, 0x8CF578B9), U64(0x82BB74F8, 0x301958CE), /* ~= 10^-170 */
    U64(0xC13A148E, 0x3032D6E7), U64(0xE36A5236, 0x3C1FAF01), /* ~= 10^-169 */
    U64(0xF18899B1, 0xBC3F8CA1), U64(0xDC44E6C3, 0xCB279AC1), /* ~= 10^-168 */
    U64(0x96F5600F, 0x15A7B7E5), U64(0x29AB103A, 0x5EF8C0B9), /* ~= 10^-167 */
    U64(0xBCB2B812, 0xDB11A5DE), U64(0x7415D448, 0xF6B6F0E7), /* ~= 10^-166 */
    U64(0xEBDF6617, 0x91D60F56), U64(0x111B495B, 0x3464AD21), /* ~= 10^-165 */
    U64(0x936B9FCE, 0xBB25C995), U64(0xCAB10DD9, 0x00BEEC34), /* ~= 10^-164 */
    U64(0xB84687C2, 0x69EF3BFB), U64(0x3D5D514F, 0x40EEA742), /* ~= 10^-163 */
    U64(0xE65829B3, 0x046B0AFA), U64(0x0CB4A5A3, 0x112A5112), /* ~= 10^-162 */
    U64(0x8FF71A0F, 0xE2C2E6DC), U64(0x47F0E785, 0xEABA72AB), /* ~= 10^-161 */
    U64(0xB3F4E093, 0xDB73A093), U64(0x59ED2167, 0x65690F56), /* ~= 10^-160 */
    U64(0xE0F218B8, 0xD25088B8), U64(0x306869C1, 0x3EC3532C), /* ~= 10^-159 */
    U64(0x8C974F73, 0x83725573), U64(0x1E414218, 0xC73A13FB), /* ~= 10^-158 */
    U64(0xAFBD2350, 0x644EEACF), U64(0xE5D1929E, 0xF90898FA), /* ~= 10^-157 */
    U64(0xDBAC6C24, 0x7D62A583), U64(0xDF45F746, 0xB74ABF39), /* ~= 10^-156 */
    U64(0x894BC396, 0xCE5DA772), U64(0x6B8BBA8C, 0x328EB783), /* ~= 10^-155 */
    U64(0xAB9EB47C, 0x81F5114F), U64(0x066EA92F, 0x3F326564), /* ~= 10^-154 */
    U64(0xD686619B, 0xA27255A2), U64(0xC80A537B, 0x0EFEFEBD), /* ~= 10^-153 */
    U64(0x8613FD01, 0x45877585), U64(0xBD06742C, 0xE95F5F36), /* ~= 10^-152 */
    U64(0xA798FC41, 0x96E952E7), U64(0x2C481138, 0x23B73704), /* ~= 10^-151 */
    U64(0xD17F3B51, 0xFCA3A7A0), U64(0xF75A1586, 0x2CA504C5), /* ~= 10^-150 */
    U64(0x82EF8513, 0x3DE648C4), U64(0x9A984D73, 0xDBE722FB), /* ~= 10^-149 */
    U64(0xA3AB6658, 0x0D5FDAF5), U64(0xC13E60D0, 0xD2E0EBBA), /* ~= 10^-148 */
    U64(0xCC963FEE, 0x10B7D1B3), U64(0x318DF905, 0x079926A8), /* ~= 10^-147 */
    U64(0xFFBBCFE9, 0x94E5C61F), U64(0xFDF17746, 0x497F7052), /* ~= 10^-146 */
    U64(0x9FD561F1, 0xFD0F9BD3), U64(0xFEB6EA8B, 0xEDEFA633), /* ~= 10^-145 */
    U64(0xC7CABA6E, 0x7C5382C8), U64(0xFE64A52E, 0xE96B8FC0), /* ~= 10^-144 */
    U64(0xF9BD690A, 0x1B68637B), U64(0x3DFDCE7A, 0xA3C673B0), /* ~= 10^-143 */
    U64(0x9C1661A6, 0x51213E2D), U64(0x06BEA10C, 0xA65C084E), /* ~= 10^-142 */
    U64(0xC31BFA0F, 0xE5698DB8), U64(0x486E494F, 0xCFF30A62), /* ~= 10^-141 */
    U64(0xF3E2F893, 0xDEC3F126), U64(0x5A89DBA3, 0xC3EFCCFA), /* ~= 10^-140 */
    U64(0x986DDB5C, 0x6B3A76B7), U64(0xF8962946, 0x5A75E01C), /* ~= 10^-139 */
    U64(0xBE895233, 0x86091465), U64(0xF6BBB397, 0xF1135823), /* ~= 10^-138 */
    U64(0xEE2BA6C0, 0x678B597F), U64(0x746AA07D, 0xED582E2C), /* ~= 10^-137 */
    U64(0x94DB4838, 0x40B717EF), U64(0xA8C2A44E, 0xB4571CDC), /* ~= 10^-136 */
    U64(0xBA121A46, 0x50E4DDEB), U64(0x92F34D62, 0x616CE413), /* ~= 10^-135 */
    U64(0xE896A0D7, 0xE51E1566), U64(0x77B020BA, 0xF9C81D17), /* ~= 10^-134 */
    U64(0x915E2486, 0xEF32CD60), U64(0x0ACE1474, 0xDC1D122E), /* ~= 10^-133 */
    U64(0xB5B5ADA8, 0xAAFF80B8), U64(0x0D819992, 0x132456BA), /* ~= 10^-132 */
    U64(0xE3231912, 0xD5BF60E6), U64(0x10E1FFF6, 0x97ED6C69), /* ~= 10^-131 */
    U64(0x8DF5EFAB, 0xC5979C8F), U64(0xCA8D3FFA, 0x1EF463C1), /* ~= 10^-130 */
    U64(0xB1736B96, 0xB6FD83B3), U64(0xBD308FF8, 0xA6B17CB2), /* ~= 10^-129 */
    U64(0xDDD0467C, 0x64BCE4A0), U64(0xAC7CB3F6, 0xD05DDBDE), /* ~= 10^-128 */
    U64(0x8AA22C0D, 0xBEF60EE4), U64(0x6BCDF07A, 0x423AA96B), /* ~= 10^-127 */
    U64(0xAD4AB711, 0x2EB3929D), U64(0x86C16C98, 0xD2C953C6), /* ~= 10^-126 */
    U64(0xD89D64D5, 0x7A607744), U64(0xE871C7BF, 0x077BA8B7), /* ~= 10^-125 */
    U64(0x87625F05, 0x6C7C4A8B), U64(0x11471CD7, 0x64AD4972), /* ~= 10^-124 */
    U64(0xA93AF6C6, 0xC79B5D2D), U64(0xD598E40D, 0x3DD89BCF), /* ~= 10^-123 */
    U64(0xD389B478, 0x79823479), U64(0x4AFF1D10, 0x8D4EC2C3), /* ~= 10^-122 */
    U64(0x843610CB, 0x4BF160CB), U64(0xCEDF722A, 0x585139BA), /* ~= 10^-121 */
    U64(0xA54394FE, 0x1EEDB8FE), U64(0xC2974EB4, 0xEE658828), /* ~= 10^-120 */
    U64(0xCE947A3D, 0xA6A9273E), U64(0x733D2262, 0x29FEEA32), /* ~= 10^-119 */
    U64(0x811CCC66, 0x8829B887), U64(0x0806357D, 0x5A3F525F), /* ~= 10^-118 */
    U64(0xA163FF80, 0x2A3426A8), U64(0xCA07C2DC, 0xB0CF26F7), /* ~= 10^-117 */
    U64(0xC9BCFF60, 0x34C13052), U64(0xFC89B393, 0xDD02F0B5), /* ~= 10^-116 */
    U64(0xFC2C3F38, 0x41F17C67), U64(0xBBAC2078, 0xD443ACE2), /* ~= 10^-115 */
    U64(0x9D9BA783, 0x2936EDC0), U64(0xD54B944B, 0x84AA4C0D), /* ~= 10^-114 */
    U64(0xC5029163, 0xF384A931), U64(0x0A9E795E, 0x65D4DF11), /* ~= 10^-113 */
    U64(0xF64335BC, 0xF065D37D), U64(0x4D4617B5, 0xFF4A16D5), /* ~= 10^-112 */
    U64(0x99EA0196, 0x163FA42E), U64(0x504BCED1, 0xBF8E4E45), /* ~= 10^-111 */
    U64(0xC06481FB, 0x9BCF8D39), U64(0xE45EC286, 0x2F71E1D6), /* ~= 10^-110 */
    U64(0xF07DA27A, 0x82C37088), U64(0x5D767327, 0xBB4E5A4C), /* ~= 10^-109 */
    U64(0x964E858C, 0x91BA2655), U64(0x3A6A07F8, 0xD510F86F), /* ~= 10^-108 */
    U64(0xBBE226EF, 0xB628AFEA), U64(0x890489F7, 0x0A55368B), /* ~= 10^-107 */
    U64(0xEADAB0AB, 0xA3B2DBE5), U64(0x2B45AC74, 0xCCEA842E), /* ~= 10^-106 */
    U64(0x92C8AE6B, 0x464FC96F), U64(0x3B0B8BC9, 0x0012929D), /* ~= 10^-105 */
    U64(0xB77ADA06, 0x17E3BBCB), U64(0x09CE6EBB, 0x40173744), /* ~= 10^-104 */
    U64(0xE5599087, 0x9DDCAABD), U64(0xCC420A6A, 0x101D0515), /* ~= 10^-103 */
    U64(0x8F57FA54, 0xC2A9EAB6), U64(0x9FA94682, 0x4A12232D), /* ~= 10^-102 */
    U64(0xB32DF8E9, 0xF3546564), U64(0x47939822, 0xDC96ABF9), /* ~= 10^-101 */
    U64(0xDFF97724, 0x70297EBD), U64(0x59787E2B, 0x93BC56F7), /* ~= 10^-100 */
    U64(0x8BFBEA76, 0xC619EF36), U64(0x57EB4EDB, 0x3C55B65A), /* ~= 10^-99 */
    U64(0xAEFAE514, 0x77A06B03), U64(0xEDE62292, 0x0B6B23F1), /* ~= 10^-98 */
    U64(0xDAB99E59, 0x958885C4), U64(0xE95FAB36, 0x8E45ECED), /* ~= 10^-97 */
    U64(0x88B402F7, 0xFD75539B), U64(0x11DBCB02, 0x18EBB414), /* ~= 10^-96 */
    U64(0xAAE103B5, 0xFCD2A881), U64(0xD652BDC2, 0x9F26A119), /* ~= 10^-95 */
    U64(0xD59944A3, 0x7C0752A2), U64(0x4BE76D33, 0x46F0495F), /* ~= 10^-94 */
    U64(0x857FCAE6, 0x2D8493A5), U64(0x6F70A440, 0x0C562DDB), /* ~= 10^-93 */
    U64(0xA6DFBD9F, 0xB8E5B88E), U64(0xCB4CCD50, 0x0F6BB952), /* ~= 10^-92 */
    U64(0xD097AD07, 0xA71F26B2), U64(0x7E2000A4, 0x1346A7A7), /* ~= 10^-91 */
    U64(0x825ECC24, 0xC873782F), U64(0x8ED40066, 0x8C0C28C8), /* ~= 10^-90 */
    U64(0xA2F67F2D, 0xFA90563B), U64(0x72890080, 0x2F0F32FA), /* ~= 10^-89 */
    U64(0xCBB41EF9, 0x79346BCA), U64(0x4F2B40A0, 0x3AD2FFB9), /* ~= 10^-88 */
    U64(0xFEA126B7, 0xD78186BC), U64(0xE2F610C8, 0x4987BFA8), /* ~= 10^-87 */
    U64(0x9F24B832, 0xE6B0F436), U64(0x0DD9CA7D, 0x2DF4D7C9), /* ~= 10^-86 */
    U64(0xC6EDE63F, 0xA05D3143), U64(0x91503D1C, 0x79720DBB), /* ~= 10^-85 */
    U64(0xF8A95FCF, 0x88747D94), U64(0x75A44C63, 0x97CE912A), /* ~= 10^-84 */
    U64(0x9B69DBE1, 0xB548CE7C), U64(0xC986AFBE, 0x3EE11ABA), /* ~= 10^-83 */
    U64(0xC24452DA, 0x229B021B), U64(0xFBE85BAD, 0xCE996168), /* ~= 10^-82 */
    U64(0xF2D56790, 0xAB41C2A2), U64(0xFAE27299, 0x423FB9C3), /* ~= 10^-81 */
    U64(0x97C560BA, 0x6B0919A5), U64(0xDCCD879F, 0xC967D41A), /* ~= 10^-80 */
    U64(0xBDB6B8E9, 0x05CB600F), U64(0x5400E987, 0xBBC1C920), /* ~= 10^-79 */
    U64(0xED246723, 0x473E3813), U64(0x290123E9, 0xAAB23B68), /* ~= 10^-78 */
    U64(0x9436C076, 0x0C86E30B), U64(0xF9A0B672, 0x0AAF6521), /* ~= 10^-77 */
    U64(0xB9447093, 0x8FA89BCE), U64(0xF808E40E, 0x8D5B3E69), /* ~= 10^-76 */
    U64(0xE7958CB8, 0x7392C2C2), U64(0xB60B1D12, 0x30B20E04), /* ~= 10^-75 */
    U64(0x90BD77F3, 0x483BB9B9), U64(0xB1C6F22B, 0x5E6F48C2), /* ~= 10^-74 */
    U64(0xB4ECD5F0, 0x1A4AA828), U64(0x1E38AEB6, 0x360B1AF3), /* ~= 10^-73 */
    U64(0xE2280B6C, 0x20DD5232), U64(0x25C6DA63, 0xC38DE1B0), /* ~= 10^-72 */
    U64(0x8D590723, 0x948A535F), U64(0x579C487E, 0x5A38AD0E), /* ~= 10^-71 */
    U64(0xB0AF48EC, 0x79ACE837), U64(0x2D835A9D, 0xF0C6D851), /* ~= 10^-70 */
    U64(0xDCDB1B27, 0x98182244), U64(0xF8E43145, 0x6CF88E65), /* ~= 10^-69 */
    U64(0x8A08F0F8, 0xBF0F156B), U64(0x1B8E9ECB, 0x641B58FF), /* ~= 10^-68 */
    U64(0xAC8B2D36, 0xEED2DAC5), U64(0xE272467E, 0x3D222F3F), /* ~= 10^-67 */
    U64(0xD7ADF884, 0xAA879177), U64(0x5B0ED81D, 0xCC6ABB0F), /* ~= 10^-66 */
    U64(0x86CCBB52, 0xEA94BAEA), U64(0x98E94712, 0x9FC2B4E9), /* ~= 10^-65 */
    U64(0xA87FEA27, 0xA539E9A5), U64(0x3F2398D7, 0x47B36224), /* ~= 10^-64 */
    U64(0xD29FE4B1, 0x8E88640E), U64(0x8EEC7F0D, 0x19A03AAD), /* ~= 10^-63 */
    U64(0x83A3EEEE, 0xF9153E89), U64(0x1953CF68, 0x300424AC), /* ~= 10^-62 */
    U64(0xA48CEAAA, 0xB75A8E2B), U64(0x5FA8C342, 0x3C052DD7), /* ~= 10^-61 */
    U64(0xCDB02555, 0x653131B6), U64(0x3792F412, 0xCB06794D), /* ~= 10^-60 */
    U64(0x808E1755, 0x5F3EBF11), U64(0xE2BBD88B, 0xBEE40BD0), /* ~= 10^-59 */
    U64(0xA0B19D2A, 0xB70E6ED6), U64(0x5B6ACEAE, 0xAE9D0EC4), /* ~= 10^-58 */
    U64(0xC8DE0475, 0x64D20A8B), U64(0xF245825A, 0x5A445275), /* ~= 10^-57 */
    U64(0xFB158592, 0xBE068D2E), U64(0xEED6E2F0, 0xF0D56712), /* ~= 10^-56 */
    U64(0x9CED737B, 0xB6C4183D), U64(0x55464DD6, 0x9685606B), /* ~= 10^-55 */
    U64(0xC428D05A, 0xA4751E4C), U64(0xAA97E14C, 0x3C26B886), /* ~= 10^-54 */
    U64(0xF5330471, 0x4D9265DF), U64(0xD53DD99F, 0x4B3066A8), /* ~= 10^-53 */
    U64(0x993FE2C6, 0xD07B7FAB), U64(0xE546A803, 0x8EFE4029), /* ~= 10^-52 */
    U64(0xBF8FDB78, 0x849A5F96), U64(0xDE985204, 0x72BDD033), /* ~= 10^-51 */
    U64(0xEF73D256, 0xA5C0F77C), U64(0x963E6685, 0x8F6D4440), /* ~= 10^-50 */
    U64(0x95A86376, 0x27989AAD), U64(0xDDE70013, 0x79A44AA8), /* ~= 10^-49 */
    U64(0xBB127C53, 0xB17EC159), U64(0x5560C018, 0x580D5D52), /* ~= 10^-48 */
    U64(0xE9D71B68, 0x9DDE71AF), U64(0xAAB8F01E, 0x6E10B4A6), /* ~= 10^-47 */
    U64(0x92267121, 0x62AB070D), U64(0xCAB39613, 0x04CA70E8), /* ~= 10^-46 */
    U64(0xB6B00D69, 0xBB55C8D1), U64(0x3D607B97, 0xC5FD0D22), /* ~= 10^-45 */
    U64(0xE45C10C4, 0x2A2B3B05), U64(0x8CB89A7D, 0xB77C506A), /* ~= 10^-44 */
    U64(0x8EB98A7A, 0x9A5B04E3), U64(0x77F3608E, 0x92ADB242), /* ~= 10^-43 */
    U64(0xB267ED19, 0x40F1C61C), U64(0x55F038B2, 0x37591ED3), /* ~= 10^-42 */
    U64(0xDF01E85F, 0x912E37A3), U64(0x6B6C46DE, 0xC52F6688), /* ~= 10^-41 */
    U64(0x8B61313B, 0xBABCE2C6), U64(0x2323AC4B, 0x3B3DA015), /* ~= 10^-40 */
    U64(0xAE397D8A, 0xA96C1B77), U64(0xABEC975E, 0x0A0D081A), /* ~= 10^-39 */
    U64(0xD9C7DCED, 0x53C72255), U64(0x96E7BD35, 0x8C904A21), /* ~= 10^-38 */
    U64(0x881CEA14, 0x545C7575), U64(0x7E50D641, 0x77DA2E54), /* ~= 10^-37 */
    U64(0xAA242499, 0x697392D2), U64(0xDDE50BD1, 0xD5D0B9E9), /* ~= 10^-36 */
    U64(0xD4AD2DBF, 0xC3D07787), U64(0x955E4EC6, 0x4B44E864), /* ~= 10^-35 */
    U64(0x84EC3C97, 0xDA624AB4), U64(0xBD5AF13B, 0xEF0B113E), /* ~= 10^-34 */
    U64(0xA6274BBD, 0xD0FADD61), U64(0xECB1AD8A, 0xEACDD58E), /* ~= 10^-33 */
    U64(0xCFB11EAD, 0x453994BA), U64(0x67DE18ED, 0xA5814AF2), /* ~= 10^-32 */
    U64(0x81CEB32C, 0x4B43FCF4), U64(0x80EACF94, 0x8770CED7), /* ~= 10^-31 */
    U64(0xA2425FF7, 0x5E14FC31), U64(0xA1258379, 0xA94D028D), /* ~= 10^-30 */
    U64(0xCAD2F7F5, 0x359A3B3E), U64(0x096EE458, 0x13A04330), /* ~= 10^-29 */
    U64(0xFD87B5F2, 0x8300CA0D), U64(0x8BCA9D6E, 0x188853FC), /* ~= 10^-28 */
    U64(0x9E74D1B7, 0x91E07E48), U64(0x775EA264, 0xCF55347D), /* ~= 10^-27 */
    U64(0xC6120625, 0x76589DDA), U64(0x95364AFE, 0x032A819D), /* ~= 10^-26 */
    U64(0xF79687AE, 0xD3EEC551), U64(0x3A83DDBD, 0x83F52204), /* ~= 10^-25 */
    U64(0x9ABE14CD, 0x44753B52), U64(0xC4926A96, 0x72793542), /* ~= 10^-24 */
    U64(0xC16D9A00, 0x95928A27), U64(0x75B7053C, 0x0F178293), /* ~= 10^-23 */
    U64(0xF1C90080, 0xBAF72CB1), U64(0x5324C68B, 0x12DD6338), /* ~= 10^-22 */
    U64(0x971DA050, 0x74DA7BEE), U64(0xD3F6FC16, 0xEBCA5E03), /* ~= 10^-21 */
    U64(0xBCE50864, 0x92111AEA), U64(0x88F4BB1C, 0xA6BCF584), /* ~= 10^-20 */
    U64(0xEC1E4A7D, 0xB69561A5), U64(0x2B31E9E3, 0xD06C32E5), /* ~= 10^-19 */
    U64(0x9392EE8E, 0x921D5D07), U64(0x3AFF322E, 0x62439FCF), /* ~= 10^-18 */
    U64(0xB877AA32, 0x36A4B449), U64(0x09BEFEB9, 0xFAD487C2), /* ~= 10^-17 */
    U64(0xE69594BE, 0xC44DE15B), U64(0x4C2EBE68, 0x7989A9B3), /* ~= 10^-16 */
    U64(0x901D7CF7, 0x3AB0ACD9), U64(0x0F9D3701, 0x4BF60A10), /* ~= 10^-15 */
    U64(0xB424DC35, 0x095CD80F), U64(0x538484C1, 0x9EF38C94), /* ~= 10^-14 */
    U64(0xE12E1342, 0x4BB40E13), U64(0x2865A5F2, 0x06B06FB9), /* ~= 10^-13 */
    U64(0x8CBCCC09, 0x6F5088CB), U64(0xF93F87B7, 0x442E45D3), /* ~= 10^-12 */
    U64(0xAFEBFF0B, 0xCB24AAFE), U64(0xF78F69A5, 0x1539D748), /* ~= 10^-11 */
    U64(0xDBE6FECE, 0xBDEDD5BE), U64(0xB573440E, 0x5A884D1B), /* ~= 10^-10 */
    U64(0x89705F41, 0x36B4A597), U64(0x31680A88, 0xF8953030), /* ~= 10^-9 */
    U64(0xABCC7711, 0x8461CEFC), U64(0xFDC20D2B, 0x36BA7C3D), /* ~= 10^-8 */
    U64(0xD6BF94D5, 0xE57A42BC), U64(0x3D329076, 0x04691B4C), /* ~= 10^-7 */
    U64(0x8637BD05, 0xAF6C69B5), U64(0xA63F9A49, 0xC2C1B10F), /* ~= 10^-6 */
    U64(0xA7C5AC47, 0x1B478423), U64(0x0FCF80DC, 0x33721D53), /* ~= 10^-5 */
    U64(0xD1B71758, 0xE219652B), U64(0xD3C36113, 0x404EA4A8), /* ~= 10^-4 */
    U64(0x83126E97, 0x8D4FDF3B), U64(0x645A1CAC, 0x083126E9), /* ~= 10^-3 */
    U64(0xA3D70A3D, 0x70A3D70A), U64(0x3D70A3D7, 0x0A3D70A3), /* ~= 10^-2 */
    U64(0xCCCCCCCC, 0xCCCCCCCC), U64(0xCCCCCCCC, 0xCCCCCCCC), /* ~= 10^-1 */
    U64(0x80000000, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^0 */
    U64(0xA0000000, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^1 */
    U64(0xC8000000, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^2 */
    U64(0xFA000000, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^3 */
    U64(0x9C400000, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^4 */
    U64(0xC3500000, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^5 */
    U64(0xF4240000, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^6 */
    U64(0x98968000, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^7 */
    U64(0xBEBC2000, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^8 */
    U64(0xEE6B2800, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^9 */
    U64(0x9502F900, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^10 */
    U64(0xBA43B740, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^11 */
    U64(0xE8D4A510, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^12 */
    U64(0x9184E72A, 0x00000000), U64(0x00000000, 0x00000000), /* == 10^13 */
    U64(0xB5E620F4, 0x80000000), U64(0x00000000, 0x00000000), /* == 10^14 */
    U64(0xE35FA931, 0xA0000000), U64(0x00000000, 0x00000000), /* == 10^15 */
    U64(0x8E1BC9BF, 0x04000000), U64(0x00000000, 0x00000000), /* == 10^16 */
    U64(0xB1A2BC2E, 0xC5000000), U64(0x00000000, 0x00000000), /* == 10^17 */
    U64(0xDE0B6B3A, 0x76400000), U64(0x00000000, 0x00000000), /* == 10^18 */
    U64(0x8AC72304, 0x89E80000), U64(0x00000000, 0x00000000), /* == 10^19 */
    U64(0xAD78EBC5, 0xAC620000), U64(0x00000000, 0x00000000), /* == 10^20 */
    U64(0xD8D726B7, 0x177A8000), U64(0x00000000, 0x00000000), /* == 10^21 */
    U64(0x87867832, 0x6EAC9000), U64(0x00000000, 0x00000000), /* == 10^22 */
    U64(0xA968163F, 0x0A57B400), U64(0x00000000, 0x00000000), /* == 10^23 */
    U64(0xD3C21BCE, 0xCCEDA100), U64(0x00000000, 0x00000000), /* == 10^24 */
    U64(0x84595161, 0x401484A0), U64(0x00000000, 0x00000000), /* == 10^25 */
    U64(0xA56FA5B9, 0x9019A5C8), U64(0x00000000, 0x00000000), /* == 10^26 */
    U64(0xCECB8F27, 0xF4200F3A), U64(0x00000000, 0x00000000), /* == 10^27 */
    U64(0x813F3978, 0xF8940984), U64(0x40000000, 0x00000000), /* == 10^28 */
    U64(0xA18F07D7, 0x36B90BE5), U64(0x50000000, 0x00000000), /* == 10^29 */
    U64(0xC9F2C9CD, 0x04674EDE), U64(0xA4000000, 0x00000000), /* == 10^30 */
    U64(0xFC6F7C40, 0x45812296), U64(0x4D000000, 0x00000000), /* == 10^31 */
    U64(0x9DC5ADA8, 0x2B70B59D), U64(0xF0200000, 0x00000000), /* == 10^32 */
    U64(0xC5371912, 0x364CE305), U64(0x6C280000, 0x00000000), /* == 10^33 */
    U64(0xF684DF56, 0xC3E01BC6), U64(0xC7320000, 0x00000000), /* == 10^34 */
    U64(0x9A130B96, 0x3A6C115C), U64(0x3C7F4000, 0x00000000), /* == 10^35 */
    U64(0xC097CE7B, 0xC90715B3), U64(0x4B9F1000, 0x00000000), /* == 10^36 */
    U64(0xF0BDC21A, 0xBB48DB20), U64(0x1E86D400, 0x00000000), /* == 10^37 */
    U64(0x96769950, 0xB50D88F4), U64(0x13144480, 0x00000000), /* == 10^38 */
    U64(0xBC143FA4, 0xE250EB31), U64(0x17D955A0, 0x00000000), /* == 10^39 */
    U64(0xEB194F8E, 0x1AE525FD), U64(0x5DCFAB08, 0x00000000), /* == 10^40 */
    U64(0x92EFD1B8, 0xD0CF37BE), U64(0x5AA1CAE5, 0x00000000), /* == 10^41 */
    U64(0xB7ABC627, 0x050305AD), U64(0xF14A3D9E, 0x40000000), /* == 10^42 */
    U64(0xE596B7B0, 0xC643C719), U64(0x6D9CCD05, 0xD0000000), /* == 10^43 */
    U64(0x8F7E32CE, 0x7BEA5C6F), U64(0xE4820023, 0xA2000000), /* == 10^44 */
    U64(0xB35DBF82, 0x1AE4F38B), U64(0xDDA2802C, 0x8A800000), /* == 10^45 */
    U64(0xE0352F62, 0xA19E306E), U64(0xD50B2037, 0xAD200000), /* == 10^46 */
    U64(0x8C213D9D, 0xA502DE45), U64(0x4526F422, 0xCC340000), /* == 10^47 */
    U64(0xAF298D05, 0x0E4395D6), U64(0x9670B12B, 0x7F410000), /* == 10^48 */
    U64(0xDAF3F046, 0x51D47B4C), U64(0x3C0CDD76, 0x5F114000), /* == 10^49 */
    U64(0x88D8762B, 0xF324CD0F), U64(0xA5880A69, 0xFB6AC800), /* == 10^50 */
    U64(0xAB0E93B6, 0xEFEE0053), U64(0x8EEA0D04, 0x7A457A00), /* == 10^51 */
    U64(0xD5D238A4, 0xABE98068), U64(0x72A49045, 0x98D6D880), /* == 10^52 */
    U64(0x85A36366, 0xEB71F041), U64(0x47A6DA2B, 0x7F864750), /* == 10^53 */
    U64(0xA70C3C40, 0xA64E6C51), U64(0x999090B6, 0x5F67D924), /* == 10^54 */
    U64(0xD0CF4B50, 0xCFE20765), U64(0xFFF4B4E3, 0xF741CF6D), /* == 10^55 */
    U64(0x82818F12, 0x81ED449F), U64(0xBFF8F10E, 0x7A8921A4), /* ~= 10^56 */
    U64(0xA321F2D7, 0x226895C7), U64(0xAFF72D52, 0x192B6A0D), /* ~= 10^57 */
    U64(0xCBEA6F8C, 0xEB02BB39), U64(0x9BF4F8A6, 0x9F764490), /* ~= 10^58 */
    U64(0xFEE50B70, 0x25C36A08), U64(0x02F236D0, 0x4753D5B4), /* ~= 10^59 */
    U64(0x9F4F2726, 0x179A2245), U64(0x01D76242, 0x2C946590), /* ~= 10^60 */
    U64(0xC722F0EF, 0x9D80AAD6), U64(0x424D3AD2, 0xB7B97EF5), /* ~= 10^61 */
    U64(0xF8EBAD2B, 0x84E0D58B), U64(0xD2E08987, 0x65A7DEB2), /* ~= 10^62 */
    U64(0x9B934C3B, 0x330C8577), U64(0x63CC55F4, 0x9F88EB2F), /* ~= 10^63 */
    U64(0xC2781F49, 0xFFCFA6D5), U64(0x3CBF6B71, 0xC76B25FB), /* ~= 10^64 */
    U64(0xF316271C, 0x7FC3908A), U64(0x8BEF464E, 0x3945EF7A), /* ~= 10^65 */
    U64(0x97EDD871, 0xCFDA3A56), U64(0x97758BF0, 0xE3CBB5AC), /* ~= 10^66 */
    U64(0xBDE94E8E, 0x43D0C8EC), U64(0x3D52EEED, 0x1CBEA317), /* ~= 10^67 */
    U64(0xED63A231, 0xD4C4FB27), U64(0x4CA7AAA8, 0x63EE4BDD), /* ~= 10^68 */
    U64(0x945E455F, 0x24FB1CF8), U64(0x8FE8CAA9, 0x3E74EF6A), /* ~= 10^69 */
    U64(0xB975D6B6, 0xEE39E436), U64(0xB3E2FD53, 0x8E122B44), /* ~= 10^70 */
    U64(0xE7D34C64, 0xA9C85D44), U64(0x60DBBCA8, 0x7196B616), /* ~= 10^71 */
    U64(0x90E40FBE, 0xEA1D3A4A), U64(0xBC8955E9, 0x46FE31CD), /* ~= 10^72 */
    U64(0xB51D13AE, 0xA4A488DD), U64(0x6BABAB63, 0x98BDBE41), /* ~= 10^73 */
    U64(0xE264589A, 0x4DCDAB14), U64(0xC696963C, 0x7EED2DD1), /* ~= 10^74 */
    U64(0x8D7EB760, 0x70A08AEC), U64(0xFC1E1DE5, 0xCF543CA2), /* ~= 10^75 */
    U64(0xB0DE6538, 0x8CC8ADA8), U64(0x3B25A55F, 0x43294BCB), /* ~= 10^76 */
    U64(0xDD15FE86, 0xAFFAD912), U64(0x49EF0EB7, 0x13F39EBE), /* ~= 10^77 */
    U64(0x8A2DBF14, 0x2DFCC7AB), U64(0x6E356932, 0x6C784337), /* ~= 10^78 */
    U64(0xACB92ED9, 0x397BF996), U64(0x49C2C37F, 0x07965404), /* ~= 10^79 */
    U64(0xD7E77A8F, 0x87DAF7FB), U64(0xDC33745E, 0xC97BE906), /* ~= 10^80 */
    U64(0x86F0AC99, 0xB4E8DAFD), U64(0x69A028BB, 0x3DED71A3), /* ~= 10^81 */
    U64(0xA8ACD7C0, 0x222311BC), U64(0xC40832EA, 0x0D68CE0C), /* ~= 10^82 */
    U64(0xD2D80DB0, 0x2AABD62B), U64(0xF50A3FA4, 0x90C30190), /* ~= 10^83 */
    U64(0x83C7088E, 0x1AAB65DB), U64(0x792667C6, 0xDA79E0FA), /* ~= 10^84 */
    U64(0xA4B8CAB1, 0xA1563F52), U64(0x577001B8, 0x91185938), /* ~= 10^85 */
    U64(0xCDE6FD5E, 0x09ABCF26), U64(0xED4C0226, 0xB55E6F86), /* ~= 10^86 */
    U64(0x80B05E5A, 0xC60B6178), U64(0x544F8158, 0x315B05B4), /* ~= 10^87 */
    U64(0xA0DC75F1, 0x778E39D6), U64(0x696361AE, 0x3DB1C721), /* ~= 10^88 */
    U64(0xC913936D, 0xD571C84C), U64(0x03BC3A19, 0xCD1E38E9), /* ~= 10^89 */
    U64(0xFB587849, 0x4ACE3A5F), U64(0x04AB48A0, 0x4065C723), /* ~= 10^90 */
    U64(0x9D174B2D, 0xCEC0E47B), U64(0x62EB0D64, 0x283F9C76), /* ~= 10^91 */
    U64(0xC45D1DF9, 0x42711D9A), U64(0x3BA5D0BD, 0x324F8394), /* ~= 10^92 */
    U64(0xF5746577, 0x930D6500), U64(0xCA8F44EC, 0x7EE36479), /* ~= 10^93 */
    U64(0x9968BF6A, 0xBBE85F20), U64(0x7E998B13, 0xCF4E1ECB), /* ~= 10^94 */
    U64(0xBFC2EF45, 0x6AE276E8), U64(0x9E3FEDD8, 0xC321A67E), /* ~= 10^95 */
    U64(0xEFB3AB16, 0xC59B14A2), U64(0xC5CFE94E, 0xF3EA101E), /* ~= 10^96 */
    U64(0x95D04AEE, 0x3B80ECE5), U64(0xBBA1F1D1, 0x58724A12), /* ~= 10^97 */
    U64(0xBB445DA9, 0xCA61281F), U64(0x2A8A6E45, 0xAE8EDC97), /* ~= 10^98 */
    U64(0xEA157514, 0x3CF97226), U64(0xF52D09D7, 0x1A3293BD), /* ~= 10^99 */
    U64(0x924D692C, 0xA61BE758), U64(0x593C2626, 0x705F9C56), /* ~= 10^100 */
    U64(0xB6E0C377, 0xCFA2E12E), U64(0x6F8B2FB0, 0x0C77836C), /* ~= 10^101 */
    U64(0xE498F455, 0xC38B997A), U64(0x0B6DFB9C, 0x0F956447), /* ~= 10^102 */
    U64(0x8EDF98B5, 0x9A373FEC), U64(0x4724BD41, 0x89BD5EAC), /* ~= 10^103 */
    U64(0xB2977EE3, 0x00C50FE7), U64(0x58EDEC91, 0xEC2CB657), /* ~= 10^104 */
    U64(0xDF3D5E9B, 0xC0F653E1), U64(0x2F2967B6, 0x6737E3ED), /* ~= 10^105 */
    U64(0x8B865B21, 0x5899F46C), U64(0xBD79E0D2, 0x0082EE74), /* ~= 10^106 */
    U64(0xAE67F1E9, 0xAEC07187), U64(0xECD85906, 0x80A3AA11), /* ~= 10^107 */
    U64(0xDA01EE64, 0x1A708DE9), U64(0xE80E6F48, 0x20CC9495), /* ~= 10^108 */
    U64(0x884134FE, 0x908658B2), U64(0x3109058D, 0x147FDCDD), /* ~= 10^109 */
    U64(0xAA51823E, 0x34A7EEDE), U64(0xBD4B46F0, 0x599FD415), /* ~= 10^110 */
    U64(0xD4E5E2CD, 0xC1D1EA96), U64(0x6C9E18AC, 0x7007C91A), /* ~= 10^111 */
    U64(0x850FADC0, 0x9923329E), U64(0x03E2CF6B, 0xC604DDB0), /* ~= 10^112 */
    U64(0xA6539930, 0xBF6BFF45), U64(0x84DB8346, 0xB786151C), /* ~= 10^113 */
    U64(0xCFE87F7C, 0xEF46FF16), U64(0xE6126418, 0x65679A63), /* ~= 10^114 */
    U64(0x81F14FAE, 0x158C5F6E), U64(0x4FCB7E8F, 0x3F60C07E), /* ~= 10^115 */
    U64(0xA26DA399, 0x9AEF7749), U64(0xE3BE5E33, 0x0F38F09D), /* ~= 10^116 */
    U64(0xCB090C80, 0x01AB551C), U64(0x5CADF5BF, 0xD3072CC5), /* ~= 10^117 */
    U64(0xFDCB4FA0, 0x02162A63), U64(0x73D9732F, 0xC7C8F7F6), /* ~= 10^118 */
    U64(0x9E9F11C4, 0x014DDA7E), U64(0x2867E7FD, 0xDCDD9AFA), /* ~= 10^119 */
    U64(0xC646D635, 0x01A1511D), U64(0xB281E1FD, 0x541501B8), /* ~= 10^120 */
    U64(0xF7D88BC2, 0x4209A565), U64(0x1F225A7C, 0xA91A4226), /* ~= 10^121 */
    U64(0x9AE75759, 0x6946075F), U64(0x3375788D, 0xE9B06958), /* ~= 10^122 */
    U64(0xC1A12D2F, 0xC3978937), U64(0x0052D6B1, 0x641C83AE), /* ~= 10^123 */
    U64(0xF209787B, 0xB47D6B84), U64(0xC0678C5D, 0xBD23A49A), /* ~= 10^124 */
    U64(0x9745EB4D, 0x50CE6332), U64(0xF840B7BA, 0x963646E0), /* ~= 10^125 */
    U64(0xBD176620, 0xA501FBFF), U64(0xB650E5A9, 0x3BC3D898), /* ~= 10^126 */
    U64(0xEC5D3FA8, 0xCE427AFF), U64(0xA3E51F13, 0x8AB4CEBE), /* ~= 10^127 */
    U64(0x93BA47C9, 0x80E98CDF), U64(0xC66F336C, 0x36B10137), /* ~= 10^128 */
    U64(0xB8A8D9BB, 0xE123F017), U64(0xB80B0047, 0x445D4184), /* ~= 10^129 */
    U64(0xE6D3102A, 0xD96CEC1D), U64(0xA60DC059, 0x157491E5), /* ~= 10^130 */
    U64(0x9043EA1A, 0xC7E41392), U64(0x87C89837, 0xAD68DB2F), /* ~= 10^131 */
    U64(0xB454E4A1, 0x79DD1877), U64(0x29BABE45, 0x98C311FB), /* ~= 10^132 */
    U64(0xE16A1DC9, 0xD8545E94), U64(0xF4296DD6, 0xFEF3D67A), /* ~= 10^133 */
    U64(0x8CE2529E, 0x2734BB1D), U64(0x1899E4A6, 0x5F58660C), /* ~= 10^134 */
    U64(0xB01AE745, 0xB101E9E4), U64(0x5EC05DCF, 0xF72E7F8F), /* ~= 10^135 */
    U64(0xDC21A117, 0x1D42645D), U64(0x76707543, 0xF4FA1F73), /* ~= 10^136 */
    U64(0x899504AE, 0x72497EBA), U64(0x6A06494A, 0x791C53A8), /* ~= 10^137 */
    U64(0xABFA45DA, 0x0EDBDE69), U64(0x0487DB9D, 0x17636892), /* ~= 10^138 */
    U64(0xD6F8D750, 0x9292D603), U64(0x45A9D284, 0x5D3C42B6), /* ~= 10^139 */
    U64(0x865B8692, 0x5B9BC5C2), U64(0x0B8A2392, 0xBA45A9B2), /* ~= 10^140 */
    U64(0xA7F26836, 0xF282B732), U64(0x8E6CAC77, 0x68D7141E), /* ~= 10^141 */
    U64(0xD1EF0244, 0xAF2364FF), U64(0x3207D795, 0x430CD926), /* ~= 10^142 */
    U64(0x8335616A, 0xED761F1F), U64(0x7F44E6BD, 0x49E807B8), /* ~= 10^143 */
    U64(0xA402B9C5, 0xA8D3A6E7), U64(0x5F16206C, 0x9C6209A6), /* ~= 10^144 */
    U64(0xCD036837, 0x130890A1), U64(0x36DBA887, 0xC37A8C0F), /* ~= 10^145 */
    U64(0x80222122, 0x6BE55A64), U64(0xC2494954, 0xDA2C9789), /* ~= 10^146 */
    U64(0xA02AA96B, 0x06DEB0FD), U64(0xF2DB9BAA, 0x10B7BD6C), /* ~= 10^147 */
    U64(0xC83553C5, 0xC8965D3D), U64(0x6F928294, 0x94E5ACC7), /* ~= 10^148 */
    U64(0xFA42A8B7, 0x3ABBF48C), U64(0xCB772339, 0xBA1F17F9), /* ~= 10^149 */
    U64(0x9C69A972, 0x84B578D7), U64(0xFF2A7604, 0x14536EFB), /* ~= 10^150 */
    U64(0xC38413CF, 0x25E2D70D), U64(0xFEF51385, 0x19684ABA), /* ~= 10^151 */
    U64(0xF46518C2, 0xEF5B8CD1), U64(0x7EB25866, 0x5FC25D69), /* ~= 10^152 */
    U64(0x98BF2F79, 0xD5993802), U64(0xEF2F773F, 0xFBD97A61), /* ~= 10^153 */
    U64(0xBEEEFB58, 0x4AFF8603), U64(0xAAFB550F, 0xFACFD8FA), /* ~= 10^154 */
    U64(0xEEAABA2E, 0x5DBF6784), U64(0x95BA2A53, 0xF983CF38), /* ~= 10^155 */
    U64(0x952AB45C, 0xFA97A0B2), U64(0xDD945A74, 0x7BF26183), /* ~= 10^156 */
    U64(0xBA756174, 0x393D88DF), U64(0x94F97111, 0x9AEEF9E4), /* ~= 10^157 */
    U64(0xE912B9D1, 0x478CEB17), U64(0x7A37CD56, 0x01AAB85D), /* ~= 10^158 */
    U64(0x91ABB422, 0xCCB812EE), U64(0xAC62E055, 0xC10AB33A), /* ~= 10^159 */
    U64(0xB616A12B, 0x7FE617AA), U64(0x577B986B, 0x314D6009), /* ~= 10^160 */
    U64(0xE39C4976, 0x5FDF9D94), U64(0xED5A7E85, 0xFDA0B80B), /* ~= 10^161 */
    U64(0x8E41ADE9, 0xFBEBC27D), U64(0x14588F13, 0xBE847307), /* ~= 10^162 */
    U64(0xB1D21964, 0x7AE6B31C), U64(0x596EB2D8, 0xAE258FC8), /* ~= 10^163 */
    U64(0xDE469FBD, 0x99A05FE3), U64(0x6FCA5F8E, 0xD9AEF3BB), /* ~= 10^164 */
    U64(0x8AEC23D6, 0x80043BEE), U64(0x25DE7BB9, 0x480D5854), /* ~= 10^165 */
    U64(0xADA72CCC, 0x20054AE9), U64(0xAF561AA7, 0x9A10AE6A), /* ~= 10^166 */
    U64(0xD910F7FF, 0x28069DA4), U64(0x1B2BA151, 0x8094DA04), /* ~= 10^167 */
    U64(0x87AA9AFF, 0x79042286), U64(0x90FB44D2, 0xF05D0842), /* ~= 10^168 */
    U64(0xA99541BF, 0x57452B28), U64(0x353A1607, 0xAC744A53), /* ~= 10^169 */
    U64(0xD3FA922F, 0x2D1675F2), U64(0x42889B89, 0x97915CE8), /* ~= 10^170 */
    U64(0x847C9B5D, 0x7C2E09B7), U64(0x69956135, 0xFEBADA11), /* ~= 10^171 */
    U64(0xA59BC234, 0xDB398C25), U64(0x43FAB983, 0x7E699095), /* ~= 10^172 */
    U64(0xCF02B2C2, 0x1207EF2E), U64(0x94F967E4, 0x5E03F4BB), /* ~= 10^173 */
    U64(0x8161AFB9, 0x4B44F57D), U64(0x1D1BE0EE, 0xBAC278F5), /* ~= 10^174 */
    U64(0xA1BA1BA7, 0x9E1632DC), U64(0x6462D92A, 0x69731732), /* ~= 10^175 */
    U64(0xCA28A291, 0x859BBF93), U64(0x7D7B8F75, 0x03CFDCFE), /* ~= 10^176 */
    U64(0xFCB2CB35, 0xE702AF78), U64(0x5CDA7352, 0x44C3D43E), /* ~= 10^177 */
    U64(0x9DEFBF01, 0xB061ADAB), U64(0x3A088813, 0x6AFA64A7), /* ~= 10^178 */
    U64(0xC56BAEC2, 0x1C7A1916), U64(0x088AAA18, 0x45B8FDD0), /* ~= 10^179 */
    U64(0xF6C69A72, 0xA3989F5B), U64(0x8AAD549E, 0x57273D45), /* ~= 10^180 */
    U64(0x9A3C2087, 0xA63F6399), U64(0x36AC54E2, 0xF678864B), /* ~= 10^181 */
    U64(0xC0CB28A9, 0x8FCF3C7F), U64(0x84576A1B, 0xB416A7DD), /* ~= 10^182 */
    U64(0xF0FDF2D3, 0xF3C30B9F), U64(0x656D44A2, 0xA11C51D5), /* ~= 10^183 */
    U64(0x969EB7C4, 0x7859E743), U64(0x9F644AE5, 0xA4B1B325), /* ~= 10^184 */
    U64(0xBC4665B5, 0x96706114), U64(0x873D5D9F, 0x0DDE1FEE), /* ~= 10^185 */
    U64(0xEB57FF22, 0xFC0C7959), U64(0xA90CB506, 0xD155A7EA), /* ~= 10^186 */
    U64(0x9316FF75, 0xDD87CBD8), U64(0x09A7F124, 0x42D588F2), /* ~= 10^187 */
    U64(0xB7DCBF53, 0x54E9BECE), U64(0x0C11ED6D, 0x538AEB2F), /* ~= 10^188 */
    U64(0xE5D3EF28, 0x2A242E81), U64(0x8F1668C8, 0xA86DA5FA), /* ~= 10^189 */
    U64(0x8FA47579, 0x1A569D10), U64(0xF96E017D, 0x694487BC), /* ~= 10^190 */
    U64(0xB38D92D7, 0x60EC4455), U64(0x37C981DC, 0xC395A9AC), /* ~= 10^191 */
    U64(0xE070F78D, 0x3927556A), U64(0x85BBE253, 0xF47B1417), /* ~= 10^192 */
    U64(0x8C469AB8, 0x43B89562), U64(0x93956D74, 0x78CCEC8E), /* ~= 10^193 */
    U64(0xAF584166, 0x54A6BABB), U64(0x387AC8D1, 0x970027B2), /* ~= 10^194 */
    U64(0xDB2E51BF, 0xE9D0696A), U64(0x06997B05, 0xFCC0319E), /* ~= 10^195 */
    U64(0x88FCF317, 0xF22241E2), U64(0x441FECE3, 0xBDF81F03), /* ~= 10^196 */
    U64(0xAB3C2FDD, 0xEEAAD25A), U64(0xD527E81C, 0xAD7626C3), /* ~= 10^197 */
    U64(0xD60B3BD5, 0x6A5586F1), U64(0x8A71E223, 0xD8D3B074), /* ~= 10^198 */
    U64(0x85C70565, 0x62757456), U64(0xF6872D56, 0x67844E49), /* ~= 10^199 */
    U64(0xA738C6BE, 0xBB12D16C), U64(0xB428F8AC, 0x016561DB), /* ~= 10^200 */
    U64(0xD106F86E, 0x69D785C7), U64(0xE13336D7, 0x01BEBA52), /* ~= 10^201 */
    U64(0x82A45B45, 0x0226B39C), U64(0xECC00246, 0x61173473), /* ~= 10^202 */
    U64(0xA34D7216, 0x42B06084), U64(0x27F002D7, 0xF95D0190), /* ~= 10^203 */
    U64(0xCC20CE9B, 0xD35C78A5), U64(0x31EC038D, 0xF7B441F4), /* ~= 10^204 */
    U64(0xFF290242, 0xC83396CE), U64(0x7E670471, 0x75A15271), /* ~= 10^205 */
    U64(0x9F79A169, 0xBD203E41), U64(0x0F0062C6, 0xE984D386), /* ~= 10^206 */
    U64(0xC75809C4, 0x2C684DD1), U64(0x52C07B78, 0xA3E60868), /* ~= 10^207 */
    U64(0xF92E0C35, 0x37826145), U64(0xA7709A56, 0xCCDF8A82), /* ~= 10^208 */
    U64(0x9BBCC7A1, 0x42B17CCB), U64(0x88A66076, 0x400BB691), /* ~= 10^209 */
    U64(0xC2ABF989, 0x935DDBFE), U64(0x6ACFF893, 0xD00EA435), /* ~= 10^210 */
    U64(0xF356F7EB, 0xF83552FE), U64(0x0583F6B8, 0xC4124D43), /* ~= 10^211 */
    U64(0x98165AF3, 0x7B2153DE), U64(0xC3727A33, 0x7A8B704A), /* ~= 10^212 */
    U64(0xBE1BF1B0, 0x59E9A8D6), U64(0x744F18C0, 0x592E4C5C), /* ~= 10^213 */
    U64(0xEDA2EE1C, 0x7064130C), U64(0x1162DEF0, 0x6F79DF73), /* ~= 10^214 */
    U64(0x9485D4D1, 0xC63E8BE7), U64(0x8ADDCB56, 0x45AC2BA8), /* ~= 10^215 */
    U64(0xB9A74A06, 0x37CE2EE1), U64(0x6D953E2B, 0xD7173692), /* ~= 10^216 */
    U64(0xE8111C87, 0xC5C1BA99), U64(0xC8FA8DB6, 0xCCDD0437), /* ~= 10^217 */
    U64(0x910AB1D4, 0xDB9914A0), U64(0x1D9C9892, 0x400A22A2), /* ~= 10^218 */
    U64(0xB54D5E4A, 0x127F59C8), U64(0x2503BEB6, 0xD00CAB4B), /* ~= 10^219 */
    U64(0xE2A0B5DC, 0x971F303A), U64(0x2E44AE64, 0x840FD61D), /* ~= 10^220 */
    U64(0x8DA471A9, 0xDE737E24), U64(0x5CEAECFE, 0xD289E5D2), /* ~= 10^221 */
    U64(0xB10D8E14, 0x56105DAD), U64(0x7425A83E, 0x872C5F47), /* ~= 10^222 */
    U64(0xDD50F199, 0x6B947518), U64(0xD12F124E, 0x28F77719), /* ~= 10^223 */
    U64(0x8A5296FF, 0xE33CC92F), U64(0x82BD6B70, 0xD99AAA6F), /* ~= 10^224 */
    U64(0xACE73CBF, 0xDC0BFB7B), U64(0x636CC64D, 0x1001550B), /* ~= 10^225 */
    U64(0xD8210BEF, 0xD30EFA5A), U64(0x3C47F7E0, 0x5401AA4E), /* ~= 10^226 */
    U64(0x8714A775, 0xE3E95C78), U64(0x65ACFAEC, 0x34810A71), /* ~= 10^227 */
    U64(0xA8D9D153, 0x5CE3B396), U64(0x7F1839A7, 0x41A14D0D), /* ~= 10^228 */
    U64(0xD31045A8, 0x341CA07C), U64(0x1EDE4811, 0x1209A050), /* ~= 10^229 */
    U64(0x83EA2B89, 0x2091E44D), U64(0x934AED0A, 0xAB460432), /* ~= 10^230 */
    U64(0xA4E4B66B, 0x68B65D60), U64(0xF81DA84D, 0x5617853F), /* ~= 10^231 */
    U64(0xCE1DE406, 0x42E3F4B9), U64(0x36251260, 0xAB9D668E), /* ~= 10^232 */
    U64(0x80D2AE83, 0xE9CE78F3), U64(0xC1D72B7C, 0x6B426019), /* ~= 10^233 */
    U64(0xA1075A24, 0xE4421730), U64(0xB24CF65B, 0x8612F81F), /* ~= 10^234 */
    U64(0xC94930AE, 0x1D529CFC), U64(0xDEE033F2, 0x6797B627), /* ~= 10^235 */
    U64(0xFB9B7CD9, 0xA4A7443C), U64(0x169840EF, 0x017DA3B1), /* ~= 10^236 */
    U64(0x9D412E08, 0x06E88AA5), U64(0x8E1F2895, 0x60EE864E), /* ~= 10^237 */
    U64(0xC491798A, 0x08A2AD4E), U64(0xF1A6F2BA, 0xB92A27E2), /* ~= 10^238 */
    U64(0xF5B5D7EC, 0x8ACB58A2), U64(0xAE10AF69, 0x6774B1DB), /* ~= 10^239 */
    U64(0x9991A6F3, 0xD6BF1765), U64(0xACCA6DA1, 0xE0A8EF29), /* ~= 10^240 */
    U64(0xBFF610B0, 0xCC6EDD3F), U64(0x17FD090A, 0x58D32AF3), /* ~= 10^241 */
    U64(0xEFF394DC, 0xFF8A948E), U64(0xDDFC4B4C, 0xEF07F5B0), /* ~= 10^242 */
    U64(0x95F83D0A, 0x1FB69CD9), U64(0x4ABDAF10, 0x1564F98E), /* ~= 10^243 */
    U64(0xBB764C4C, 0xA7A4440F), U64(0x9D6D1AD4, 0x1ABE37F1), /* ~= 10^244 */
    U64(0xEA53DF5F, 0xD18D5513), U64(0x84C86189, 0x216DC5ED), /* ~= 10^245 */
    U64(0x92746B9B, 0xE2F8552C), U64(0x32FD3CF5, 0xB4E49BB4), /* ~= 10^246 */
    U64(0xB7118682, 0xDBB66A77), U64(0x3FBC8C33, 0x221DC2A1), /* ~= 10^247 */
    U64(0xE4D5E823, 0x92A40515), U64(0x0FABAF3F, 0xEAA5334A), /* ~= 10^248 */
    U64(0x8F05B116, 0x3BA6832D), U64(0x29CB4D87, 0xF2A7400E), /* ~= 10^249 */
    U64(0xB2C71D5B, 0xCA9023F8), U64(0x743E20E9, 0xEF511012), /* ~= 10^250 */
    U64(0xDF78E4B2, 0xBD342CF6), U64(0x914DA924, 0x6B255416), /* ~= 10^251 */
    U64(0x8BAB8EEF, 0xB6409C1A), U64(0x1AD089B6, 0xC2F7548E), /* ~= 10^252 */
    U64(0xAE9672AB, 0xA3D0C320), U64(0xA184AC24, 0x73B529B1), /* ~= 10^253 */
    U64(0xDA3C0F56, 0x8CC4F3E8), U64(0xC9E5D72D, 0x90A2741E), /* ~= 10^254 */
    U64(0x88658996, 0x17FB1871), U64(0x7E2FA67C, 0x7A658892), /* ~= 10^255 */
    U64(0xAA7EEBFB, 0x9DF9DE8D), U64(0xDDBB901B, 0x98FEEAB7), /* ~= 10^256 */
    U64(0xD51EA6FA, 0x85785631), U64(0x552A7422, 0x7F3EA565), /* ~= 10^257 */
    U64(0x8533285C, 0x936B35DE), U64(0xD53A8895, 0x8F87275F), /* ~= 10^258 */
    U64(0xA67FF273, 0xB8460356), U64(0x8A892ABA, 0xF368F137), /* ~= 10^259 */
    U64(0xD01FEF10, 0xA657842C), U64(0x2D2B7569, 0xB0432D85), /* ~= 10^260 */
    U64(0x8213F56A, 0x67F6B29B), U64(0x9C3B2962, 0x0E29FC73), /* ~= 10^261 */
    U64(0xA298F2C5, 0x01F45F42), U64(0x8349F3BA, 0x91B47B8F), /* ~= 10^262 */
    U64(0xCB3F2F76, 0x42717713), U64(0x241C70A9, 0x36219A73), /* ~= 10^263 */
    U64(0xFE0EFB53, 0xD30DD4D7), U64(0xED238CD3, 0x83AA0110), /* ~= 10^264 */
    U64(0x9EC95D14, 0x63E8A506), U64(0xF4363804, 0x324A40AA), /* ~= 10^265 */
    U64(0xC67BB459, 0x7CE2CE48), U64(0xB143C605, 0x3EDCD0D5), /* ~= 10^266 */
    U64(0xF81AA16F, 0xDC1B81DA), U64(0xDD94B786, 0x8E94050A), /* ~= 10^267 */
    U64(0x9B10A4E5, 0xE9913128), U64(0xCA7CF2B4, 0x191C8326), /* ~= 10^268 */
    U64(0xC1D4CE1F, 0x63F57D72), U64(0xFD1C2F61, 0x1F63A3F0), /* ~= 10^269 */
    U64(0xF24A01A7, 0x3CF2DCCF), U64(0xBC633B39, 0x673C8CEC), /* ~= 10^270 */
    U64(0x976E4108, 0x8617CA01), U64(0xD5BE0503, 0xE085D813), /* ~= 10^271 */
    U64(0xBD49D14A, 0xA79DBC82), U64(0x4B2D8644, 0xD8A74E18), /* ~= 10^272 */
    U64(0xEC9C459D, 0x51852BA2), U64(0xDDF8E7D6, 0x0ED1219E), /* ~= 10^273 */
    U64(0x93E1AB82, 0x52F33B45), U64(0xCABB90E5, 0xC942B503), /* ~= 10^274 */
    U64(0xB8DA1662, 0xE7B00A17), U64(0x3D6A751F, 0x3B936243), /* ~= 10^275 */
    U64(0xE7109BFB, 0xA19C0C9D), U64(0x0CC51267, 0x0A783AD4), /* ~= 10^276 */
    U64(0x906A617D, 0x450187E2), U64(0x27FB2B80, 0x668B24C5), /* ~= 10^277 */
    U64(0xB484F9DC, 0x9641E9DA), U64(0xB1F9F660, 0x802DEDF6), /* ~= 10^278 */
    U64(0xE1A63853, 0xBBD26451), U64(0x5E7873F8, 0xA0396973), /* ~= 10^279 */
    U64(0x8D07E334, 0x55637EB2), U64(0xDB0B487B, 0x6423E1E8), /* ~= 10^280 */
    U64(0xB049DC01, 0x6ABC5E5F), U64(0x91CE1A9A, 0x3D2CDA62), /* ~= 10^281 */
    U64(0xDC5C5301, 0xC56B75F7), U64(0x7641A140, 0xCC7810FB), /* ~= 10^282 */
    U64(0x89B9B3E1, 0x1B6329BA), U64(0xA9E904C8, 0x7FCB0A9D), /* ~= 10^283 */
    U64(0xAC2820D9, 0x623BF429), U64(0x546345FA, 0x9FBDCD44), /* ~= 10^284 */
    U64(0xD732290F, 0xBACAF133), U64(0xA97C1779, 0x47AD4095), /* ~= 10^285 */
    U64(0x867F59A9, 0xD4BED6C0), U64(0x49ED8EAB, 0xCCCC485D), /* ~= 10^286 */
    U64(0xA81F3014, 0x49EE8C70), U64(0x5C68F256, 0xBFFF5A74), /* ~= 10^287 */
    U64(0xD226FC19, 0x5C6A2F8C), U64(0x73832EEC, 0x6FFF3111), /* ~= 10^288 */
    U64(0x83585D8F, 0xD9C25DB7), U64(0xC831FD53, 0xC5FF7EAB), /* ~= 10^289 */
    U64(0xA42E74F3, 0xD032F525), U64(0xBA3E7CA8, 0xB77F5E55), /* ~= 10^290 */
    U64(0xCD3A1230, 0xC43FB26F), U64(0x28CE1BD2, 0xE55F35EB), /* ~= 10^291 */
    U64(0x80444B5E, 0x7AA7CF85), U64(0x7980D163, 0xCF5B81B3), /* ~= 10^292 */
    U64(0xA0555E36, 0x1951C366), U64(0xD7E105BC, 0xC332621F), /* ~= 10^293 */
    U64(0xC86AB5C3, 0x9FA63440), U64(0x8DD9472B, 0xF3FEFAA7), /* ~= 10^294 */
    U64(0xFA856334, 0x878FC150), U64(0xB14F98F6, 0xF0FEB951), /* ~= 10^295 */
    U64(0x9C935E00, 0xD4B9D8D2), U64(0x6ED1BF9A, 0x569F33D3), /* ~= 10^296 */
    U64(0xC3B83581, 0x09E84F07), U64(0x0A862F80, 0xEC4700C8), /* ~= 10^297 */
    U64(0xF4A642E1, 0x4C6262C8), U64(0xCD27BB61, 0x2758C0FA), /* ~= 10^298 */
    U64(0x98E7E9CC, 0xCFBD7DBD), U64(0x8038D51C, 0xB897789C), /* ~= 10^299 */
    U64(0xBF21E440, 0x03ACDD2C), U64(0xE0470A63, 0xE6BD56C3), /* ~= 10^300 */
    U64(0xEEEA5D50, 0x04981478), U64(0x1858CCFC, 0xE06CAC74), /* ~= 10^301 */
    U64(0x95527A52, 0x02DF0CCB), U64(0x0F37801E, 0x0C43EBC8), /* ~= 10^302 */
    U64(0xBAA718E6, 0x8396CFFD), U64(0xD3056025, 0x8F54E6BA), /* ~= 10^303 */
    U64(0xE950DF20, 0x247C83FD), U64(0x47C6B82E, 0xF32A2069), /* ~= 10^304 */
    U64(0x91D28B74, 0x16CDD27E), U64(0x4CDC331D, 0x57FA5441), /* ~= 10^305 */
    U64(0xB6472E51, 0x1C81471D), U64(0xE0133FE4, 0xADF8E952), /* ~= 10^306 */
    U64(0xE3D8F9E5, 0x63A198E5), U64(0x58180FDD, 0xD97723A6), /* ~= 10^307 */
    U64(0x8E679C2F, 0x5E44FF8F), U64(0x570F09EA, 0xA7EA7648), /* ~= 10^308 */
    U64(0xB201833B, 0x35D63F73), U64(0x2CD2CC65, 0x51E513DA), /* ~= 10^309 */
    U64(0xDE81E40A, 0x034BCF4F), U64(0xF8077F7E, 0xA65E58D1), /* ~= 10^310 */
    U64(0x8B112E86, 0x420F6191), U64(0xFB04AFAF, 0x27FAF782), /* ~= 10^311 */
    U64(0xADD57A27, 0xD29339F6), U64(0x79C5DB9A, 0xF1F9B563), /* ~= 10^312 */
    U64(0xD94AD8B1, 0xC7380874), U64(0x18375281, 0xAE7822BC), /* ~= 10^313 */
    U64(0x87CEC76F, 0x1C830548), U64(0x8F229391, 0x0D0B15B5), /* ~= 10^314 */
    U64(0xA9C2794A, 0xE3A3C69A), U64(0xB2EB3875, 0x504DDB22), /* ~= 10^315 */
    U64(0xD433179D, 0x9C8CB841), U64(0x5FA60692, 0xA46151EB), /* ~= 10^316 */
    U64(0x849FEEC2, 0x81D7F328), U64(0xDBC7C41B, 0xA6BCD333), /* ~= 10^317 */
    U64(0xA5C7EA73, 0x224DEFF3), U64(0x12B9B522, 0x906C0800), /* ~= 10^318 */
    U64(0xCF39E50F, 0xEAE16BEF), U64(0xD768226B, 0x34870A00), /* ~= 10^319 */
    U64(0x81842F29, 0xF2CCE375), U64(0xE6A11583, 0x00D46640), /* ~= 10^320 */
    U64(0xA1E53AF4, 0x6F801C53), U64(0x60495AE3, 0xC1097FD0), /* ~= 10^321 */
    U64(0xCA5E89B1, 0x8B602368), U64(0x385BB19C, 0xB14BDFC4), /* ~= 10^322 */
    U64(0xFCF62C1D, 0xEE382C42), U64(0x46729E03, 0xDD9ED7B5), /* ~= 10^323 */
    U64(0x9E19DB92, 0xB4E31BA9), U64(0x6C07A2C2, 0x6A8346D1)  /* ~= 10^324 */
};

/**
 Get the cached pow10 value from `pow10_sig_table`.
 @param exp10 The exponent of pow(10, e). This value must in range
              `POW10_SIG_TABLE_MIN_EXP` to `POW10_SIG_TABLE_MAX_EXP`.
 @param hi    The highest 64 bits of pow(10, e).
 @param lo    The lower 64 bits after `hi`.
 */
static_inline void pow10_table_get_sig(i32 exp10, u64 *hi, u64 *lo) {
    i32 idx = exp10 - (POW10_SIG_TABLE_MIN_EXP);
    *hi = pow10_sig_table[idx * 2];
    *lo = pow10_sig_table[idx * 2 + 1];
}

/**
 Get the exponent (base 2) for highest 64 bits significand in `pow10_sig_table`.
 */
static_inline void pow10_table_get_exp(i32 exp10, i32 *exp2) {
    /* e2 = floor(log2(pow(10, e))) - 64 + 1 */
    /*    = floor(e * log2(10) - 63)         */
    *exp2 = (exp10 * 217706 - 4128768) >> 16;
}

#endif



/*==============================================================================
 * MARK: - Number and Bit Utils (Private)
 *============================================================================*/

/** Convert bits to double. */
static_inline f64 f64_from_bits(u64 u) {
    f64 f;
    memcpy(&f, &u, sizeof(u));
    return f;
}

/** Convert double to bits. */
static_inline u64 f64_to_bits(f64 f) {
    u64 u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

/** Convert double to bits. */
static_inline u32 f32_to_bits(f32 f) {
    u32 u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

/** Get 'infinity' bits with sign. */
static_inline u64 f64_bits_inf(bool sign) {
#if CTOON_HAS_IEEE_754
    return F64_BITS_INF | ((u64)sign << 63);
#elif defined(INFINITY)
    return f64_to_bits(sign ? -INFINITY : INFINITY);
#else
    return f64_to_bits(sign ? -HUGE_VAL : HUGE_VAL);
#endif
}

/** Get 'nan' bits with sign. */
static_inline u64 f64_bits_nan(bool sign) {
#if CTOON_HAS_IEEE_754
    return F64_BITS_NAN | ((u64)sign << 63);
#elif defined(NAN)
    return f64_to_bits(sign ? (f64)-NAN : (f64)NAN);
#else
    return f64_to_bits((sign ? -0.0 : 0.0) / 0.0);
#endif
}

/** Casting double to float, allow overflow. */
#if ctoon_has_attribute(no_sanitize)
__attribute__((no_sanitize("undefined")))
#elif ctoon_gcc_available(4, 9, 0)
__attribute__((__no_sanitize_undefined__))
#endif
static_inline f32 f64_to_f32(f64 val) {
    return (f32)val;
}

/** Returns the number of leading 0-bits in value (input should not be 0). */
static_inline u32 u64_lz_bits(u64 v) {
#if GCC_HAS_CLZLL
    return (u32)__builtin_clzll(v);
#elif MSC_HAS_BIT_SCAN_64
    unsigned long r;
    _BitScanReverse64(&r, v);
    return (u32)63 - (u32)r;
#elif MSC_HAS_BIT_SCAN
    unsigned long hi, lo;
    bool hi_set = _BitScanReverse(&hi, (u32)(v >> 32)) != 0;
    _BitScanReverse(&lo, (u32)v);
    hi |= 32;
    return (u32)63 - (u32)(hi_set ? hi : lo);
#else
    /* branchless, use De Bruijn sequence */
    /* see: https://www.chessprogramming.org/BitScan */
    const u8 table[64] = {
        63, 16, 62,  7, 15, 36, 61,  3,  6, 14, 22, 26, 35, 47, 60,  2,
         9,  5, 28, 11, 13, 21, 42, 19, 25, 31, 34, 40, 46, 52, 59,  1,
        17,  8, 37,  4, 23, 27, 48, 10, 29, 12, 43, 20, 32, 41, 53, 18,
        38, 24, 49, 30, 44, 33, 54, 39, 50, 45, 55, 51, 56, 57, 58,  0
    };
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    return table[(v * U64(0x03F79D71, 0xB4CB0A89)) >> 58];
#endif
}

/** Returns the number of trailing 0-bits in value (input should not be 0). */
static_inline u32 u64_tz_bits(u64 v) {
#if GCC_HAS_CTZLL
    return (u32)__builtin_ctzll(v);
#elif MSC_HAS_BIT_SCAN_64
    unsigned long r;
    _BitScanForward64(&r, v);
    return (u32)r;
#elif MSC_HAS_BIT_SCAN
    unsigned long lo, hi;
    bool lo_set = _BitScanForward(&lo, (u32)(v)) != 0;
    _BitScanForward(&hi, (u32)(v >> 32));
    hi += 32;
    return lo_set ? lo : hi;
#else
    /* branchless, use De Bruijn sequence */
    /* see: https://www.chessprogramming.org/BitScan */
    const u8 table[64] = {
         0,  1,  2, 53,  3,  7, 54, 27,  4, 38, 41,  8, 34, 55, 48, 28,
        62,  5, 39, 46, 44, 42, 22,  9, 24, 35, 59, 56, 49, 18, 29, 11,
        63, 52,  6, 26, 37, 40, 33, 47, 61, 45, 43, 21, 23, 58, 17, 10,
        51, 25, 36, 32, 60, 20, 57, 16, 50, 31, 19, 15, 30, 14, 13, 12
    };
    return table[((v & (~v + 1)) * U64(0x022FDD63, 0xCC95386D)) >> 58];
#endif
}

/** Multiplies two 64-bit unsigned integers (a * b),
    returns the 128-bit result as 'hi' and 'lo'. */
static_inline void u128_mul(u64 a, u64 b, u64 *hi, u64 *lo) {
#if CTOON_HAS_INT128
    u128 m = (u128)a * b;
    *hi = (u64)(m >> 64);
    *lo = (u64)(m);
#elif MSC_HAS_UMUL128
    *lo = _umul128(a, b, hi);
#else
    u32 a0 = (u32)(a), a1 = (u32)(a >> 32);
    u32 b0 = (u32)(b), b1 = (u32)(b >> 32);
    u64 p00 = (u64)a0 * b0, p01 = (u64)a0 * b1;
    u64 p10 = (u64)a1 * b0, p11 = (u64)a1 * b1;
    u64 m0 = p01 + (p00 >> 32);
    u32 m00 = (u32)(m0), m01 = (u32)(m0 >> 32);
    u64 m1 = p10 + m00;
    u32 m10 = (u32)(m1), m11 = (u32)(m1 >> 32);
    *hi = p11 + m01 + m11;
    *lo = ((u64)m10 << 32) | (u32)p00;
#endif
}

/** Multiplies two 64-bit unsigned integers and add a value (a * b + c),
    returns the 128-bit result as 'hi' and 'lo'. */
static_inline void u128_mul_add(u64 a, u64 b, u64 c, u64 *hi, u64 *lo) {
#if CTOON_HAS_INT128
    u128 m = (u128)a * b + c;
    *hi = (u64)(m >> 64);
    *lo = (u64)(m);
#else
    u64 h, l, t;
    u128_mul(a, b, &h, &l);
    t = l + c;
    h += (u64)(((t < l) | (t < c)));
    *hi = h;
    *lo = t;
#endif
}



/*==============================================================================
 * MARK: - File Utils (Private)
 * These functions are used to read and write TOON files.
 *============================================================================*/

#define CTOON_FOPEN_E
#if !defined(_MSC_VER) && defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#   if __GLIBC_PREREQ(2, 7)
#       undef CTOON_FOPEN_E
#       define CTOON_FOPEN_E "e" /* glibc extension to enable O_CLOEXEC */
#   endif
#endif

static_inline FILE *fopen_safe(const char *path, const char *mode) {
#if CTOON_MSC_VER >= 1400
    FILE *file = NULL;
    if (fopen_s(&file, path, mode) != 0) return NULL;
    return file;
#else
    return fopen(path, mode);
#endif
}

static_inline FILE *fopen_readonly(const char *path) {
    return fopen_safe(path, "rb" CTOON_FOPEN_E);
}

static_inline FILE *fopen_writeonly(const char *path) {
    return fopen_safe(path, "wb" CTOON_FOPEN_E);
}

static_inline usize fread_safe(void *buf, usize size, FILE *file) {
#if CTOON_MSC_VER >= 1400
    return fread_s(buf, size, 1, size, file);
#else
    return fread(buf, 1, size, file);
#endif
}



/*==============================================================================
 * MARK: - Size Utils (Private)
 * These functions are used for memory allocation.
 *============================================================================*/

/** Returns whether the size is overflow after increment. */
static_inline bool size_add_is_overflow(usize size, usize add) {
    return size > (size + add);
}

/** Returns whether the size is power of 2 (size should not be 0). */
static_inline bool size_is_pow2(usize size) {
    return (size & (size - 1)) == 0;
}

/** Align size upwards (may overflow). */
static_inline usize size_align_up(usize size, usize align) {
    if (size_is_pow2(align)) {
        return (size + (align - 1)) & ~(align - 1);
    } else {
        return size + align - (size + align - 1) % align - 1;
    }
}

/** Align size downwards. */
static_inline usize size_align_down(usize size, usize align) {
    if (size_is_pow2(align)) {
        return size & ~(align - 1);
    } else {
        return size - (size % align);
    }
}

/** Align address upwards (may overflow). */
static_inline void *mem_align_up(void *mem, usize align) {
    usize size;
    memcpy(&size, &mem, sizeof(usize));
    size = size_align_up(size, align);
    memcpy(&mem, &size, sizeof(usize));
    return mem;
}



/*==============================================================================
 * MARK: - Default Memory Allocator (Private)
 * This is a simple libc memory allocator wrapper.
 *============================================================================*/

static void *default_malloc(void *ctx, usize size) {
    if (ctx) return NULL; /* default allocator requires ctx == NULL */
    return malloc(size);
}

static void *default_realloc(void *ctx, void *ptr, usize old_size, usize size) {
    if (ctx || old_size >= size) {} /* default allocator: ctx must be NULL */
    return realloc(ptr, size);
}

static void default_free(void *ctx, void *ptr) {
    if (ctx) return; /* default allocator: ctx must be NULL */
    free(ptr);
}

static const ctoon_alc CTOON_DEFAULT_ALC = {
    default_malloc, default_realloc, default_free, NULL
};



/*==============================================================================
 * MARK: - Null Memory Allocator (Private)
 * This allocator is just a placeholder to ensure that the internal
 * malloc/realloc/free function pointers are not null.
 *============================================================================*/

static void *null_malloc(void *ctx, usize size) {
    if (ctx || size) return NULL;
    return NULL;
}

static void *null_realloc(void *ctx, void *ptr, usize old_size, usize size) {
    if (ctx || ptr || old_size || size) return NULL;
    return NULL;
}

static void null_free(void *ctx, void *ptr) {
    if (ctx || ptr) return;
}

static const ctoon_alc CTOON_NULL_ALC = {
    null_malloc, null_realloc, null_free, NULL
};



/*==============================================================================
 * MARK: - Pool Memory Allocator (Public)
 * This allocator is initialized with a fixed-size buffer.
 * The buffer is split into multiple memory chunks for memory allocation.
 *============================================================================*/

/** memory chunk header */
typedef struct pool_chunk {
    usize size; /* chunk memory size, include chunk header */
    struct pool_chunk *next; /* linked list, nullable */
    /* char mem[]; flexible array member */
} pool_chunk;

/** allocator ctx header */
typedef struct pool_ctx {
    usize size; /* total memory size, include ctx header */
    pool_chunk *free_list; /* linked list, nullable */
    /* pool_chunk chunks[]; flexible array member */
} pool_ctx;

/** align up the input size to chunk size */
static_inline void pool_size_align(usize *size) {
    *size = size_align_up(*size, sizeof(pool_chunk)) + sizeof(pool_chunk);
}

static void *pool_malloc(void *ctx_ptr, usize size) {
    /* assert(size != 0) */
    pool_ctx *ctx = (pool_ctx *)ctx_ptr;
    pool_chunk *next, *prev = NULL, *cur = ctx->free_list;

    if (unlikely(size >= ctx->size)) return NULL;
    pool_size_align(&size);

    while (cur) {
        if (cur->size < size) {
            /* not enough space, try next chunk */
            prev = cur;
            cur = cur->next;
            continue;
        }
        if (cur->size >= size + sizeof(pool_chunk) * 2) {
            /* too much space, split this chunk */
            next = (pool_chunk *)(void *)((u8 *)cur + size);
            next->size = cur->size - size;
            next->next = cur->next;
            cur->size = size;
        } else {
            /* just enough space, use whole chunk */
            next = cur->next;
        }
        if (prev) prev->next = next;
        else ctx->free_list = next;
        return (void *)(cur + 1);
    }
    return NULL;
}

static void pool_free(void *ctx_ptr, void *ptr) {
    /* assert(ptr != NULL) */
    pool_ctx *ctx = (pool_ctx *)ctx_ptr;
    pool_chunk *cur = ((pool_chunk *)ptr) - 1;
    pool_chunk *prev = NULL, *next = ctx->free_list;

    while (next && next < cur) {
        prev = next;
        next = next->next;
    }
    if (prev) prev->next = cur;
    else ctx->free_list = cur;
    cur->next = next;

    if (next && ((u8 *)cur + cur->size) == (u8 *)next) {
        /* merge cur to higher chunk */
        cur->size += next->size;
        cur->next = next->next;
    }
    if (prev && ((u8 *)prev + prev->size) == (u8 *)cur) {
        /* merge cur to lower chunk */
        prev->size += cur->size;
        prev->next = cur->next;
    }
}

static void *pool_realloc(void *ctx_ptr, void *ptr,
                          usize old_size, usize size) {
    /* assert(ptr != NULL && size != 0 && old_size < size) */
    pool_ctx *ctx = (pool_ctx *)ctx_ptr;
    pool_chunk *cur = ((pool_chunk *)ptr) - 1, *prev, *next, *tmp;

    /* check size */
    if (unlikely(size >= ctx->size)) return NULL;
    pool_size_align(&old_size);
    pool_size_align(&size);
    if (unlikely(old_size == size)) return ptr;

    /* find next and prev chunk */
    prev = NULL;
    next = ctx->free_list;
    while (next && next < cur) {
        prev = next;
        next = next->next;
    }

    if ((u8 *)cur + cur->size == (u8 *)next && cur->size + next->size >= size) {
        /* merge to higher chunk if they are contiguous */
        usize free_size = cur->size + next->size - size;
        if (free_size > sizeof(pool_chunk) * 2) {
            tmp = (pool_chunk *)(void *)((u8 *)cur + size);
            if (prev) prev->next = tmp;
            else ctx->free_list = tmp;
            tmp->next = next->next;
            tmp->size = free_size;
            cur->size = size;
        } else {
            if (prev) prev->next = next->next;
            else ctx->free_list = next->next;
            cur->size += next->size;
        }
        return ptr;
    } else {
        /* fallback to malloc and memcpy */
        void *new_ptr = pool_malloc(ctx_ptr, size - sizeof(pool_chunk));
        if (new_ptr) {
            memcpy(new_ptr, ptr, cur->size - sizeof(pool_chunk));
            pool_free(ctx_ptr, ptr);
        }
        return new_ptr;
    }
}

bool ctoon_alc_pool_init(ctoon_alc *alc, void *buf, usize size) {
    pool_chunk *chunk;
    pool_ctx *ctx;

    if (unlikely(!alc)) return false;
    *alc = CTOON_NULL_ALC;
    if (size < sizeof(pool_ctx) * 4) return false;
    ctx = (pool_ctx *)mem_align_up(buf, sizeof(pool_ctx));
    if (unlikely(!ctx)) return false;
    size -= (usize)((u8 *)ctx - (u8 *)buf);
    size = size_align_down(size, sizeof(pool_ctx));

    chunk = (pool_chunk *)(ctx + 1);
    chunk->size = size - sizeof(pool_ctx);
    chunk->next = NULL;
    ctx->size = size;
    ctx->free_list = chunk;

    alc->malloc = pool_malloc;
    alc->realloc = pool_realloc;
    alc->free = pool_free;
    alc->ctx = (void *)ctx;
    return true;
}



/*==============================================================================
 * MARK: - Dynamic Memory Allocator (Public)
 * This allocator allocates memory on demand and does not immediately release
 * unused memory. Instead, it places the unused memory into a freelist for
 * potential reuse in the future. It is only when the entire allocator is
 * destroyed that all previously allocated memory is released at once.
 *============================================================================*/

/** memory chunk header */
typedef struct dyn_chunk {
    usize size; /* chunk size, include header */
    struct dyn_chunk *next;
    /* char mem[]; flexible array member */
} dyn_chunk;

/** allocator ctx header */
typedef struct {
    dyn_chunk free_list; /* dummy header, sorted from small to large */
    dyn_chunk used_list; /* dummy header */
} dyn_ctx;

/** align up the input size to chunk size */
static_inline bool dyn_size_align(usize *size) {
    usize alc_size = *size + sizeof(dyn_chunk);
    alc_size = size_align_up(alc_size, CTOON_ALC_DYN_MIN_SIZE);
    if (unlikely(alc_size < *size)) return false; /* overflow */
    *size = alc_size;
    return true;
}

/** remove a chunk from list (the chunk must already be in the list) */
static_inline void dyn_chunk_list_remove(dyn_chunk *list, dyn_chunk *chunk) {
    dyn_chunk *prev = list, *cur;
    for (cur = prev->next; cur; cur = cur->next) {
        if (cur == chunk) {
            prev->next = cur->next;
            cur->next = NULL;
            return;
        }
        prev = cur;
    }
}

/** add a chunk to list header (the chunk must not be in the list) */
static_inline void dyn_chunk_list_add(dyn_chunk *list, dyn_chunk *chunk) {
    chunk->next = list->next;
    list->next = chunk;
}

static void *dyn_malloc(void *ctx_ptr, usize size) {
    /* assert(size != 0) */
    const ctoon_alc def = CTOON_DEFAULT_ALC;
    dyn_ctx *ctx = (dyn_ctx *)ctx_ptr;
    dyn_chunk *chunk, *prev;
    if (unlikely(!dyn_size_align(&size))) return NULL;

    /* freelist is empty, create new chunk */
    if (!ctx->free_list.next) {
        chunk = (dyn_chunk *)def.malloc(def.ctx, size);
        if (unlikely(!chunk)) return NULL;
        chunk->size = size;
        chunk->next = NULL;
        dyn_chunk_list_add(&ctx->used_list, chunk);
        return (void *)(chunk + 1);
    }

    /* find a large enough chunk, or resize the largest chunk */
    prev = &ctx->free_list;
    while (true) {
        chunk = prev->next;
        if (chunk->size >= size) { /* enough size, reuse this chunk */
            prev->next = chunk->next;
            dyn_chunk_list_add(&ctx->used_list, chunk);
            return (void *)(chunk + 1);
        }
        if (!chunk->next) { /* resize the largest chunk */
            chunk = (dyn_chunk *)def.realloc(def.ctx, chunk, chunk->size, size);
            if (unlikely(!chunk)) return NULL;
            prev->next = NULL;
            chunk->size = size;
            dyn_chunk_list_add(&ctx->used_list, chunk);
            return (void *)(chunk + 1);
        }
        prev = chunk;
    }
}

static void *dyn_realloc(void *ctx_ptr, void *ptr,
                          usize old_size, usize size) {
    /* assert(ptr != NULL && size != 0 && old_size < size) */
    if (old_size >= size) return ptr; /* already large enough */
    const ctoon_alc def = CTOON_DEFAULT_ALC;
    dyn_ctx *ctx = (dyn_ctx *)ctx_ptr;
    dyn_chunk *new_chunk, *chunk = (dyn_chunk *)ptr - 1;
    if (unlikely(!dyn_size_align(&size))) return NULL;
    if (chunk->size >= size) return ptr;

    dyn_chunk_list_remove(&ctx->used_list, chunk);
    new_chunk = (dyn_chunk *)def.realloc(def.ctx, chunk, chunk->size, size);
    if (likely(new_chunk)) {
        new_chunk->size = size;
        chunk = new_chunk;
    }
    dyn_chunk_list_add(&ctx->used_list, chunk);
    return new_chunk ? (void *)(new_chunk + 1) : NULL;
}

static void dyn_free(void *ctx_ptr, void *ptr) {
    /* assert(ptr != NULL) */
    dyn_ctx *ctx = (dyn_ctx *)ctx_ptr;
    dyn_chunk *chunk = (dyn_chunk *)ptr - 1, *prev;

    dyn_chunk_list_remove(&ctx->used_list, chunk);
    for (prev = &ctx->free_list; prev; prev = prev->next) {
        if (!prev->next || prev->next->size >= chunk->size) {
            chunk->next = prev->next;
            prev->next = chunk;
            break;
        }
    }
}

ctoon_alc *ctoon_alc_dyn_new(void) {
    const ctoon_alc def = CTOON_DEFAULT_ALC;
    usize hdr_len = sizeof(ctoon_alc) + sizeof(dyn_ctx);
    ctoon_alc *alc = (ctoon_alc *)def.malloc(def.ctx, hdr_len);
    dyn_ctx *ctx = (dyn_ctx *)(void *)(alc + 1);
    if (unlikely(!alc)) return NULL;
    alc->malloc = dyn_malloc;
    alc->realloc = dyn_realloc;
    alc->free = dyn_free;
    alc->ctx = alc + 1;
    memset(ctx, 0, sizeof(*ctx));
    return alc;
}

void ctoon_alc_dyn_free(ctoon_alc *alc) {
    const ctoon_alc def = CTOON_DEFAULT_ALC;
    dyn_ctx *ctx = (dyn_ctx *)(void *)(alc + 1);
    dyn_chunk *chunk, *next;
    if (unlikely(!alc)) return;
    for (chunk = ctx->free_list.next; chunk; chunk = next) {
        next = chunk->next;
        def.free(def.ctx, chunk);
    }
    for (chunk = ctx->used_list.next; chunk; chunk = next) {
        next = chunk->next;
        def.free(def.ctx, chunk);
    }
    def.free(def.ctx, alc);
}



/*==============================================================================
 * MARK: - TOON Struct Utils (Public)
 * These functions are used for creating, copying, releasing, and comparing
 * TOON documents and values. They are widely used throughout this library.
 *============================================================================*/

static_inline void unsafe_ctoon_str_pool_release(ctoon_str_pool *pool,
                                                  ctoon_alc *alc) {
    ctoon_str_chunk *chunk = pool->chunks, *next;
    while (chunk) {
        next = chunk->next;
        alc->free(alc->ctx, chunk);
        chunk = next;
    }
}

static_inline void unsafe_ctoon_val_pool_release(ctoon_val_pool *pool,
                                                  ctoon_alc *alc) {
    ctoon_val_chunk *chunk = pool->chunks, *next;
    while (chunk) {
        next = chunk->next;
        alc->free(alc->ctx, chunk);
        chunk = next;
    }
}

bool unsafe_ctoon_str_pool_grow(ctoon_str_pool *pool,
                                 const ctoon_alc *alc, usize len) {
    ctoon_str_chunk *chunk;
    usize size, max_len;

    /* create a new chunk */
    max_len = USIZE_MAX - sizeof(ctoon_str_chunk);
    if (unlikely(len > max_len)) return false;
    size = len + sizeof(ctoon_str_chunk);
    size = ctoon_max(pool->chunk_size, size);
    chunk = (ctoon_str_chunk *)alc->malloc(alc->ctx, size);
    if (unlikely(!chunk)) return false;

    /* insert the new chunk as the head of the linked list */
    chunk->next = pool->chunks;
    chunk->chunk_size = size;
    pool->chunks = chunk;
    pool->cur = (char *)chunk + sizeof(ctoon_str_chunk);
    pool->end = (char *)chunk + size;

    /* the next chunk is twice the size of the current one */
    size = ctoon_min(pool->chunk_size * 2, pool->chunk_size_max);
    if (size < pool->chunk_size) size = pool->chunk_size_max; /* overflow */
    pool->chunk_size = size;
    return true;
}

bool unsafe_ctoon_val_pool_grow(ctoon_val_pool *pool,
                                 const ctoon_alc *alc, usize count) {
    ctoon_val_chunk *chunk;
    usize size, max_count;

    /* create a new chunk */
    max_count = USIZE_MAX / sizeof(ctoon_mut_val) - 1;
    if (unlikely(count > max_count)) return false;
    size = (count + 1) * sizeof(ctoon_mut_val);
    size = ctoon_max(pool->chunk_size, size);
    chunk = (ctoon_val_chunk *)alc->malloc(alc->ctx, size);
    if (unlikely(!chunk)) return false;

    /* insert the new chunk as the head of the linked list */
    chunk->next = pool->chunks;
    chunk->chunk_size = size;
    pool->chunks = chunk;
    pool->cur = (ctoon_mut_val *)(void *)((u8 *)chunk) + 1;
    pool->end = (ctoon_mut_val *)(void *)((u8 *)chunk + size);

    /* the next chunk is twice the size of the current one */
    size = ctoon_min(pool->chunk_size * 2, pool->chunk_size_max);
    if (size < pool->chunk_size) size = pool->chunk_size_max; /* overflow */
    pool->chunk_size = size;
    return true;
}

bool ctoon_mut_doc_set_str_pool_size(ctoon_mut_doc *doc, size_t len) {
    usize max_size = USIZE_MAX - sizeof(ctoon_str_chunk);
    if (!doc || !len || len > max_size) return false;
    doc->str_pool.chunk_size = len + sizeof(ctoon_str_chunk);
    return true;
}

bool ctoon_mut_doc_set_val_pool_size(ctoon_mut_doc *doc, size_t count) {
    usize max_count = USIZE_MAX / sizeof(ctoon_mut_val) - 1;
    if (!doc || !count || count > max_count) return false;
    doc->val_pool.chunk_size = (count + 1) * sizeof(ctoon_mut_val);
    return true;
}

void ctoon_mut_doc_free(ctoon_mut_doc *doc) {
    if (doc) {
        ctoon_alc alc = doc->alc;
        memset(&doc->alc, 0, sizeof(alc));
        unsafe_ctoon_str_pool_release(&doc->str_pool, &alc);
        unsafe_ctoon_val_pool_release(&doc->val_pool, &alc);
        alc.free(alc.ctx, doc);
    }
}

ctoon_mut_doc *ctoon_mut_doc_new(const ctoon_alc *alc) {
    ctoon_mut_doc *doc;
    if (!alc) alc = &CTOON_DEFAULT_ALC;
    doc = (ctoon_mut_doc *)alc->malloc(alc->ctx, sizeof(ctoon_mut_doc));
    if (!doc) return NULL;
    memset(doc, 0, sizeof(ctoon_mut_doc));

    doc->alc = *alc;
    doc->str_pool.chunk_size = CTOON_MUT_DOC_STR_POOL_INIT_SIZE;
    doc->str_pool.chunk_size_max = CTOON_MUT_DOC_STR_POOL_MAX_SIZE;
    doc->val_pool.chunk_size = CTOON_MUT_DOC_VAL_POOL_INIT_SIZE;
    doc->val_pool.chunk_size_max = CTOON_MUT_DOC_VAL_POOL_MAX_SIZE;
    return doc;
}

ctoon_mut_doc *ctoon_doc_mut_copy(ctoon_doc *doc, const ctoon_alc *alc) {
    ctoon_mut_doc *m_doc;
    ctoon_mut_val *m_val;

    if (!doc || !doc->root) return NULL;
    m_doc = ctoon_mut_doc_new(alc);
    if (!m_doc) return NULL;
    m_val = ctoon_val_mut_copy(m_doc, doc->root);
    if (!m_val) {
        ctoon_mut_doc_free(m_doc);
        return NULL;
    }
    ctoon_mut_doc_set_root(m_doc, m_val);
    return m_doc;
}

ctoon_mut_doc *ctoon_mut_doc_mut_copy(ctoon_mut_doc *doc,
                                        const ctoon_alc *alc) {
    ctoon_mut_doc *m_doc;
    ctoon_mut_val *m_val;

    if (!doc) return NULL;
    if (!doc->root) return ctoon_mut_doc_new(alc);

    m_doc = ctoon_mut_doc_new(alc);
    if (!m_doc) return NULL;
    m_val = ctoon_mut_val_mut_copy(m_doc, doc->root);
    if (!m_val) {
        ctoon_mut_doc_free(m_doc);
        return NULL;
    }
    ctoon_mut_doc_set_root(m_doc, m_val);
    return m_doc;
}

ctoon_mut_val *ctoon_val_mut_copy(ctoon_mut_doc *m_doc,
                                    ctoon_val *i_vals) {
    /*
     The immutable object or array stores all sub-values in a contiguous memory,
     We copy them to another contiguous memory as mutable values,
     then reconnect the mutable values with the original relationship.
     */
    usize i_vals_len;
    ctoon_mut_val *m_vals, *m_val;
    ctoon_val *i_val, *i_end;

    if (!m_doc || !i_vals) return NULL;
    i_end = unsafe_ctoon_get_next(i_vals);
    i_vals_len = (usize)(unsafe_ctoon_get_next(i_vals) - i_vals);
    m_vals = unsafe_ctoon_mut_val(m_doc, i_vals_len);
    if (!m_vals) return NULL;
    i_val = i_vals;
    m_val = m_vals;

    for (; i_val < i_end; i_val++, m_val++) {
        ctoon_type type = unsafe_ctoon_get_type(i_val);
        m_val->tag = i_val->tag;
        m_val->uni.u64 = i_val->uni.u64;
        if (type == CTOON_TYPE_STR || type == CTOON_TYPE_RAW) {
            const char *str = i_val->uni.str;
            usize str_len = unsafe_ctoon_get_len(i_val);
            m_val->uni.str = unsafe_ctoon_mut_strncpy(m_doc, str, str_len);
            if (!m_val->uni.str) return NULL;
        } else if (type == CTOON_TYPE_ARR) {
            usize len = unsafe_ctoon_get_len(i_val);
            if (len > 0) {
                ctoon_val *ii_val = i_val + 1, *ii_next;
                ctoon_mut_val *mm_val = m_val + 1, *mm_ctn = m_val, *mm_next;
                while (len-- > 1) {
                    ii_next = unsafe_ctoon_get_next(ii_val);
                    mm_next = mm_val + (ii_next - ii_val);
                    mm_val->next = mm_next;
                    ii_val = ii_next;
                    mm_val = mm_next;
                }
                mm_val->next = mm_ctn + 1;
                mm_ctn->uni.ptr = mm_val;
            }
        } else if (type == CTOON_TYPE_OBJ) {
            usize len = unsafe_ctoon_get_len(i_val);
            if (len > 0) {
                ctoon_val *ii_key = i_val + 1, *ii_nextkey;
                ctoon_mut_val *mm_key = m_val + 1, *mm_ctn = m_val;
                ctoon_mut_val *mm_nextkey;
                while (len-- > 1) {
                    ii_nextkey = unsafe_ctoon_get_next(ii_key + 1);
                    mm_nextkey = mm_key + (ii_nextkey - ii_key);
                    mm_key->next = mm_key + 1;
                    mm_key->next->next = mm_nextkey;
                    ii_key = ii_nextkey;
                    mm_key = mm_nextkey;
                }
                mm_key->next = mm_key + 1;
                mm_key->next->next = mm_ctn + 1;
                mm_ctn->uni.ptr = mm_key;
            }
        }
    }
    return m_vals;
}

static ctoon_mut_val *unsafe_ctoon_mut_val_mut_copy(ctoon_mut_doc *m_doc,
                                                      ctoon_mut_val *m_vals) {
    /*
     The mutable object or array stores all sub-values in a circular linked
     list, so we can traverse them in the same loop. The traversal starts from
     the last item, continues with the first item in a list, and ends with the
     second to last item, which needs to be linked to the last item to close the
     circle.
     */
    ctoon_mut_val *m_val = unsafe_ctoon_mut_val(m_doc, 1);
    if (unlikely(!m_val)) return NULL;
    m_val->tag = m_vals->tag;

    switch (unsafe_ctoon_get_type(m_vals)) {
        case CTOON_TYPE_OBJ:
        case CTOON_TYPE_ARR:
            if (unsafe_ctoon_get_len(m_vals) > 0) {
                ctoon_mut_val *last = (ctoon_mut_val *)m_vals->uni.ptr;
                ctoon_mut_val *next = last->next, *prev;
                prev = unsafe_ctoon_mut_val_mut_copy(m_doc, last);
                if (!prev) return NULL;
                m_val->uni.ptr = (void *)prev;
                while (next != last) {
                    prev->next = unsafe_ctoon_mut_val_mut_copy(m_doc, next);
                    if (!prev->next) return NULL;
                    prev = prev->next;
                    next = next->next;
                }
                prev->next = (ctoon_mut_val *)m_val->uni.ptr;
            }
            break;
        case CTOON_TYPE_RAW:
        case CTOON_TYPE_STR: {
            const char *str = m_vals->uni.str;
            usize str_len = unsafe_ctoon_get_len(m_vals);
            m_val->uni.str = unsafe_ctoon_mut_strncpy(m_doc, str, str_len);
            if (!m_val->uni.str) return NULL;
            break;
        }
        default:
            m_val->uni = m_vals->uni;
            break;
    }
    return m_val;
}

ctoon_mut_val *ctoon_mut_val_mut_copy(ctoon_mut_doc *doc,
                                        ctoon_mut_val *val) {
    if (doc && val) return unsafe_ctoon_mut_val_mut_copy(doc, val);
    return NULL;
}

/* Count the number of values and the total length of the strings. */
static void ctoon_mut_stat(ctoon_mut_val *val,
                            usize *val_sum, usize *str_sum) {
    ctoon_type type = unsafe_ctoon_get_type(val);
    *val_sum += 1;
    if (type == CTOON_TYPE_ARR || type == CTOON_TYPE_OBJ) {
        ctoon_mut_val *child = (ctoon_mut_val *)val->uni.ptr;
        usize len = unsafe_ctoon_get_len(val), i;
        len <<= (u8)(type == CTOON_TYPE_OBJ);
        *val_sum += len;
        for (i = 0; i < len; i++) {
            ctoon_type stype = unsafe_ctoon_get_type(child);
            if (stype == CTOON_TYPE_STR || stype == CTOON_TYPE_RAW) {
                *str_sum += unsafe_ctoon_get_len(child) + 1;
            } else if (stype == CTOON_TYPE_ARR || stype == CTOON_TYPE_OBJ) {
                ctoon_mut_stat(child, val_sum, str_sum);
                *val_sum -= 1;
            }
            child = child->next;
        }
    } else if (type == CTOON_TYPE_STR || type == CTOON_TYPE_RAW) {
        *str_sum += unsafe_ctoon_get_len(val) + 1;
    }
}

/* Copy mutable values to immutable value pool. */
static usize ctoon_imut_copy(ctoon_val **val_ptr, char **buf_ptr,
                              ctoon_mut_val *mval) {
    ctoon_val *val = *val_ptr;
    ctoon_type type = unsafe_ctoon_get_type(mval);
    if (type == CTOON_TYPE_ARR || type == CTOON_TYPE_OBJ) {
        ctoon_mut_val *child = (ctoon_mut_val *)mval->uni.ptr;
        usize len = unsafe_ctoon_get_len(mval), i;
        usize val_sum = 1;
        if (type == CTOON_TYPE_OBJ) {
            if (len) child = child->next->next;
            len <<= 1;
        } else {
            if (len) child = child->next;
        }
        *val_ptr = val + 1;
        for (i = 0; i < len; i++) {
            val_sum += ctoon_imut_copy(val_ptr, buf_ptr, child);
            child = child->next;
        }
        val->tag = mval->tag;
        val->uni.ofs = val_sum * sizeof(ctoon_val);
        return val_sum;
    } else if (type == CTOON_TYPE_STR || type == CTOON_TYPE_RAW) {
        char *buf = *buf_ptr;
        usize len = unsafe_ctoon_get_len(mval);
        memcpy((void *)buf, (const void *)mval->uni.str, len);
        buf[len] = '\0';
        val->tag = mval->tag;
        val->uni.str = buf;
        *val_ptr = val + 1;
        *buf_ptr = buf + len + 1;
        return 1;
    } else {
        val->tag = mval->tag;
        val->uni = mval->uni;
        *val_ptr = val + 1;
        return 1;
    }
}

ctoon_doc *ctoon_mut_doc_imut_copy(ctoon_mut_doc *mdoc,
                                     const ctoon_alc *alc) {
    if (!mdoc) return NULL;
    return ctoon_mut_val_imut_copy(mdoc->root, alc);
}

ctoon_doc *ctoon_mut_val_imut_copy(ctoon_mut_val *mval,
                                     const ctoon_alc *alc) {
    usize val_num = 0, str_sum = 0, hdr_size, buf_size;
    ctoon_doc *doc = NULL;
    ctoon_val *val_hdr = NULL;

    /* This value should be NULL here. Setting a non-null value suppresses
       warning from the clang analyzer. */
    char *str_hdr = (char *)(void *)&str_sum;
    if (!mval) return NULL;
    if (!alc) alc = &CTOON_DEFAULT_ALC;

    /* traverse the input value to get pool size */
    ctoon_mut_stat(mval, &val_num, &str_sum);

    /* create doc and val pool */
    hdr_size = size_align_up(sizeof(ctoon_doc), sizeof(ctoon_val));
    buf_size = hdr_size + val_num * sizeof(ctoon_val);
    doc = (ctoon_doc *)alc->malloc(alc->ctx, buf_size);
    if (!doc) return NULL;
    memset(doc, 0, sizeof(ctoon_doc));
    val_hdr = (ctoon_val *)(void *)((char *)(void *)doc + hdr_size);
    doc->root = val_hdr;
    doc->alc = *alc;

    /* create str pool */
    if (str_sum > 0) {
        str_hdr = (char *)alc->malloc(alc->ctx, str_sum);
        doc->str_pool = str_hdr;
        if (!str_hdr) {
            alc->free(alc->ctx, (void *)doc);
            return NULL;
        }
    }

    /* copy vals and strs */
    doc->val_read = ctoon_imut_copy(&val_hdr, &str_hdr, mval);
    doc->dat_read = str_sum + 1;
    return doc;
}

static_inline bool unsafe_ctoon_num_equals(void *lhs, void *rhs) {
    ctoon_val_uni *luni = &((ctoon_val *)lhs)->uni;
    ctoon_val_uni *runi = &((ctoon_val *)rhs)->uni;
    ctoon_subtype lt = unsafe_ctoon_get_subtype(lhs);
    ctoon_subtype rt = unsafe_ctoon_get_subtype(rhs);
    if (lt == rt) return luni->u64 == runi->u64;
    if (lt == CTOON_SUBTYPE_SINT && rt == CTOON_SUBTYPE_UINT) {
        return luni->i64 >= 0 && luni->u64 == runi->u64;
    }
    if (lt == CTOON_SUBTYPE_UINT && rt == CTOON_SUBTYPE_SINT) {
        return runi->i64 >= 0 && luni->u64 == runi->u64;
    }
    return false;
}

static_inline bool unsafe_ctoon_str_equals(void *lhs, void *rhs) {
    usize len = unsafe_ctoon_get_len(lhs);
    if (len != unsafe_ctoon_get_len(rhs)) return false;
    return !memcmp(unsafe_ctoon_get_str(lhs),
                   unsafe_ctoon_get_str(rhs), len);
}

bool unsafe_ctoon_equals(ctoon_val *lhs, ctoon_val *rhs) {
    ctoon_type type = unsafe_ctoon_get_type(lhs);
    if (type != unsafe_ctoon_get_type(rhs)) return false;

    switch (type) {
        case CTOON_TYPE_OBJ: {
            usize len = unsafe_ctoon_get_len(lhs);
            if (len != unsafe_ctoon_get_len(rhs)) return false;
            if (len > 0) {
                ctoon_obj_iter iter;
                ctoon_obj_iter_init(rhs, &iter);
                lhs = unsafe_ctoon_get_first(lhs);
                while (len-- > 0) {
                    rhs = ctoon_obj_iter_getn(&iter, lhs->uni.str,
                                               unsafe_ctoon_get_len(lhs));
                    if (!rhs) return false;
                    if (!unsafe_ctoon_equals(lhs + 1, rhs)) return false;
                    lhs = unsafe_ctoon_get_next(lhs + 1);
                }
            }
            /* ctoon allows duplicate keys, so the check may be inaccurate */
            return true;
        }

        case CTOON_TYPE_ARR: {
            usize len = unsafe_ctoon_get_len(lhs);
            if (len != unsafe_ctoon_get_len(rhs)) return false;
            if (len > 0) {
                lhs = unsafe_ctoon_get_first(lhs);
                rhs = unsafe_ctoon_get_first(rhs);
                while (len-- > 0) {
                    if (!unsafe_ctoon_equals(lhs, rhs)) return false;
                    lhs = unsafe_ctoon_get_next(lhs);
                    rhs = unsafe_ctoon_get_next(rhs);
                }
            }
            return true;
        }

        case CTOON_TYPE_NUM:
            return unsafe_ctoon_num_equals(lhs, rhs);

        case CTOON_TYPE_RAW:
        case CTOON_TYPE_STR:
            return unsafe_ctoon_str_equals(lhs, rhs);

        case CTOON_TYPE_NULL:
        case CTOON_TYPE_BOOL:
            return lhs->tag == rhs->tag;

        default:
            return false;
    }
}

bool unsafe_ctoon_mut_equals(ctoon_mut_val *lhs, ctoon_mut_val *rhs) {
    ctoon_type type = unsafe_ctoon_get_type(lhs);
    if (type != unsafe_ctoon_get_type(rhs)) return false;

    switch (type) {
        case CTOON_TYPE_OBJ: {
            usize len = unsafe_ctoon_get_len(lhs);
            if (len != unsafe_ctoon_get_len(rhs)) return false;
            if (len > 0) {
                ctoon_mut_obj_iter iter;
                ctoon_mut_obj_iter_init(rhs, &iter);
                lhs = (ctoon_mut_val *)lhs->uni.ptr;
                while (len-- > 0) {
                    rhs = ctoon_mut_obj_iter_getn(&iter, lhs->uni.str,
                                                   unsafe_ctoon_get_len(lhs));
                    if (!rhs) return false;
                    if (!unsafe_ctoon_mut_equals(lhs->next, rhs)) return false;
                    lhs = lhs->next->next;
                }
            }
            /* ctoon allows duplicate keys, so the check may be inaccurate */
            return true;
        }

        case CTOON_TYPE_ARR: {
            usize len = unsafe_ctoon_get_len(lhs);
            if (len != unsafe_ctoon_get_len(rhs)) return false;
            if (len > 0) {
                lhs = (ctoon_mut_val *)lhs->uni.ptr;
                rhs = (ctoon_mut_val *)rhs->uni.ptr;
                while (len-- > 0) {
                    if (!unsafe_ctoon_mut_equals(lhs, rhs)) return false;
                    lhs = lhs->next;
                    rhs = rhs->next;
                }
            }
            return true;
        }

        case CTOON_TYPE_NUM:
            return unsafe_ctoon_num_equals(lhs, rhs);

        case CTOON_TYPE_RAW:
        case CTOON_TYPE_STR:
            return unsafe_ctoon_str_equals(lhs, rhs);

        case CTOON_TYPE_NULL:
        case CTOON_TYPE_BOOL:
            return lhs->tag == rhs->tag;

        default:
            return false;
    }
}

bool ctoon_locate_pos(const char *str, size_t len, size_t pos,
                       size_t *line, size_t *col, size_t *chr) {
    usize line_sum = 0, line_pos = 0, chr_sum = 0;
    const u8 *cur = (const u8 *)str;
    const u8 *end = cur + pos;

    if (!str || pos > len) {
        if (line) *line = 0;
        if (col) *col = 0;
        if (chr) *chr = 0;
        return false;
    }

    if (pos >= 3 && is_utf8_bom(cur)) cur += 3; /* don't count BOM */
    while (cur < end) {
        u8 c = *cur;
        chr_sum += 1;
        if (likely(c < 0x80)) {         /* 0xxxxxxx (0x00-0x7F) ASCII */
            if (c == '\n') {
                line_sum += 1;
                line_pos = chr_sum;
            }
            cur += 1;
        }
        else if (c < 0xC0) cur += 1;    /* 10xxxxxx (0x80-0xBF) Invalid */
        else if (c < 0xE0) cur += 2;    /* 110xxxxx (0xC0-0xDF) 2-byte UTF-8 */
        else if (c < 0xF0) cur += 3;    /* 1110xxxx (0xE0-0xEF) 3-byte UTF-8 */
        else if (c < 0xF8) cur += 4;    /* 11110xxx (0xF0-0xF7) 4-byte UTF-8 */
        else               cur += 1;    /* 11111xxx (0xF8-0xFF) Invalid */
    }
    if (line) *line = line_sum + 1;
    if (col) *col = chr_sum - line_pos + 1;
    if (chr) *chr = chr_sum;
    return true;
}



static bool ctoon_has_flag(ctoon_read_flag flags, ctoon_read_flag check) {
    return (flags & check) != 0;
}

#if !CTOON_DISABLE_READER

/*============================================================================
 * Internal helpers
 *===========================================================================*/

static bool ctoon_read_has_flag(ctoon_read_flag flags, ctoon_read_flag check) {
    return (flags & check) != 0;
}

/* fread wrapper for portability */
static usize ctoon_sys_fread(void *buf, usize size, FILE *file) {
#if defined(_WIN32) && !defined(__MINGW32__)
    return fread_s(buf, size, 1, size, file);
#else
    return fread(buf, 1, size, file);
#endif
}

/*============================================================================
 * String pool  – single pre-allocated buffer, bump allocator
 *
 * Total decoded string bytes can never exceed the input byte count, so we
 * allocate one buffer of (input_len + 1) bytes upfront.  Pointers into this
 * buffer are stable for the lifetime of the document.
 *
 * doc->str_pool points to this buffer; ctoon_doc_free releases it with a
 * single alc.free() call.
 *===========================================================================*/

typedef struct {
    char     *buf;   /* base of the pre-allocated buffer */
    usize     used;  /* bytes written so far */
    usize     cap;   /* total usable bytes (= input_len + 1) */
} ctoon_str_pool_buf;

static bool ctoon_str_pool_buf_init(ctoon_str_pool_buf *sp, ctoon_alc alc, usize input_len) {
    usize cap = input_len + 1;
    sp->buf  = (char *)alc.malloc(alc.ctx, cap);
    if (!sp->buf) return false;
    sp->used = 0;
    sp->cap  = cap;
    return true;
}

static void ctoon_str_pool_buf_free(ctoon_str_pool_buf *sp, ctoon_alc alc) {
    alc.free(alc.ctx, sp->buf);
    sp->buf = NULL;
}

/* Copy [src, src+len) into pool, append '\0', return pointer.
   The caller guarantees that total bytes written never exceed cap. */
static const char *ctoon_str_pool_buf_put(ctoon_str_pool_buf *sp, const u8 *src, usize len) {
    /* Assertion: sp->used + len + 1 <= sp->cap (always true by pre-alloc). */
    char *dst = sp->buf + sp->used;
    memcpy(dst, src, len);
    dst[len] = '\0';
    sp->used += len + 1;
    return dst;
}

/*============================================================================
 * Val pool  – flat resizable array of ctoon_val
 *===========================================================================*/

typedef struct {
    ctoon_val *base;   /* allocation start (ctoon_doc lives here) */
    ctoon_val *cur;    /* next slot to fill */
    usize      cap;    /* total slots allocated */
    ctoon_alc  alc;
} ctoon_read_vpool;

static bool ctoon_read_vpool_init(ctoon_read_vpool *vp, ctoon_alc alc,
                             usize hdr_slots, usize est_vals) {
    usize total = hdr_slots + est_vals;
    vp->base = (ctoon_val *)alc.malloc(alc.ctx, total * sizeof(ctoon_val));
    if (!vp->base) return false;
    vp->cur  = vp->base + hdr_slots;
    vp->cap  = total;
    vp->alc  = alc;
    return true;
}

static bool ctoon_read_vpool_grow(ctoon_read_vpool *vp) {
    usize cur_idx = (usize)(vp->cur - vp->base);
    usize new_cap = vp->cap + vp->cap / 2;
    if (new_cap < vp->cap + 4) new_cap = vp->cap + 4;
    ctoon_val *tmp = (ctoon_val *)vp->alc.realloc(
        vp->alc.ctx, vp->base,
        vp->cap * sizeof(ctoon_val),
        new_cap * sizeof(ctoon_val));
    if (!tmp) return false;
    vp->base = tmp;
    vp->cur  = tmp + cur_idx;
    vp->cap  = new_cap;
    return true;
}

/* Reserve next slot; grow if needed.  Does NOT advance cur. */
static ctoon_val *ctoon_read_vpool_reserve(ctoon_read_vpool *vp) {
    /* keep 1-slot headroom so callers may safely look at cur+1 */
    if (vp->cur + 1 >= vp->base + vp->cap)
        if (!ctoon_read_vpool_grow(vp)) return NULL;
    return vp->cur;
}

/* Reserve + advance. */
static ctoon_val *ctoon_read_vpool_alloc(ctoon_read_vpool *vp) {
    ctoon_val *v = ctoon_read_vpool_reserve(vp);
    if (v) vp->cur++;
    return v;
}

/*============================================================================
 * Container stack  – tracks open OBJ/ARR while parsing
 *===========================================================================*/

#define C2R_STACK_INIT 32

typedef struct {
    usize val_idx;   /* index of container val in vpool.base */
    usize count;     /* child count so far */
} ctoon_ctn_frame;

typedef struct {
    ctoon_ctn_frame *frames;
    usize      depth;  /* current depth (0 = sentinel) */
    usize      cap;
    ctoon_alc  alc;
} ctoon_ctn_stack;

static bool ctoon_ctn_stack_init(ctoon_ctn_stack *st, ctoon_alc alc) {
    st->frames = (ctoon_ctn_frame *)alc.malloc(alc.ctx,
                     C2R_STACK_INIT * sizeof(ctoon_ctn_frame));
    if (!st->frames) return false;
    st->depth  = 0;
    st->cap    = C2R_STACK_INIT;
    st->alc    = alc;
    return true;
}

static void ctoon_ctn_stack_free(ctoon_ctn_stack *st) {
    st->alc.free(st->alc.ctx, st->frames);
    st->frames = NULL;
}

static bool ctoon_ctn_stack_push(ctoon_ctn_stack *st, usize val_idx) {
    usize new_depth = st->depth + 1;
    if (new_depth >= st->cap) {
        usize nc  = st->cap * 2;
        ctoon_ctn_frame *tmp = (ctoon_ctn_frame *)st->alc.realloc(
            st->alc.ctx, st->frames,
            st->cap * sizeof(ctoon_ctn_frame),
            nc      * sizeof(ctoon_ctn_frame));
        if (!tmp) return false;
        st->frames = tmp;
        st->cap    = nc;
    }
    st->frames[new_depth] = (ctoon_ctn_frame){ val_idx, 0 };
    st->depth = new_depth;
    return true;
}

static ctoon_ctn_frame *ctoon_ctn_stack_top(ctoon_ctn_stack *st) {
    return &st->frames[st->depth];
}

static void ctoon_ctn_stack_pop(ctoon_ctn_stack *st) {
    if (st->depth > 0) st->depth--;
}

/*============================================================================
 * Parse context
 *===========================================================================*/

typedef struct {
    const u8       *hdr;     /* start of (padded) input */
    const u8       *cur;     /* read cursor */
    const u8       *eof;     /* one past last real byte */
    int             indent;  /* spaces per level */
    char            delim;   /* active delimiter: ',' '|' '\t' */
    ctoon_read_flag flags;
    ctoon_read_vpool       vp;
    ctoon_ctn_stack       st;
    ctoon_str_pool_buf       sp;
    ctoon_read_err *err;
    usize           hdr_slots; /* val slots consumed by ctoon_doc */
} ctoon_read_ctx;

/*----------------------------------------------------------------------------
 * Cursor helpers
 *---------------------------------------------------------------------------*/

static bool ctoon_cur_at_eof(const ctoon_read_ctx *c) { return c->cur >= c->eof; }
static u8   ctoon_cur_peek  (const ctoon_read_ctx *c) { return c->cur < c->eof ? *c->cur : 0; }

static void ctoon_cur_skip_spaces(ctoon_read_ctx *c) {
    while (c->cur < c->eof && *c->cur == ' ') c->cur++;
}

static void ctoon_cur_skip_to_eol(ctoon_read_ctx *c) {
    while (c->cur < c->eof && *c->cur != '\n') c->cur++;
}

static void ctoon_cur_skip_nl(ctoon_read_ctx *c) {
    if (c->cur < c->eof && *c->cur == '\r') c->cur++;
    if (c->cur < c->eof && *c->cur == '\n') c->cur++;
}

static bool ctoon_cur_at_eol(const ctoon_read_ctx *c) {
    return c->cur >= c->eof || *c->cur == '\n' || *c->cur == '\r';
}

static int ctoon_cur_measure_indent(const ctoon_read_ctx *c) {
    const u8 *p = c->cur;
    while (p < c->eof && *p == ' ') p++;
    return (int)((p - c->cur) / (usize)c->indent);
}

static bool ctoon_cur_consume_indent(ctoon_read_ctx *c, int depth) {
    int spaces = depth * c->indent;
    for (int i = 0; i < spaces; i++) {
        if (c->cur >= c->eof || *c->cur != ' ') return false;
        c->cur++;
    }
    return true;
}

/*----------------------------------------------------------------------------
 * Error helper
 *---------------------------------------------------------------------------*/

static bool ctoon_read_set_err(ctoon_read_ctx *c, ctoon_read_code code, const char *msg) {
    if (c->err) {
        c->err->code = code;
        c->err->msg  = msg;
        c->err->pos  = (usize)(c->cur - c->hdr);
    }
    return false;
}

/*----------------------------------------------------------------------------
 * Container open / close
 *---------------------------------------------------------------------------*/

static bool ctoon_ctn_open(ctoon_read_ctx *c, u8 type) {
    ctoon_val *cv = ctoon_read_vpool_alloc(&c->vp);
    if (!cv) return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
    cv->tag     = (u64)type;
    cv->uni.ofs = 0;
    usize idx   = (usize)(cv - c->vp.base);
    if (!ctoon_ctn_stack_push(&c->st, idx))
        return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
    return true;
}

static void ctoon_ctn_close(ctoon_read_ctx *c, u8 type) {
    ctoon_ctn_frame *fr = ctoon_ctn_stack_top(&c->st);
    ctoon_val *cv = c->vp.base + fr->val_idx;
    /* forward offset = distance in bytes to the next-sibling slot */
    cv->uni.ofs = (usize)((u8 *)c->vp.cur - (u8 *)cv);
    /* child count in upper bits; OBJ and ARR both use TAG_BIT-1 shift */
    cv->tag = ((u64)fr->count << CTOON_TAG_BIT) | (u64)type;
    ctoon_ctn_stack_pop(&c->st);
}

static void ctoon_ctn_child_added(ctoon_read_ctx *c) {
    ctoon_ctn_stack_top(&c->st)->count++;
}

/*============================================================================
 * Scalar parsers
 *===========================================================================*/

/* Parse a quoted string starting at cur (must be '"').
   Decodes escape sequences into a temporary buffer, then copies into the
   spool.  Sets val->tag and val->uni.str on success. */
static bool ctoon_parse_str_quoted(ctoon_read_ctx *c, ctoon_val *val) {
    c->cur++;   /* skip opening '"' */
    const u8 *src = c->cur;

    /* First pass: compute worst-case decoded length */
    usize dec_len = 0;
    {
        const u8 *s = src;
        while (s < c->eof && *s != '"' && *s != '\n') {
            if (*s == '\\') {
                s++;
                if (s >= c->eof) break;
                dec_len += (*s == 'u') ? 3 : 1;
                if (*s == 'u') s += 4; else s++;
                continue;
            }
            dec_len++;
            s++;
        }
    }

    /* Decode into a temporary heap buffer (freed before return) */
    char *tmp_buf = (char *)c->vp.alc.malloc(c->vp.alc.ctx, dec_len + 1);
    if (!tmp_buf)
        return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
    char *dst_cur = tmp_buf;

    while (src < c->eof && *src != '"' && *src != '\n') {
        if (likely(*src != '\\')) {
            *dst_cur++ = (char)*src++;
            continue;
        }
        src++;  /* skip backslash */
        if (src >= c->eof)
            return ctoon_read_set_err(c, CTOON_READ_ERROR_INVALID_STRING, "truncated escape");
        switch (*src) {
            case '"':  *dst_cur++ = '"';  src++; break;
            case '\\': *dst_cur++ = '\\'; src++; break;
            case '/':  *dst_cur++ = '/';  src++; break;
            case 'n':  *dst_cur++ = '\n'; src++; break;
            case 'r':  *dst_cur++ = '\r'; src++; break;
            case 't':  *dst_cur++ = '\t'; src++; break;
            case 'b':  *dst_cur++ = '\b'; src++; break;
            case 'f':  *dst_cur++ = '\f'; src++; break;
            case 'u': {
                if (src + 4 >= c->eof)
                    return ctoon_read_set_err(c, CTOON_READ_ERROR_INVALID_STRING, "truncated \\u");
                src++;
                u32 cp = 0;
                for (int k = 0; k < 4; k++) {
                    u8  ch  = *src++;
                    u32 nib;
                    if      (ch >= '0' && ch <= '9') nib = (u32)(ch - '0');
                    else if (ch >= 'a' && ch <= 'f') nib = (u32)(ch - 'a' + 10);
                    else if (ch >= 'A' && ch <= 'F') nib = (u32)(ch - 'A' + 10);
                    else return ctoon_read_set_err(c, CTOON_READ_ERROR_INVALID_STRING, "bad \\u hex");
                    cp = (cp << 4) | nib;
                }
                if (cp < 0x80) {
                    *dst_cur++ = (char)(u8)cp;
                } else if (cp < 0x800) {
                    *dst_cur++ = (char)(u8)(0xC0 | (cp >> 6));
                    *dst_cur++ = (char)(u8)(0x80 | (cp & 0x3F));
                } else {
                    *dst_cur++ = (char)(u8)(0xE0 | (cp >> 12));
                    *dst_cur++ = (char)(u8)(0x80 | ((cp >> 6) & 0x3F));
                    *dst_cur++ = (char)(u8)(0x80 | (cp & 0x3F));
                }
                break;
            }
            default:
                *dst_cur++ = '\\';
                *dst_cur++ = (char)*src++;
                break;
        }
    }
    if (src >= c->eof || *src != '"') {
        c->vp.alc.free(c->vp.alc.ctx, tmp_buf);
        return ctoon_read_set_err(c, CTOON_READ_ERROR_INVALID_STRING, "unterminated string");
    }

    usize actual_len = (usize)(dst_cur - tmp_buf);
    const char *interned = ctoon_str_pool_buf_put(&c->sp, (const u8 *)tmp_buf, actual_len);
    c->vp.alc.free(c->vp.alc.ctx, tmp_buf);
    if (!interned)
        return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);

    unsafe_ctoon_set_tag(val, CTOON_TYPE_STR, CTOON_SUBTYPE_NONE, actual_len);
    val->uni.str = interned;
    c->cur = src + 1;   /* skip closing '"' */
    return true;
}

/* Parse an unquoted token (key or value).
   stop_chars: additional characters that terminate the token (besides EOL).
   Sets val->tag/uni on success. */
static bool ctoon_parse_str_raw(ctoon_read_ctx *c, ctoon_val *val,
                                const char *stop_chars) {
    ctoon_cur_skip_spaces(c);
    if (ctoon_cur_at_eol(c))
        return ctoon_read_set_err(c, CTOON_READ_ERROR_UNEXPECTED_CHARACTER, "expected value");

    /* Quoted string: dispatch to the proper quoted parser */
    if (ctoon_cur_peek(c) == '"') return ctoon_parse_str_quoted(c, val);

    const u8 *start = c->cur;
    while (c->cur < c->eof && *c->cur != '\n' && *c->cur != '\r') {
        if (stop_chars && strchr(stop_chars, (char)*c->cur)) break;
        c->cur++;
    }
    usize len = (usize)(c->cur - start);
    while (len > 0 && start[len - 1] == ' ') len--;
    if (len == 0)
        return ctoon_read_set_err(c, CTOON_READ_ERROR_UNEXPECTED_CHARACTER, "empty token");

    /* null / true / false */
    if (len == 4
        && start[0]=='n' && start[1]=='u'
        && start[2]=='l' && start[3]=='l') {
        val->tag = CTOON_TYPE_NULL; val->uni.u64 = 0; return true;
    }
    if (len == 4
        && start[0]=='t' && start[1]=='r'
        && start[2]=='u' && start[3]=='e') {
        val->tag = CTOON_TYPE_BOOL | CTOON_SUBTYPE_TRUE; val->uni.u64 = 0; return true;
    }
    if (len == 5
        && start[0]=='f' && start[1]=='a' && start[2]=='l'
        && start[3]=='s' && start[4]=='e') {
        val->tag = CTOON_TYPE_BOOL | CTOON_SUBTYPE_FALSE; val->uni.u64 = 0; return true;
    }

    /* number (only when short enough to fit the scratch buffer) */
    if (len < 64) {
        char tmp[64];
        memcpy(tmp, start, len);
        tmp[len] = '\0';

        /* integer: optional '-', then only digits */
        bool is_int = true;
        usize di = (tmp[0] == '-') ? 1u : 0u;
        if (di == len) is_int = false;
        for (usize k = di; k < len && is_int; k++)
            if ((unsigned)(tmp[k] - '0') > 9) is_int = false;

        if (is_int) {
            char *end;
            errno = 0;
            if (tmp[0] == '-') {
                long long iv = strtoll(tmp, &end, 10);
                if (end == tmp + len && errno == 0) {
                    val->tag = CTOON_TYPE_NUM | CTOON_SUBTYPE_SINT;
                    val->uni.i64 = (i64)iv;
                    return true;
                }
            } else {
                unsigned long long uv = strtoull(tmp, &end, 10);
                if (end == tmp + len && errno == 0) {
                    val->tag = CTOON_TYPE_NUM | CTOON_SUBTYPE_UINT;
                    val->uni.u64 = (u64)uv;
                    return true;
                }
            }
        }

        /* float */
        char *end;
        errno = 0;
        double dv = strtod(tmp, &end);
        if (end == tmp + len && errno == 0) {
            val->tag = CTOON_TYPE_NUM | CTOON_SUBTYPE_REAL;
            val->uni.f64 = dv;
            return true;
        }
    }

    /* unquoted string — copy into pool */
    const char *s = ctoon_str_pool_buf_put(&c->sp, start, len);
    if (!s) return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
    unsafe_ctoon_set_tag(val, CTOON_TYPE_STR, CTOON_SUBTYPE_NOESC, len);
    val->uni.str = s;
    return true;
}

/* Parse a key token (stops at ':' or '[').
   Writes result directly into *val. */
static bool ctoon_parse_key(ctoon_read_ctx *c, ctoon_val *val) {
    if (ctoon_cur_peek(c) == '"') return ctoon_parse_str_quoted(c, val);

    const u8 *start = c->cur;
    while (c->cur < c->eof
           && *c->cur != ':' && *c->cur != '['
           && *c->cur != '\n')
        c->cur++;
    usize len = (usize)(c->cur - start);
    while (len > 0 && start[len - 1] == ' ') len--;
    if (len == 0)
        return ctoon_read_set_err(c, CTOON_READ_ERROR_UNEXPECTED_CHARACTER, "empty key");
    const char *s = ctoon_str_pool_buf_put(&c->sp, start, len);
    if (!s) return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
    unsafe_ctoon_set_tag(val, CTOON_TYPE_STR, CTOON_SUBTYPE_NOESC, len);
    val->uni.str = s;
    return true;
}

/*============================================================================
 * Forward declarations
 *===========================================================================*/

static bool ctoon_parse_object    (ctoon_read_ctx *c, int depth);
static bool ctoon_parse_array(ctoon_read_ctx *c, int arr_depth);

/*============================================================================
 * Array content parsers
 *===========================================================================*/

/* Tabular rows: one OBJ per line, fields separated by delimiter */
static bool ctoon_parse_tabular(ctoon_read_ctx *c, int depth, long expected,
                               const char **fields, int nfields) {
    char delim_stop[2] = { c->delim, '\0' };
    long parsed = 0;
    while (parsed < expected) {
        /* skip blank lines */
        bool blank = true;
        for (const u8 *bl = c->cur;
             bl < c->eof && *bl != '\n' && *bl != '\r'; bl++)
            if (*bl != ' ') { blank = false; break; }
        if (blank) { ctoon_cur_skip_to_eol(c); ctoon_cur_skip_nl(c); continue; }

        if (ctoon_cur_measure_indent(c) != depth) break;
        if (!ctoon_cur_consume_indent(c, depth)) break;

        if (!ctoon_ctn_open(c, CTOON_TYPE_OBJ)) return false;

        for (int fi = 0; fi < nfields; fi++) {
            /* key */
            ctoon_val *kv = ctoon_read_vpool_alloc(&c->vp);
            if (!kv)
                return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
            usize klen = strlen(fields[fi]);
            unsafe_ctoon_set_tag(kv, CTOON_TYPE_STR, CTOON_SUBTYPE_NOESC, klen);
            kv->uni.str = fields[fi];
            /* value */
            ctoon_cur_skip_spaces(c);
            ctoon_val *vv = ctoon_read_vpool_alloc(&c->vp);
            if (!vv)
                return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
            if (!ctoon_parse_str_raw(c, vv,
                    fi < nfields - 1 ? delim_stop : NULL)) return false;
            ctoon_ctn_stack_top(&c->st)->count++; /* one kv-pair */
            if (fi < nfields - 1) {
                if (ctoon_cur_peek(c) != (u8)c->delim)
                    return ctoon_read_set_err(c, CTOON_READ_ERROR_UNEXPECTED_CHARACTER,
                                    "expected delimiter in tabular row");
                c->cur++;
            }
        }
        ctoon_ctn_close(c, CTOON_TYPE_OBJ);
        ctoon_ctn_stack_top(&c->st)->count++; /* row added to parent ARR */
        ctoon_cur_skip_to_eol(c); ctoon_cur_skip_nl(c);
        parsed++;
    }
    return true;
}

/* List items: each line starts with "- " at the given indent depth */
static bool ctoon_parse_list(ctoon_read_ctx *c, int depth) {
    char delim_stop[2] = { c->delim, '\0' };
    while (!ctoon_cur_at_eof(c)) {
        /* skip blank lines */
        bool blank = true;
        for (const u8 *bl = c->cur;
             bl < c->eof && *bl != '\n' && *bl != '\r'; bl++)
            if (*bl != ' ') { blank = false; break; }
        if (blank) { ctoon_cur_skip_to_eol(c); ctoon_cur_skip_nl(c); continue; }

        if (ctoon_cur_measure_indent(c) != depth) break;
        const u8 *row_start = c->cur;
        if (!ctoon_cur_consume_indent(c, depth)) { c->cur = row_start; break; }
        if (c->cur + 1 >= c->eof
            || *c->cur != '-' || *(c->cur + 1) != ' ') {
            c->cur = row_start; break;
        }
        c->cur += 2;
        ctoon_cur_skip_spaces(c);

        /* determine item shape: does the rest of this line have ':' or '['? */
        bool has_colon = false, has_bracket = false;
        bool in_quotes = false;
        for (const u8 *sc = c->cur;
             sc < c->eof && *sc != '\n' && *sc != '\r'; sc++) {
            if (*sc == '"')  { in_quotes = !in_quotes; continue; }
            if (in_quotes)   continue;
            if (*sc == ':')  { has_colon   = true; break; }
            if (*sc == '[')  { has_bracket = true; break; }
        }

        if (!has_colon && !has_bracket) {
            /* primitive item */
            ctoon_val *vv = ctoon_read_vpool_alloc(&c->vp);
            if (!vv)
                return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
            if (!ctoon_parse_str_raw(c, vv, NULL)) return false;
            ctoon_ctn_child_added(c);
            ctoon_cur_skip_to_eol(c); ctoon_cur_skip_nl(c);
        } else {
            /* object item: first field on this line, continuations at depth+1 */
            if (!ctoon_ctn_open(c, CTOON_TYPE_OBJ)) return false;
            bool first_field = true;
            for (;;) {
                if (!first_field) {
                    /* skip blank lines */
                    for (;;) {
                        if (ctoon_cur_at_eof(c)) goto obj_item_done;
                        bool bl2 = true;
                        for (const u8 *b = c->cur;
                             b < c->eof && *b != '\n' && *b != '\r'; b++)
                            if (*b != ' ') { bl2 = false; break; }
                        if (!bl2) break;
                        ctoon_cur_skip_to_eol(c); ctoon_cur_skip_nl(c);
                    }
                    if (ctoon_cur_measure_indent(c) != depth + 1) goto obj_item_done;
                    const u8 *cont = c->cur;
                    if (!ctoon_cur_consume_indent(c, depth + 1)) {
                        c->cur = cont; goto obj_item_done;
                    }
                    /* stop if this is the next list item */
                    if (c->cur + 1 < c->eof
                        && *c->cur == '-' && *(c->cur + 1) == ' ') {
                        c->cur = cont; goto obj_item_done;
                    }
                } else {
                    first_field = false;
                }
                /* key */
                ctoon_val *kv = ctoon_read_vpool_alloc(&c->vp);
                if (!kv)
                    return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION,
                                    MSG_MALLOC);
                if (!ctoon_parse_key(c, kv)) return false;
                ctoon_cur_skip_spaces(c);
                int fd = depth + 1;

                if (ctoon_cur_peek(c) == '[') {
                    c->cur++;
                    if (!ctoon_parse_array(c, fd)) return false;
                } else if (ctoon_cur_peek(c) == ':') {
                    c->cur++; ctoon_cur_skip_spaces(c);
                    if (ctoon_cur_at_eol(c)) {
                        ctoon_cur_skip_nl(c);
                        if (!ctoon_parse_object(c, fd + 1)) return false;
                    } else {
                        ctoon_val *vv = ctoon_read_vpool_alloc(&c->vp);
                        if (!vv)
                            return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION,
                                            MSG_MALLOC);
                        if (!ctoon_parse_str_raw(c, vv, NULL)) return false;
                        ctoon_cur_skip_to_eol(c); ctoon_cur_skip_nl(c);
                    }
                } else {
                    goto obj_item_done;
                }
                ctoon_ctn_stack_top(&c->st)->count++; /* one kv-pair */
            }
obj_item_done:
            ctoon_ctn_close(c, CTOON_TYPE_OBJ);
            ctoon_ctn_child_added(c); /* item added to parent ARR */
        }
    }
    (void)delim_stop;
    return true;
}

/*============================================================================
 * Array body (called after '[' consumed)
 *===========================================================================*/

static bool ctoon_parse_array(ctoon_read_ctx *c, int arr_depth) {
    /* read length digits */
    const u8 *ns = c->cur;
    while (c->cur < c->eof && (unsigned)(*c->cur - '0') <= 9) c->cur++;
    usize nl = (usize)(c->cur - ns);
    if (nl == 0 || nl >= 20)
        return ctoon_read_set_err(c, CTOON_READ_ERROR_TOON_STRUCTURE, "invalid array length");
    char nbuf[24];
    memcpy(nbuf, ns, nl); nbuf[nl] = '\0';
    long expected = atol(nbuf);

    /* optional delimiter marker inside brackets */
    if (c->cur < c->eof && *c->cur != ']') {
        char d = (char)*c->cur;
        c->delim = (d == '|' || d == '\t') ? d : ',';
        c->cur++;
    } else {
        c->delim = ',';
    }
    if (ctoon_cur_peek(c) != ']')
        return ctoon_read_set_err(c, CTOON_READ_ERROR_TOON_STRUCTURE, "expected ']'");
    c->cur++;

    /* optional {field1,field2} for tabular */
    const char *fields[32];
    int nfields = 0;
    if (ctoon_cur_peek(c) == '{') {
        c->cur++;
        while (ctoon_cur_peek(c) != '}' && !ctoon_cur_at_eol(c)) {
            ctoon_cur_skip_spaces(c);
            const u8 *fs = c->cur;
            while (c->cur < c->eof
                   && *c->cur != (u8)c->delim
                   && *c->cur != '}')
                c->cur++;
            usize fl = (usize)(c->cur - fs);
            while (fl > 0 && fs[fl - 1] == ' ') fl--;
            if (fl > 0 && nfields < 32) {
                const char *fn = ctoon_str_pool_buf_put(&c->sp, fs, fl);
                if (!fn)
                    return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION,
                                    MSG_MALLOC);
                fields[nfields++] = fn;
            }
            if (ctoon_cur_peek(c) == (u8)c->delim) c->cur++;
        }
        if (ctoon_cur_peek(c) == '}') c->cur++;
    }

    ctoon_cur_skip_spaces(c);
    if (ctoon_cur_peek(c) != ':')
        return ctoon_read_set_err(c, CTOON_READ_ERROR_TOON_STRUCTURE, "expected ':'");
    c->cur++;
    ctoon_cur_skip_spaces(c);

    if (!ctoon_ctn_open(c, CTOON_TYPE_ARR)) return false;

    if (nfields > 0) {
        /* tabular */
        ctoon_cur_skip_to_eol(c); ctoon_cur_skip_nl(c);
        if (!ctoon_parse_tabular(c, arr_depth + 1, expected, fields, nfields))
            return false;
    } else if (!ctoon_cur_at_eol(c)) {
        /* inline primitives */
        char stop[2] = { c->delim, '\0' };
        while (!ctoon_cur_at_eol(c)) {
            ctoon_cur_skip_spaces(c);
            if (ctoon_cur_at_eol(c)) break;
            ctoon_val *vv = ctoon_read_vpool_alloc(&c->vp);
            if (!vv)
                return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
            if (!ctoon_parse_str_raw(c, vv, stop)) return false;
            ctoon_ctn_child_added(c);
            ctoon_cur_skip_spaces(c);
            if (ctoon_cur_peek(c) == (u8)c->delim) {
                c->cur++; ctoon_cur_skip_spaces(c);
            } else break;
        }
    } else {
        /* list items on next lines */
        ctoon_cur_skip_nl(c);
        if (!ctoon_parse_list(c, arr_depth + 1)) return false;
    }

    ctoon_ctn_close(c, CTOON_TYPE_ARR);
    return true;
}

/*============================================================================
 * Object (key: value per indented line)
 *===========================================================================*/

static bool ctoon_parse_object(ctoon_read_ctx *c, int depth) {
    if (!ctoon_ctn_open(c, CTOON_TYPE_OBJ)) return false;

    while (!ctoon_cur_at_eof(c)) {
        /* skip blank lines */
        bool blank = true;
        for (const u8 *bl = c->cur;
             bl < c->eof && *bl != '\n' && *bl != '\r'; bl++)
            if (*bl != ' ') { blank = false; break; }
        if (blank) { ctoon_cur_skip_to_eol(c); ctoon_cur_skip_nl(c); continue; }

        /* skip comment lines */
        const u8 *tmp = c->cur;
        while (tmp < c->eof && *tmp == ' ') tmp++;
        if (tmp < c->eof && *tmp == '#') {
            ctoon_cur_skip_to_eol(c); ctoon_cur_skip_nl(c); continue;
        }

        int ind = ctoon_cur_measure_indent(c);
        if (ind < depth) break;               /* dedent → end of this object */
        if (ind > depth) {                    /* over-indented → skip */
            ctoon_cur_skip_to_eol(c); ctoon_cur_skip_nl(c); continue;
        }
        if (!ctoon_cur_consume_indent(c, depth)) break;

        /* key */
        ctoon_val *kv = ctoon_read_vpool_alloc(&c->vp);
        if (!kv)
            return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        if (!ctoon_parse_key(c, kv)) return false;
        ctoon_cur_skip_spaces(c);

        if (ctoon_cur_peek(c) == '[') {
            c->cur++;
            if (!ctoon_parse_array(c, depth)) return false;
        } else if (ctoon_cur_peek(c) == ':') {
            c->cur++;
            ctoon_cur_skip_spaces(c);
            if (ctoon_cur_at_eol(c)) {
                ctoon_cur_skip_nl(c);
                if (!ctoon_parse_object(c, depth + 1)) return false;
            } else {
                ctoon_val *vv = ctoon_read_vpool_alloc(&c->vp);
                if (!vv)
                    return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION,
                                    MSG_MALLOC);
                if (!ctoon_parse_str_raw(c, vv, NULL)) return false;
                ctoon_cur_skip_to_eol(c); ctoon_cur_skip_nl(c);
            }
        } else {
            return ctoon_read_set_err(c, CTOON_READ_ERROR_TOON_STRUCTURE,
                            "expected ':' or '[' after key");
        }
        ctoon_ctn_stack_top(&c->st)->count++; /* one kv-pair */
    }

    ctoon_ctn_close(c, CTOON_TYPE_OBJ);
    return true;
}

/*============================================================================
 * File reading helper
 *===========================================================================*/

static char *ctoon_read_file_to_buf(FILE *file, usize *out_len, ctoon_alc alc) {
    long fsize = 0;
    long fpos  = ftell(file);
    if (fpos >= 0) {
        if (fseek(file, 0, SEEK_END) == 0) fsize = ftell(file);
        if (fseek(file, fpos, SEEK_SET) != 0) fsize = 0;
        if (fsize > 0) fsize -= fpos;
    }
    void *buf = NULL;
    usize used = 0;
    if (fsize > 0) {
        usize alloc = (usize)fsize + CTOON_PADDING_SIZE;
        buf = alc.malloc(alc.ctx, alloc);
        if (!buf) return NULL;
        if (ctoon_sys_fread(buf, (usize)fsize, file) != (usize)fsize) {
            alc.free(alc.ctx, buf); return NULL;
        }
        used = (usize)fsize;
    } else {
        usize cap   = CTOON_PADDING_SIZE;
        usize chunk = 65536;
        while (true) {
            usize nc  = cap + chunk;
            void *tmp = buf ? alc.realloc(alc.ctx, buf, cap, nc)
                            : alc.malloc(alc.ctx, nc);
            if (!tmp) { alc.free(alc.ctx, buf); return NULL; }
            buf = tmp; cap = nc;
            usize rd = ctoon_sys_fread((u8 *)buf + used, chunk, file);
            used += rd;
            if (rd != chunk) { buf = alc.realloc(alc.ctx, buf, cap,
                                    used + CTOON_PADDING_SIZE); cap = used + CTOON_PADDING_SIZE; break; }
            if (chunk < 8u * 1024u * 1024u) chunk *= 2;
        }
    }
    memset((u8 *)buf + used, 0, CTOON_PADDING_SIZE);
    *out_len = used;
    return (char *)buf;
}

/*============================================================================
 * Document builder
 *===========================================================================*/

static ctoon_doc *ctoon_read_build_doc(char *dat, usize len,
                                 ctoon_read_flag flags,
                                 ctoon_alc alc,
                                 ctoon_read_err *err) {
    bool insitu = ctoon_read_has_flag(flags, CTOON_READ_INSITU);

    ctoon_read_ctx c;
    memset(&c, 0, sizeof(c));
    c.flags  = flags;
    c.indent = 2;
    c.delim  = ',';
    c.err    = err;

    /* ---- padded input buffer ---- */
    if (insitu) {
        c.hdr = (const u8 *)dat;
        c.eof = (const u8 *)dat + len;
        c.cur = c.hdr;
    } else {
        if (len >= USIZE_MAX - CTOON_PADDING_SIZE) {
            ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
            return NULL;
        }
        u8 *buf = (u8 *)alc.malloc(alc.ctx, len + CTOON_PADDING_SIZE);
        if (!buf) {
            ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
            return NULL;
        }
        memcpy(buf, dat, len);
        memset(buf + len, 0, CTOON_PADDING_SIZE);
        c.hdr = buf; c.eof = buf + len; c.cur = buf;
    }

    /* ---- val pool ---- */
    usize hdr_slots = (sizeof(ctoon_doc) + sizeof(ctoon_val) - 1)
                      / sizeof(ctoon_val);
    usize est_vals  = len / 8 + 8;
    c.hdr_slots = hdr_slots;
    if (!ctoon_read_vpool_init(&c.vp, alc, hdr_slots, est_vals)) {
        ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        goto fail_input;
    }

    /* ---- container stack ---- */
    if (!ctoon_ctn_stack_init(&c.st, alc)) {
        ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        goto fail_vpool;
    }
    /* sentinel frame points at the root slot */
    c.st.frames[0] = (ctoon_ctn_frame){ hdr_slots, 0 };

    /* ---- string pool ---- */
    if (!ctoon_str_pool_buf_init(&c.sp, alc, len)) {
        ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        goto fail_stack;
    }

    /* ---- BOM ---- */
    if (ctoon_read_has_flag(flags, CTOON_READ_ALLOW_BOM)
        && len >= 3 && is_utf8_bom(c.cur))
        c.cur += 3;

    /* ---- skip leading blank / comment lines ---- */
    while (c.cur < c.eof) {
        bool blank = true;
        for (const u8 *bl = c.cur;
             bl < c.eof && *bl != '\n' && *bl != '\r'; bl++)
            if (*bl != ' ') { blank = false; break; }
        if (!blank) {
            const u8 *tmp = c.cur;
            while (tmp < c.eof && *tmp == ' ') tmp++;
            if (tmp < c.eof && *tmp == '#') {
                ctoon_cur_skip_to_eol(&c); ctoon_cur_skip_nl(&c); continue;
            }
            break;
        }
        ctoon_cur_skip_to_eol(&c); ctoon_cur_skip_nl(&c);
    }

    /* ---- empty document → empty object ---- */
    if (c.cur >= c.eof) {
        ctoon_val *rv = ctoon_read_vpool_alloc(&c.vp);
        if (!rv) {
            ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
            goto fail_spool;
        }
        rv->tag     = (u64)CTOON_TYPE_OBJ;
        rv->uni.ofs = sizeof(ctoon_val);
        goto done;
    }

    /* ---- dispatch on first real line ---- */
    {
        bool has_colon = false;
        bool in_quotes = false;
        for (const u8 *sc = c.cur;
             sc < c.eof && *sc != '\n' && *sc != '\r'; sc++) {
            if (*sc == '"') { in_quotes = !in_quotes; continue; }
            if (!in_quotes && *sc == ':') { has_colon = true; break; }
        }
        if (*c.cur == '[') {
            c.cur++;
            if (!ctoon_parse_array(&c, 0)) goto fail_spool;
        } else if (has_colon) {
            if (!ctoon_parse_object(&c, 0)) goto fail_spool;
        } else {
            ctoon_val *rv = ctoon_read_vpool_alloc(&c.vp);
            if (!rv) {
                ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
                goto fail_spool;
            }
            if (!ctoon_parse_str_raw(&c, rv, NULL)) goto fail_spool;
        }
    }

done:;
    /* free the padded input copy; strings live in the spool now */
    if (!insitu) alc.free(alc.ctx, (void *)c.hdr);
    ctoon_ctn_stack_free(&c.st);

    ctoon_doc *doc = (ctoon_doc *)(void *)c.vp.base;
    memset(doc, 0, sizeof(ctoon_doc));
    doc->root      = c.vp.base + hdr_slots;
    doc->alc       = alc;
    doc->dat_read  = (usize)(c.cur - c.hdr);
    doc->val_read  = (usize)(c.vp.cur - doc->root);
    doc->str_pool  = c.sp.buf;   /* ownership transferred; freed by ctoon_doc_free */

    /* trim val pool to actual usage */
    usize used = (usize)(c.vp.cur - c.vp.base);
    ctoon_val *trimmed = (ctoon_val *)alc.realloc(
        alc.ctx, c.vp.base,
        c.vp.cap * sizeof(ctoon_val),
        used      * sizeof(ctoon_val));
    if (trimmed) {
        doc           = (ctoon_doc *)(void *)trimmed;
        doc->root     = trimmed + hdr_slots;
    }
    if (err) memset(err, 0, sizeof(*err));
    return doc;

fail_spool:
    ctoon_str_pool_buf_free(&c.sp, alc);
fail_stack:
    ctoon_ctn_stack_free(&c.st);
fail_vpool:
    alc.free(alc.ctx, c.vp.base);
fail_input:
    if (!insitu) alc.free(alc.ctx, (void *)c.hdr);
    return NULL;
}

/*============================================================================
 * Public API
 *===========================================================================*/

ctoon_doc *ctoon_read_opts(char *dat, usize len,
                             ctoon_read_flag flg,
                             const ctoon_alc *alc_ptr,
                             ctoon_read_err *err) {
    ctoon_read_err tmp;
    if (!err) err = &tmp;
    ctoon_alc alc = alc_ptr ? *alc_ptr : CTOON_DEFAULT_ALC;
    if (!dat) {
        err->code = CTOON_READ_ERROR_INVALID_PARAMETER;
        err->msg  = "input is NULL";
        err->pos  = 0;
        return NULL;
    }
    if (!len) {
        err->code = CTOON_READ_ERROR_INVALID_PARAMETER;
        err->msg  = "length is 0";
        err->pos  = 0;
        return NULL;
    }
    return ctoon_read_build_doc(dat, len, flg, alc, err);
}

ctoon_doc *ctoon_read_file(const char *path,
                             ctoon_read_flag flg,
                             const ctoon_alc *alc_ptr,
                             ctoon_read_err *err) {
    ctoon_read_err tmp;
    if (!err) err = &tmp;
    if (!path) {
        err->code = CTOON_READ_ERROR_INVALID_PARAMETER;
        err->msg  = "path is NULL";
        err->pos  = 0;
        return NULL;
    }
    FILE *f = fopen_safe(path, "rb");
    if (!f) {
        err->code = CTOON_READ_ERROR_FILE_OPEN;
        err->msg  = "cannot open file";
        err->pos  = 0;
        return NULL;
    }
    ctoon_doc *doc = ctoon_read_fp(f, flg, alc_ptr, err);
    fclose(f);
    return doc;
}

ctoon_doc *ctoon_read_fp(FILE *file,
                           ctoon_read_flag flg,
                           const ctoon_alc *alc_ptr,
                           ctoon_read_err *err) {
    ctoon_read_err tmp;
    if (!err) err = &tmp;
    ctoon_alc alc = alc_ptr ? *alc_ptr : CTOON_DEFAULT_ALC;
    if (!file) {
        err->code = CTOON_READ_ERROR_INVALID_PARAMETER;
        err->msg  = "file is NULL";
        err->pos  = 0;
        return NULL;
    }
    usize len  = 0;
    char *buf  = ctoon_read_file_to_buf(file, &len, alc);
    if (!buf) {
        err->code = CTOON_READ_ERROR_FILE_READ;
        err->msg  = "cannot read file";
        err->pos  = 0;
        return NULL;
    }
    /* build_doc copies the input into its padded buffer and frees buf here. */
    ctoon_doc *doc = ctoon_read_build_doc(buf, len,
                                    flg & (ctoon_read_flag)~CTOON_READ_INSITU,
                                    alc, err);
    alc.free(alc.ctx, buf);
    return doc;
}


/*==============================================================================
 * MARK: - JSON Reader (Public)
 *
 * Parses RFC 8259 JSON text directly into the flat ctoon_val pool.
 * The resulting ctoon_doc is identical in layout to one produced by the
 * TOON reader — the same API (ctoon_get_str, ctoon_arr_iter, …) works on
 * both without any conversion step.
 *
 * Reuses all reader infrastructure: ctoon_read_ctx, ctoon_ctn_open/close,
 * ctoon_read_vpool, ctoon_str_pool_buf, ctoon_parse_str_quoted.
 *============================================================================*/

#if !CTOON_DISABLE_JSON

/* ---- JSON whitespace skip ------------------------------------------------ */

static void cj_skip_ws(ctoon_read_ctx *c) {
    while (c->cur < c->eof) {
        u8 ch = *c->cur;
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') c->cur++;
        else break;
    }
}

/* ---- JSON number parser -------------------------------------------------- */

static bool cj_parse_number(ctoon_read_ctx *c, ctoon_val *val) {
    const u8 *start = c->cur;
    bool neg = false;

    if (c->cur < c->eof && *c->cur == '-') { neg = true; c->cur++; }

    /* integer digits */
    if (c->cur >= c->eof || (u8)(*c->cur - '0') > 9)
        return ctoon_read_set_err(c, CTOON_READ_ERROR_INVALID_NUMBER, "invalid number");

    u64 uint_val = 0;
    bool overflow = false;
    while (c->cur < c->eof && (unsigned)(*c->cur - '0') <= 9) {
        u8 d = (u8)(*c->cur++ - '0');
        if (!overflow) {
            if (uint_val > (U64_MAX - d) / 10) overflow = true;
            else uint_val = uint_val * 10 + d;
        }
    }

    /* if we hit '.', 'e', 'E' or overflow → float */
    bool is_float = overflow ||
                    (c->cur < c->eof && (*c->cur == '.' || *c->cur == 'e' || *c->cur == 'E'));

    if (is_float) {
        /* skip fractional and exponent parts */
        if (c->cur < c->eof && *c->cur == '.') {
            c->cur++;
            while (c->cur < c->eof && (unsigned)(*c->cur - '0') <= 9) c->cur++;
        }
        if (c->cur < c->eof && (*c->cur == 'e' || *c->cur == 'E')) {
            c->cur++;
            if (c->cur < c->eof && (*c->cur == '+' || *c->cur == '-')) c->cur++;
            while (c->cur < c->eof && (unsigned)(*c->cur - '0') <= 9) c->cur++;
        }
        /* parse via strtod for correctness */
        usize tlen = (usize)(c->cur - start);
        if (tlen >= 64)
            return ctoon_read_set_err(c, CTOON_READ_ERROR_INVALID_NUMBER, "number too long");
        char tmp[64];
        memcpy(tmp, start, tlen); tmp[tlen] = '\0';
        char *end = NULL; errno = 0;
        double dv = strtod(tmp, &end);
        if (end != tmp + tlen || errno != 0)
            return ctoon_read_set_err(c, CTOON_READ_ERROR_INVALID_NUMBER, "invalid number");
        val->tag = CTOON_TYPE_NUM | CTOON_SUBTYPE_REAL;
        val->uni.f64 = dv;
        return true;
    }

    /* integer */
    if (neg) {
        /* negate: if uint_val > INT64_MAX+1 it overflows i64 */
        if (uint_val > (u64)INT64_MAX + 1) {
            /* store as real */
            usize tlen = (usize)(c->cur - start);
            char tmp[64]; memcpy(tmp, start, tlen); tmp[tlen] = '\0';
            char *end = NULL;
            double dv = strtod(tmp, &end);
            val->tag = CTOON_TYPE_NUM | CTOON_SUBTYPE_REAL;
            val->uni.f64 = dv;
            return true;
        }
        val->tag = CTOON_TYPE_NUM | CTOON_SUBTYPE_SINT;
        val->uni.i64 = -(i64)uint_val;
    } else {
        val->tag = CTOON_TYPE_NUM | CTOON_SUBTYPE_UINT;
        val->uni.u64 = uint_val;
    }
    return true;
}

/* ---- JSON value (forward-declared) --------------------------------------- */

static bool cj_parse_value(ctoon_read_ctx *c);

/* ---- JSON array ---------------------------------------------------------- */

static bool cj_parse_array(ctoon_read_ctx *c) {
    c->cur++; /* skip '[' */
    if (!ctoon_ctn_open(c, CTOON_TYPE_ARR)) return false;
    cj_skip_ws(c);
    if (c->cur < c->eof && *c->cur == ']') { c->cur++; goto arr_done; }
    for (;;) {
        if (!cj_parse_value(c)) return false;
        ctoon_ctn_child_added(c);
        cj_skip_ws(c);
        if (c->cur >= c->eof)
            return ctoon_read_set_err(c, CTOON_READ_ERROR_TOON_STRUCTURE, "unterminated array");
        if (*c->cur == ']') { c->cur++; break; }
        if (*c->cur != ',')
            return ctoon_read_set_err(c, CTOON_READ_ERROR_UNEXPECTED_CHARACTER,
                                       "expected ',' or ']'");
        c->cur++; /* skip ',' */
        cj_skip_ws(c);
    }
arr_done:
    ctoon_ctn_close(c, CTOON_TYPE_ARR);
    return true;
}

/* ---- JSON object --------------------------------------------------------- */

static bool cj_parse_object(ctoon_read_ctx *c) {
    c->cur++; /* skip '{' */
    if (!ctoon_ctn_open(c, CTOON_TYPE_OBJ)) return false;
    cj_skip_ws(c);
    if (c->cur < c->eof && *c->cur == '}') { c->cur++; goto obj_done; }
    for (;;) {
        cj_skip_ws(c);
        if (c->cur >= c->eof || *c->cur != '"')
            return ctoon_read_set_err(c, CTOON_READ_ERROR_UNEXPECTED_CHARACTER,
                                       "expected '\"' for key");
        /* key */
        ctoon_val *kv = ctoon_read_vpool_alloc(&c->vp);
        if (!kv) return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        if (!ctoon_parse_str_quoted(c, kv)) return false;
        /* colon */
        cj_skip_ws(c);
        if (c->cur >= c->eof || *c->cur != ':')
            return ctoon_read_set_err(c, CTOON_READ_ERROR_UNEXPECTED_CHARACTER,
                                       "expected ':'");
        c->cur++;
        cj_skip_ws(c);
        /* value */
        if (!cj_parse_value(c)) return false;
        ctoon_ctn_child_added(c); /* one kv-pair */
        cj_skip_ws(c);
        if (c->cur >= c->eof)
            return ctoon_read_set_err(c, CTOON_READ_ERROR_TOON_STRUCTURE, "unterminated object");
        if (*c->cur == '}') { c->cur++; break; }
        if (*c->cur != ',')
            return ctoon_read_set_err(c, CTOON_READ_ERROR_UNEXPECTED_CHARACTER,
                                       "expected ',' or '}'");
        c->cur++; /* skip ',' */
    }
obj_done:
    ctoon_ctn_close(c, CTOON_TYPE_OBJ);
    return true;
}

/* ---- JSON value dispatcher ----------------------------------------------- */

static bool cj_parse_value(ctoon_read_ctx *c) {
    cj_skip_ws(c);
    if (c->cur >= c->eof)
        return ctoon_read_set_err(c, CTOON_READ_ERROR_UNEXPECTED_CHARACTER,
                                   "unexpected end of input");
    u8 ch = *c->cur;

    /* string */
    if (ch == '"') {
        ctoon_val *vv = ctoon_read_vpool_alloc(&c->vp);
        if (!vv) return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        return ctoon_parse_str_quoted(c, vv);
    }
    /* object */
    if (ch == '{') return cj_parse_object(c);
    /* array */
    if (ch == '[') return cj_parse_array(c);
    /* number */
    if (ch == '-' || (ch >= '0' && ch <= '9')) {
        ctoon_val *vv = ctoon_read_vpool_alloc(&c->vp);
        if (!vv) return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        return cj_parse_number(c, vv);
    }
    /* true */
    if (c->eof - c->cur >= 4
        && c->cur[0]=='t' && c->cur[1]=='r' && c->cur[2]=='u' && c->cur[3]=='e') {
        ctoon_val *vv = ctoon_read_vpool_alloc(&c->vp);
        if (!vv) return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        vv->tag = CTOON_TYPE_BOOL | CTOON_SUBTYPE_TRUE; vv->uni.u64 = 0;
        c->cur += 4; return true;
    }
    /* false */
    if (c->eof - c->cur >= 5
        && c->cur[0]=='f' && c->cur[1]=='a' && c->cur[2]=='l'
        && c->cur[3]=='s' && c->cur[4]=='e') {
        ctoon_val *vv = ctoon_read_vpool_alloc(&c->vp);
        if (!vv) return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        vv->tag = CTOON_TYPE_BOOL | CTOON_SUBTYPE_FALSE; vv->uni.u64 = 0;
        c->cur += 5; return true;
    }
    /* null */
    if (c->eof - c->cur >= 4
        && c->cur[0]=='n' && c->cur[1]=='u' && c->cur[2]=='l' && c->cur[3]=='l') {
        ctoon_val *vv = ctoon_read_vpool_alloc(&c->vp);
        if (!vv) return ctoon_read_set_err(c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        vv->tag = CTOON_TYPE_NULL; vv->uni.u64 = 0;
        c->cur += 4; return true;
    }
    return ctoon_read_set_err(c, CTOON_READ_ERROR_UNEXPECTED_CHARACTER,
                               "unexpected character");
}

/* ---- JSON document builder ---------------------------------------------- */

static ctoon_doc *cj_build_doc(char *dat, usize len,
                                ctoon_read_flag flags,
                                ctoon_alc alc,
                                ctoon_read_err *err) {
    /* Reuse ctoon_read_build_doc's setup by constructing our own ctx.
       We cannot call ctoon_read_build_doc directly because it dispatches
       to the TOON parser; instead we duplicate its setup/teardown here,
       sharing all the pool helpers. */
    ctoon_read_ctx c;
    memset(&c, 0, sizeof(c));
    c.flags = flags;
    c.err   = err;

    bool insitu = ctoon_has_flag(flags, CTOON_READ_INSITU);

    /* padded input copy */
    if (insitu) {
        c.hdr = (const u8 *)dat; c.eof = (const u8 *)dat + len; c.cur = c.hdr;
    } else {
        if (len >= USIZE_MAX - CTOON_PADDING_SIZE) {
            ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
            return NULL;
        }
        u8 *buf = (u8 *)alc.malloc(alc.ctx, len + CTOON_PADDING_SIZE);
        if (!buf) { ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC); return NULL; }
        memcpy(buf, dat, len); memset(buf + len, 0, CTOON_PADDING_SIZE);
        c.hdr = buf; c.eof = buf + len; c.cur = buf;
    }

    /* val pool */
    usize hdr_slots = (sizeof(ctoon_doc) + sizeof(ctoon_val) - 1) / sizeof(ctoon_val);
    usize est_vals  = len / 8 + 8;
    c.hdr_slots = hdr_slots;
    if (!ctoon_read_vpool_init(&c.vp, alc, hdr_slots, est_vals)) {
        ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        goto fail_input;
    }

    /* container stack */
    if (!ctoon_ctn_stack_init(&c.st, alc)) {
        ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        goto fail_vpool;
    }
    c.st.frames[0] = (ctoon_ctn_frame){ hdr_slots, 0 };

    /* string pool */
    if (!ctoon_str_pool_buf_init(&c.sp, alc, len)) {
        ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC);
        goto fail_stack;
    }

    /* skip BOM */
    if (ctoon_has_flag(flags, CTOON_READ_ALLOW_BOM)
        && len >= 3 && is_utf8_bom(c.cur)) c.cur += 3;

    /* skip leading whitespace */
    cj_skip_ws(&c);

    if (c.cur >= c.eof) {
        /* empty → empty object */
        ctoon_val *rv = ctoon_read_vpool_alloc(&c.vp);
        if (!rv) { ctoon_read_set_err(&c, CTOON_READ_ERROR_MEMORY_ALLOCATION, MSG_MALLOC); goto fail_spool; }
        rv->tag     = (u64)CTOON_TYPE_OBJ;
        rv->uni.ofs = sizeof(ctoon_val);
        goto done;
    }

    if (!cj_parse_value(&c)) goto fail_spool;

    /* skip trailing whitespace and ensure nothing follows */
    cj_skip_ws(&c);
    if (c.cur < c.eof) {
        ctoon_read_set_err(&c, CTOON_READ_ERROR_UNEXPECTED_CHARACTER,
                            "trailing garbage after JSON value");
        goto fail_spool;
    }

done:;
    ctoon_ctn_stack_free(&c.st);
    if (!insitu) alc.free(alc.ctx, (void *)c.hdr);

    ctoon_doc *doc = (ctoon_doc *)(void *)c.vp.base;
    memset(doc, 0, sizeof(ctoon_doc));
    doc->root      = c.vp.base + hdr_slots;
    doc->alc       = alc;
    doc->dat_read  = (usize)(c.cur - c.hdr);
    doc->val_read  = (usize)(c.vp.cur - doc->root);
    doc->str_pool  = c.sp.buf;

    /* trim val pool */
    {
        usize used = (usize)(c.vp.cur - c.vp.base);
        ctoon_val *t = (ctoon_val *)alc.realloc(alc.ctx, c.vp.base,
            c.vp.cap * sizeof(ctoon_val), used * sizeof(ctoon_val));
        if (t) { doc = (ctoon_doc *)(void *)t; doc->root = t + hdr_slots; }
    }
    if (err) memset(err, 0, sizeof(*err));
    return doc;

fail_spool:
    ctoon_str_pool_buf_free(&c.sp, alc);
fail_stack:
    ctoon_ctn_stack_free(&c.st);
fail_vpool:
    alc.free(alc.ctx, c.vp.base);
fail_input:
    if (!insitu) alc.free(alc.ctx, (void *)c.hdr);
    return NULL;
}

/* ---- Public API ---------------------------------------------------------- */

ctoon_doc *ctoon_read_json(char *dat, usize len,
                            ctoon_read_flag flg,
                            const ctoon_alc *alc_ptr,
                            ctoon_read_err *err) {
    ctoon_read_err tmp;
    if (!err) err = &tmp;
    ctoon_alc alc = alc_ptr ? *alc_ptr : CTOON_DEFAULT_ALC;
    if (!dat) {
        err->code = CTOON_READ_ERROR_INVALID_PARAMETER; err->msg = "input is NULL"; err->pos = 0;
        return NULL;
    }
    if (!len) {
        err->code = CTOON_READ_ERROR_INVALID_PARAMETER; err->msg = "length is 0"; err->pos = 0;
        return NULL;
    }
    return cj_build_doc(dat, len, flg, alc, err);
}

ctoon_doc *ctoon_read_json_file(const char *path,
                                 ctoon_read_flag flg,
                                 const ctoon_alc *alc_ptr,
                                 ctoon_read_err *err) {
    ctoon_read_err tmp;
    if (!err) err = &tmp;
    if (!path) {
        err->code = CTOON_READ_ERROR_INVALID_PARAMETER; err->msg = "path is NULL"; err->pos = 0;
        return NULL;
    }
    FILE *f = fopen_safe(path, "rb");
    if (!f) {
        err->code = CTOON_READ_ERROR_FILE_OPEN; err->msg = "cannot open file"; err->pos = 0;
        return NULL;
    }
    ctoon_doc *doc = ctoon_read_json_fp(f, flg, alc_ptr, err);  /* file helper */
    fclose(f);
    return doc;
}

ctoon_doc *ctoon_read_json_fp(FILE *file,
                               ctoon_read_flag flg,
                               const ctoon_alc *alc_ptr,
                               ctoon_read_err *err) {
    ctoon_read_err tmp;
    if (!err) err = &tmp;
    ctoon_alc alc = alc_ptr ? *alc_ptr : CTOON_DEFAULT_ALC;
    if (!file) {
        err->code = CTOON_READ_ERROR_INVALID_PARAMETER; err->msg = "file is NULL"; err->pos = 0;
        return NULL;
    }
    usize len  = 0;
    char *buf  = ctoon_read_file_to_buf(file, &len, alc);
    if (!buf) {
        err->code = CTOON_READ_ERROR_FILE_READ; err->msg = "cannot read file"; err->pos = 0;
        return NULL;
    }
    ctoon_doc *doc = cj_build_doc(buf, len,
                                   flg & (ctoon_read_flag)~CTOON_READ_INSITU, alc, err);
    alc.free(alc.ctx, buf);
    return doc;
}

#endif /* CTOON_DISABLE_JSON */

#endif /* CTOON_DISABLE_READER */

/*==============================================================================
 * MARK: - TOON Writer (Private)
 *
 * Serialises a flat ctoon_val tree (yyjson layout) to TOON text.
 *
 * Traversal:
 *   - container first child  = val + 1
 *   - container next sibling = (ctoon_val*)((u8*)val + val->uni.ofs)
 *   - leaf       next val    = val + 1
 *   - child count            = val->tag >> TAG_BIT
 *                            same for ARR and OBJ
 *
 * The writer owns a growable heap buffer (tw struct).  On success it hands
 * ownership to the caller; on error it frees internally.
 *============================================================================*/

#if !CTOON_DISABLE_WRITER /* writer begin */

/*==============================================================================
 * MARK: - Integer / Float Writer (Private)
 *
 * Highly optimised integer-to-string conversion identical to yyjson's.
 * We keep the same digit table trick for speed.
 *============================================================================*/

/** Digit table: 0..99 → 2-char ASCII (little-endian for memcpy). */
static const u8 digit_table[200] = {
    '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
    '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
    '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
    '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
    '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
    '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
    '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
    '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
    '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
    '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9'
};

/** Write uint64 to buf (no NUL), returns pointer past last byte. */
static_inline u8 *write_u64(u64 val, u8 *buf) {
    u8 tmp[20]; int len = 0;
    do {
        u64 q = val / 100;
        u32 r = (u32)(val % 100) * 2;
        val = q;
        tmp[len++] = digit_table[r + 1];
        tmp[len++] = digit_table[r];
    } while (val);
    /* last byte might be a leading zero from the pair */
    if (len > 1 && tmp[len-1] == '0') len--;
    for (int i = len - 1; i >= 0; i--) *buf++ = tmp[i];
    return buf;
}

/** Write int64 to buf (no NUL). */
static_inline u8 *write_i64(i64 val, u8 *buf) {
    if (val < 0) { *buf++ = '-'; return write_u64((u64)(-(val + 1)) + 1, buf); }
    return write_u64((u64)val, buf);
}

/*==============================================================================
 * MARK: - Writer Context (Private)
 *============================================================================*/

typedef struct {
    u8    *buf;      /* output buffer                        */
    usize  len;      /* bytes written                        */
    usize  cap;      /* bytes allocated                      */
    /* resolved options */
    char   delim;    /* ',' '\t' or '|'                      */
    int    indent;   /* spaces per indent level (0 = no indent, but TOON always needs ≥1) */
    bool   lmarker;  /* prepend '#' length marker            */
    bool   esc_uni;  /* escape non-ASCII as \uXXXX           */
    bool   esc_slsh; /* escape '/' as '\/'                   */
    bool   inf_null; /* write inf/nan as null                */
    bool   inf_lit;  /* write inf/nan as Infinity/NaN        */
    ctoon_write_flag flg;
    ctoon_alc alc;
} ctoon_write_ctx;

/*---- buffer growth ---------------------------------------------------------*/

static_noinline bool ctoon_write_buf_grow(ctoon_write_ctx *w, usize need) {
    usize nc = w->cap ? w->cap * 2 : 512;
    while (nc < w->len + need) nc *= 2;
    u8 *nb = (u8 *)w->alc.realloc(w->alc.ctx, w->buf,
                                    w->cap, nc);
    if (unlikely(!nb)) return false;
    w->buf = nb; w->cap = nc;
    return true;
}

static_inline bool ctoon_write_buf_reserve(ctoon_write_ctx *w, usize n) {
    if (likely(w->len + n <= w->cap)) return true;
    return ctoon_write_buf_grow(w, n);
}

static_inline bool ctoon_write_char(ctoon_write_ctx *w, u8 c) {
    if (unlikely(w->len + 1 > w->cap) && !ctoon_write_buf_grow(w, 1)) return false;
    w->buf[w->len++] = c;
    return true;
}

static_inline bool ctoon_write_bytes(ctoon_write_ctx *w, const u8 *s, usize n) {
    if (!ctoon_write_buf_reserve(w, n)) return false;
    memcpy(w->buf + w->len, s, n);
    w->len += n;
    return true;
}

static_inline bool ctoon_write_str_lit(ctoon_write_ctx *w, const char *s) {
    return ctoon_write_bytes(w, (const u8 *)s, strlen(s));
}

static_inline bool ctoon_write_indent(ctoon_write_ctx *w, int depth) {
    int sp = depth * w->indent;
    if (!ctoon_write_buf_reserve(w, (usize)sp)) return false;
    memset(w->buf + w->len, ' ', (usize)sp);
    w->len += (usize)sp;
    return true;
}

/*---- number writer ---------------------------------------------------------*/

static_noinline bool ctoon_write_num(ctoon_write_ctx *w, const ctoon_val *val) {
    /* need at most 40 bytes: sign + 19 digits + dot + 15 frac + e+308 */
    if (!ctoon_write_buf_reserve(w, 40)) return false;
    u8 *p = w->buf + w->len;
    u8 type = (u8)(val->tag & CTOON_TAG_MASK);

    if ((type & CTOON_SUBTYPE_MASK) == CTOON_SUBTYPE_UINT) {
        p = write_u64(val->uni.u64, p);
    } else if ((type & CTOON_SUBTYPE_MASK) == CTOON_SUBTYPE_SINT) {
        p = write_i64(val->uni.i64, p);
    } else {
        double d = val->uni.f64;
        if (unlikely(!isfinite(d))) {
            if (w->inf_null) {
                if (!ctoon_write_str_lit(w, "null")) return false;
                return true;
            }
            if (w->inf_lit) {
                if (isinf(d)) {
                    const char *s = (d > 0) ? "Infinity" : "-Infinity";
                    return ctoon_write_str_lit(w, s);
                }
                return ctoon_write_str_lit(w, "NaN");
            }
            /* error: NaN/Inf not allowed */
            return false;
        }
        /* check fp format flags */
        ctoon_write_flag fp_flags = w->flg >> (32 - CTOON_WRITE_FP_FLAG_BITS);
        if (fp_flags & (1u << (CTOON_WRITE_FP_PREC_BITS))) {
            /* CTOON_WRITE_FP_TO_FLOAT: cast to float first */
            d = (double)(float)d;
        }
        u32 fp_prec_bits = (w->flg >> (32 - CTOON_WRITE_FP_PREC_BITS)) & 0xF;
        if (fp_prec_bits) {
            /* CTOON_WRITE_FP_TO_FIXED(prec) */
            int prec = (int)fp_prec_bits;
            int n = snprintf((char *)p, 32, "%.*f", prec, d);
            p += (n > 0) ? (usize)n : 0;
        } else {
            /* default: shortest round-trip representation */
            if (d == (double)(long long)d && d >= -1e15 && d <= 1e15) {
                p = write_i64((i64)d, p);
            } else {
                int n = snprintf((char *)p, 32, "%.15g", d);
                p += (n > 0) ? (usize)n : 0;
            }
        }
    }
    w->len = (usize)(p - w->buf);
    return true;
}

/*---- string writer ---------------------------------------------------------*/

/*
 * Determine if a string needs quoting in a TOON context:
 * - is_key=true : quoting needed if contains ':', '[', '{', '"', '\\', '\n' '\r' '\t'
 *                 or starts/ends with space, or starts with "- "
 * - is_key=false: additionally quote if matches null/true/false/number,
 *                 or contains delimiter char, ']', '}'
 *
 * Return true → must quote.
 */
static_inline bool ctoon_write_str_needs_quote(ctoon_write_ctx *w, const char *s, usize len,
                                      bool is_key) {
    if (len == 0) return true;
    if (s[0] == ' ' || s[len-1] == ' ') return true;
    if (len >= 2 && s[0] == '-' && s[1] == ' ') return true;

    for (usize i = 0; i < len; i++) {
        u8 c = (u8)s[i];
        if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t')
            return true;
        if (is_key && (c == ':' || c == '[' || c == '{'))
            return true;
        if (!is_key && (c == (u8)w->delim || c == ':' ||
                         c == '[' || c == ']' || c == '{' || c == '}'))
            return true;
        /* high-bit bytes: if ESCAPE_UNICODE flag is set we must quote */
        if (c >= 0x80 && w->esc_uni) return true;
    }

    if (!is_key) {
        /* quote if looks like null/true/false/number (would be mis-read) */
        if (len == 4 && memcmp(s, "null",  4) == 0) return true;
        if (len == 4 && memcmp(s, "true",  4) == 0) return true;
        if (len == 5 && memcmp(s, "false", 5) == 0) return true;
        /* number check */
        if (len < 32) {
            char tmp[32]; memcpy(tmp, s, len); tmp[len] = '\0';
            char *end = NULL;
            strtod(tmp, &end);
            if (end == tmp + len) return true;
        }
    }
    return false;
}

static_noinline bool ctoon_write_str(ctoon_write_ctx *w, const char *s, usize len,
                                    bool is_key) {
    if (!ctoon_write_str_needs_quote(w, s, len, is_key)) {
        return ctoon_write_bytes(w, (const u8 *)s, len);
    }
    /* quoted */
    if (!ctoon_write_char(w, '"')) return false;
    const u8 *p   = (const u8 *)s;
    const u8 *end = p + len;
    while (p < end) {
        u8 c = *p;
        if (likely(c >= 0x20 && c != '"' && c != '\\' && !(c >= 0x80 && w->esc_uni))) {
            /* fast-path: copy a run of safe bytes */
            const u8 *run = p;
            while (p < end && *p >= 0x20 && *p != '"' && *p != '\\'
                   && !(*p >= 0x80 && w->esc_uni)) p++;
            if (!ctoon_write_bytes(w, run, (usize)(p - run))) return false;
            continue;
        }
        p++;
        switch (c) {
            case '"':  if (!ctoon_write_str_lit(w, "\\\"")) return false; break;
            case '\\':
                if (w->esc_slsh && *(p-1) == '/') {
                    if (!ctoon_write_str_lit(w, "\\/")) return false;
                } else {
                    if (!ctoon_write_str_lit(w, "\\\\")) return false;
                }
                break;
            case '\n': if (!ctoon_write_str_lit(w, "\\n"))  return false; break;
            case '\r': if (!ctoon_write_str_lit(w, "\\r"))  return false; break;
            case '\t': if (!ctoon_write_str_lit(w, "\\t"))  return false; break;
            case '\b': if (!ctoon_write_str_lit(w, "\\b"))  return false; break;
            case '\f': if (!ctoon_write_str_lit(w, "\\f"))  return false; break;
            default:
                if (c < 0x20) {
                    /* control char: \uXXXX */
                    if (!ctoon_write_buf_reserve(w, 6)) return false;
                    w->buf[w->len++] = '\\'; w->buf[w->len++] = 'u';
                    w->buf[w->len++] = '0';  w->buf[w->len++] = '0';
                    w->buf[w->len++] = "0123456789ABCDEF"[(c >> 4) & 0xF];
                    w->buf[w->len++] = "0123456789ABCDEF"[c & 0xF];
                } else if (c >= 0x80 && w->esc_uni) {
                    /* decode UTF-8 then emit \uXXXX */
                    u32 cp;
                    int extra;
                    if      ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
                    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
                    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
                    else                          { cp = '?';      extra = 0; }
                    for (int k = 0; k < extra && p < end; k++)
                        cp = (cp << 6) | (*p++ & 0x3F);
                    if (!ctoon_write_buf_reserve(w, 12)) return false;
                    if (cp <= 0xFFFF) {
                        w->len += (usize)snprintf((char*)w->buf+w->len, 8,
                                                   "\\u%04X", (unsigned)cp);
                    } else {
                        /* surrogate pair */
                        cp -= 0x10000;
                        u32 hi = 0xD800 + (cp >> 10);
                        u32 lo = 0xDC00 + (cp & 0x3FF);
                        w->len += (usize)snprintf((char*)w->buf+w->len, 14,
                                                   "\\u%04X\\u%04X",
                                                   (unsigned)hi, (unsigned)lo);
                    }
                } else {
                    if (!ctoon_write_char(w, c)) return false;
                }
                break;
        }
    }
    return ctoon_write_char(w, '"');
}

/*---- forward declarations --------------------------------------------------*/
static bool ctoon_write_val(ctoon_write_ctx *w, const ctoon_val *val, int depth);
static bool ctoon_write_obj(ctoon_write_ctx *w, const ctoon_val *obj, int depth);
static bool ctoon_write_arr_content(ctoon_write_ctx *w, const ctoon_val *arr, int depth);

/*---- container size helpers ------------------------------------------------*/

/* Child count of ARR or OBJ (OBJ stores kv-pair count; ARR stores elem count) */
static_inline usize ctoon_ctn_child_count(const ctoon_val *ctn) {
    return (usize)(ctn->tag >> CTOON_TAG_BIT);
}

/* First child of a container */
static_inline const ctoon_val *ctoon_ctn_first_child(const ctoon_val *ctn) {
    return ctn + 1;
}

/* Next sibling of ANY val (leaf: +1; container: skip to uni.ofs) */
static_inline const ctoon_val *ctoon_val_next_sibling(const ctoon_val *val) {
    bool is_ctn = (unsafe_ctoon_get_type((void *)val) >= CTOON_TYPE_ARR);
    usize ofs = is_ctn ? val->uni.ofs : sizeof(ctoon_val);
    return (const ctoon_val *)(const void *)((const u8 *)val + ofs);
}

/*---- tabular check ---------------------------------------------------------*/

/*
 * Returns true when arr is non-empty, every element is an OBJ with the same
 * set of scalar keys in the same order, and no value is itself a container.
 */
static bool ctoon_arr_is_tabular(const ctoon_val *arr) {
    usize count = ctoon_ctn_child_count(arr);
    if (count == 0) return false;

    const ctoon_val *first = ctoon_ctn_first_child(arr);
    if (unsafe_ctoon_get_type((void *)first) != CTOON_TYPE_OBJ) return false;
    usize ncols = ctoon_ctn_child_count(first);
    if (ncols == 0) return false;

    /* collect first-row keys */
    const ctoon_val *fkv = ctoon_ctn_first_child(first);
    const ctoon_val *cur = first;
    for (usize r = 0; r < count; r++) {
        if (unsafe_ctoon_get_type((void *)cur) != CTOON_TYPE_OBJ) return false;
        if (ctoon_ctn_child_count(cur) != ncols) return false;
        const ctoon_val *kv = ctoon_ctn_first_child(cur);
        const ctoon_val *fkv2 = fkv;
        for (usize c = 0; c < ncols; c++) {
            /* check key match */
            usize klen = unsafe_ctoon_get_len((void *)kv);
            usize fklen = unsafe_ctoon_get_len((void *)fkv2);
            if (klen != fklen || memcmp(kv->uni.str, fkv2->uni.str, klen)) return false;
            /* value must be scalar */
            const ctoon_val *vv = ctoon_val_next_sibling(kv);
            u8 vtype = unsafe_ctoon_get_type((void *)vv);
            if (vtype == CTOON_TYPE_ARR || vtype == CTOON_TYPE_OBJ) return false;
            kv  = ctoon_val_next_sibling(vv);
            fkv2 = ctoon_val_next_sibling(ctoon_val_next_sibling(fkv2));
        }
        cur = ctoon_val_next_sibling(cur);
    }
    return true;
}

/*---- array bracket formatter -----------------------------------------------*/

/*
 * Writes the array header token like:  key[3]:    or  key[#5|]{f1|f2}:
 * len_only: just the bracket+colon part (no {fields}).
 * For the closing (inline / list) variant:  [N]:
 * For the tabular (open) variant:           [N]{f1,f2}:
 */
static bool ctoon_write_arr_header(ctoon_write_ctx *w, const ctoon_val *arr, bool tabular) {
    usize count = ctoon_ctn_child_count(arr);
    if (!ctoon_write_char(w, '[')) return false;
    if (w->lmarker && !ctoon_write_char(w, '#')) return false;
    /* write count */
    {
        if (!ctoon_write_buf_reserve(w, 24)) return false;
        u8 *p = write_u64((u64)count, w->buf + w->len);
        w->len = (usize)(p - w->buf);
    }
    /* delimiter marker (only when non-comma) */
    if (w->delim != ',') {
        if (!ctoon_write_char(w, (u8)w->delim)) return false;
    }
    if (!ctoon_write_char(w, ']')) return false;

    if (tabular) {
        /* {field1,field2,...} */
        if (!ctoon_write_char(w, '{')) return false;
        const ctoon_val *first = ctoon_ctn_first_child(arr);
        const ctoon_val *kv    = ctoon_ctn_first_child(first);
        usize ncols = ctoon_ctn_child_count(first);
        for (usize c = 0; c < ncols; c++) {
            if (c > 0 && !ctoon_write_char(w, (u8)w->delim)) return false;
            usize klen = unsafe_ctoon_get_len((void *)kv);
            if (!ctoon_write_str(w, kv->uni.str, klen, true)) return false;
            kv = ctoon_val_next_sibling(ctoon_val_next_sibling(kv)); /* skip key+val */
        }
        if (!ctoon_write_char(w, '}')) return false;
    }
    return ctoon_write_str_lit(w, ": ");
}

/*---- array content writer --------------------------------------------------*/

static bool ctoon_write_arr_content(ctoon_write_ctx *w, const ctoon_val *arr, int depth) {
    usize count = ctoon_ctn_child_count(arr);
    if (count == 0) return true;

    /* check if all elements are primitives */
    bool all_prim = true;
    const ctoon_val *it = ctoon_ctn_first_child(arr);
    for (usize i = 0; i < count && all_prim; i++) {
        u8 t = unsafe_ctoon_get_type((void *)it);
        if (t == CTOON_TYPE_ARR || t == CTOON_TYPE_OBJ) all_prim = false;
        it = ctoon_val_next_sibling(it);
    }

    if (all_prim) {
        /* inline: v1,v2,v3 */
        it = ctoon_ctn_first_child(arr);
        for (usize i = 0; i < count; i++) {
            if (i > 0 && !ctoon_write_char(w, (u8)w->delim)) return false;
            if (!ctoon_write_val(w, it, 0)) return false;
            it = ctoon_val_next_sibling(it);
        }
        return true;
    }

    /* tabular */
    if (ctoon_arr_is_tabular(arr)) {
        usize ncols = ctoon_ctn_child_count(ctoon_ctn_first_child(arr));
        if (!ctoon_write_char(w, '\n')) return false;
        const ctoon_val *row = ctoon_ctn_first_child(arr);
        for (usize r = 0; r < count; r++) {
            if (!ctoon_write_indent(w, depth + 1)) return false;
            const ctoon_val *kv = ctoon_ctn_first_child(row);
            for (usize c = 0; c < ncols; c++) {
                if (c > 0 && !ctoon_write_char(w, (u8)w->delim)) return false;
                const ctoon_val *vv = ctoon_val_next_sibling(kv);
                if (!ctoon_write_val(w, vv, 0)) return false;
                kv = ctoon_val_next_sibling(vv);
            }
            if (r + 1 < count && !ctoon_write_char(w, '\n')) return false;
            row = ctoon_val_next_sibling(row);
        }
        return true;
    }

    /* list items */
    if (!ctoon_write_char(w, '\n')) return false;
    it = ctoon_ctn_first_child(arr);
    for (usize i = 0; i < count; i++) {
        if (!ctoon_write_indent(w, depth + 1)) return false;
        if (!ctoon_write_str_lit(w, "- ")) return false;
        u8 t = unsafe_ctoon_get_type((void *)it);
        if (t == CTOON_TYPE_OBJ) {
            /* object as list item: first field inline, rest indented */
            usize nfields = ctoon_ctn_child_count(it);
            const ctoon_val *kv = ctoon_ctn_first_child(it);
            for (usize fi = 0; fi < nfields; fi++) {
                if (fi > 0) {
                    if (!ctoon_write_char(w, '\n')) return false;
                    if (!ctoon_write_indent(w, depth + 2)) return false;
                }
                usize klen = unsafe_ctoon_get_len((void *)kv);
                if (!ctoon_write_str(w, kv->uni.str, klen, true)) return false;
                const ctoon_val *vv = ctoon_val_next_sibling(kv);
                u8 vt = unsafe_ctoon_get_type((void *)vv);
                if (vt == CTOON_TYPE_OBJ) {
                    if (!ctoon_write_char(w, ':')) return false;
                    if (!ctoon_write_char(w, '\n')) return false;
                    if (!ctoon_write_obj(w, vv, depth + 3)) return false;
                } else if (vt == CTOON_TYPE_ARR) {
                    bool tab = ctoon_arr_is_tabular(vv);
                    if (!ctoon_write_arr_header(w, vv, tab)) return false;
                    if (!ctoon_write_arr_content(w, vv, depth + 2)) return false;
                } else {
                    if (!ctoon_write_str_lit(w, ": ")) return false;
                    if (!ctoon_write_val(w, vv, depth + 2)) return false;
                }
                kv = ctoon_val_next_sibling(vv);
            }
        } else {
            if (!ctoon_write_val(w, it, depth + 1)) return false;
        }
        if (i + 1 < count && !ctoon_write_char(w, '\n')) return false;
        it = ctoon_val_next_sibling(it);
    }
    return true;
}

/*---- object writer ---------------------------------------------------------*/

static bool ctoon_write_obj(ctoon_write_ctx *w, const ctoon_val *obj, int depth) {
    usize nfields = ctoon_ctn_child_count(obj);
    const ctoon_val *kv = ctoon_ctn_first_child(obj);
    for (usize i = 0; i < nfields; i++) {
        if (i > 0 && !ctoon_write_char(w, '\n')) return false;
        if (!ctoon_write_indent(w, depth)) return false;
        /* key */
        usize klen = unsafe_ctoon_get_len((void *)kv);
        if (!ctoon_write_str(w, kv->uni.str, klen, true)) return false;
        const ctoon_val *vv = ctoon_val_next_sibling(kv);
        u8 vt = unsafe_ctoon_get_type((void *)vv);

        if (vt == CTOON_TYPE_ARR) {
            bool tab = ctoon_arr_is_tabular(vv);
            if (!ctoon_write_arr_header(w, vv, tab)) return false;
            if (!ctoon_write_arr_content(w, vv, depth)) return false;
        } else if (vt == CTOON_TYPE_OBJ) {
            if (!ctoon_write_char(w, ':')) return false;
            if (!ctoon_write_char(w, '\n')) return false;
            if (!ctoon_write_obj(w, vv, depth + 1)) return false;
        } else {
            if (!ctoon_write_str_lit(w, ": ")) return false;
            if (!ctoon_write_val(w, vv, depth)) return false;
        }
        kv = ctoon_val_next_sibling(vv);
    }
    return true;
}

/*---- leaf value writer -----------------------------------------------------*/

static bool ctoon_write_val(ctoon_write_ctx *w, const ctoon_val *val, int depth) {
    u8 type = unsafe_ctoon_get_type((void *)val);
    switch (type) {
        case CTOON_TYPE_NULL:
            return ctoon_write_str_lit(w, "null");
        case CTOON_TYPE_BOOL:
            return ctoon_write_str_lit(w, ((val->tag & CTOON_SUBTYPE_MASK) == CTOON_SUBTYPE_TRUE)
                               ? "true" : "false");
        case CTOON_TYPE_NUM:
            return ctoon_write_num(w, val);
        case CTOON_TYPE_STR: {
            usize len = unsafe_ctoon_get_len((void *)val);
            return ctoon_write_str(w, val->uni.str, len, false);
        }
        case CTOON_TYPE_ARR: {
            bool tab = ctoon_arr_is_tabular(val);
            if (!ctoon_write_arr_header(w, val, tab)) return false;
            return ctoon_write_arr_content(w, val, depth);
        }
        case CTOON_TYPE_OBJ:
            return ctoon_write_obj(w, val, depth);
        case CTOON_TYPE_RAW: {
            /* raw string: write as-is (no quoting) */
            usize len = unsafe_ctoon_get_len((void *)val);
            return ctoon_write_bytes(w, (const u8 *)val->uni.str, len);
        }
        default:
            return ctoon_write_str_lit(w, "null");
    }
}

/*---- options resolver ------------------------------------------------------*/

static_inline void ctoon_write_resolve_opts(ctoon_write_ctx *w, const ctoon_write_options *opts) {
    if (opts) {
        w->flg       = opts->flag;
        w->delim     = (opts->delimiter == CTOON_DELIMITER_TAB)  ? '\t' :
                       (opts->delimiter == CTOON_DELIMITER_PIPE) ? '|'  : ',';
        w->indent    = (opts->indent > 0) ? opts->indent : 2;
        w->lmarker   = (opts->flag & CTOON_WRITE_LENGTH_MARKER)        != 0;
        w->esc_uni   = (opts->flag & CTOON_WRITE_ESCAPE_UNICODE)       != 0;
        w->esc_slsh  = (opts->flag & CTOON_WRITE_ESCAPE_SLASHES)       != 0;
        w->inf_null  = (opts->flag & CTOON_WRITE_INF_AND_NAN_AS_NULL)  != 0;
        w->inf_lit   = (!w->inf_null) &&
                       (opts->flag & CTOON_WRITE_ALLOW_INF_AND_NAN)    != 0;
    } else {
        /* defaults */
        w->flg      = CTOON_WRITE_NOFLAG;
        w->delim    = ',';
        w->indent   = 2;
        w->lmarker  = false;
        w->esc_uni  = false;
        w->esc_slsh = false;
        w->inf_null = false;
        w->inf_lit  = false;
    }
}

/*---- immutable doc write ---------------------------------------------------*/

static char *ctoon_write_to_buf(const ctoon_val *root,
                             const ctoon_write_options *opts,
                             const ctoon_alc *alc_ptr,
                             usize *len_out,
                             ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;

    ctoon_write_ctx w;
    memset(&w, 0, sizeof(w));
    w.alc = alc_ptr ? *alc_ptr : CTOON_DEFAULT_ALC;
    ctoon_write_resolve_opts(&w, opts);

    bool ok = ctoon_write_val(&w, root, 0);

    if (ok && (w.flg & CTOON_WRITE_NEWLINE_AT_END))
        ok = ctoon_write_char(&w, '\n');

    if (unlikely(!ok)) {
        if (w.buf) w.alc.free(w.alc.ctx, w.buf);
        if (len_out) *len_out = 0;
        err->code = CTOON_WRITE_ERROR_MEMORY_ALLOCATION;
        err->msg  = "write failed (OOM or invalid value)";
        return NULL;
    }

    /* null-terminate */
    if (!ctoon_write_char(&w, 0)) {
        w.alc.free(w.alc.ctx, w.buf);
        if (len_out) *len_out = 0;
        err->code = CTOON_WRITE_ERROR_MEMORY_ALLOCATION;
        err->msg  = MSG_MALLOC;
        return NULL;
    }
    w.len--; /* exclude NUL from reported length */

    if (len_out) *len_out = w.len;
    err->code = CTOON_WRITE_SUCCESS;
    err->msg  = NULL;
    return (char *)w.buf;
}

/*==============================================================================
 * MARK: - Public Writer API (Implementation)
 *============================================================================*/

ctoon_api char *ctoon_write_opts(const ctoon_doc *doc,
                                   const ctoon_write_options *opts,
                                   const ctoon_alc *alc,
                                   usize *len,
                                   ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;
    if (unlikely(!doc || !doc->root)) {
        err->code = CTOON_WRITE_ERROR_INVALID_PARAMETER;
        err->msg  = "doc is NULL";
        if (len) *len = 0;
        return NULL;
    }
    return ctoon_write_to_buf(doc->root, opts, alc, len, err);
}

ctoon_api bool ctoon_write_file(const char *path,
                                  const ctoon_doc *doc,
                                  const ctoon_write_options *opts,
                                  const ctoon_alc *alc,
                                  ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;
    usize len = 0;
    char *raw = ctoon_write_opts(doc, opts, alc, &len, err);
    if (unlikely(!raw)) return false;
    FILE *f = fopen_safe(path, "wb");
    if (unlikely(!f)) {
        ctoon_alc a = alc ? *alc : CTOON_DEFAULT_ALC;
        a.free(a.ctx, raw);
        err->code = CTOON_WRITE_ERROR_FILE_OPEN;
        err->msg  = "failed to open file";
        return false;
    }
    usize wr = fwrite(raw, 1, len, f);
    fclose(f);
    ctoon_alc a = alc ? *alc : CTOON_DEFAULT_ALC;
    a.free(a.ctx, raw);
    if (unlikely(wr != len)) {
        err->code = CTOON_WRITE_ERROR_FILE_WRITE;
        err->msg  = "failed to write file";
        return false;
    }
    err->code = CTOON_WRITE_SUCCESS; err->msg = NULL;
    return true;
}

ctoon_api bool ctoon_write_fp(FILE *fp,
                                const ctoon_doc *doc,
                                const ctoon_write_options *opts,
                                const ctoon_alc *alc,
                                ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;
    usize len = 0;
    char *raw = ctoon_write_opts(doc, opts, alc, &len, err);
    if (unlikely(!raw)) return false;
    usize wr = fwrite(raw, 1, len, fp);
    ctoon_alc a = alc ? *alc : CTOON_DEFAULT_ALC;
    a.free(a.ctx, raw);
    if (unlikely(wr != len)) {
        err->code = CTOON_WRITE_ERROR_FILE_WRITE;
        err->msg  = "failed to write to file pointer";
        return false;
    }
    err->code = CTOON_WRITE_SUCCESS; err->msg = NULL;
    return true;
}

ctoon_api char *ctoon_val_write_opts(const ctoon_val *val,
                                      const ctoon_write_options *opts,
                                      const ctoon_alc *alc,
                                      usize *len,
                                      ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;
    if (unlikely(!val)) {
        err->code = CTOON_WRITE_ERROR_INVALID_PARAMETER;
        err->msg  = "val is NULL";
        if (len) *len = 0;
        return NULL;
    }
    return ctoon_write_to_buf(val, opts, alc, len, err);
}

/*---- mutable doc write -----------------------------------------------------*/

/*
 * To serialise a ctoon_mut_doc we convert to immutable first (cheap copy),
 * write it, then free the immutable copy.  This reuses all the ctoon_write_ctx logic.
 */
ctoon_api char *ctoon_mut_write_opts(const ctoon_mut_doc *doc,
                                      const ctoon_write_options *opts,
                                      const ctoon_alc *alc,
                                      usize *len,
                                      ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;
    if (unlikely(!doc || !doc->root)) {
        err->code = CTOON_WRITE_ERROR_INVALID_PARAMETER;
        err->msg  = "doc is NULL";
        if (len) *len = 0;
        return NULL;
    }
    /* convert to immutable */
    ctoon_alc a = alc ? *alc : CTOON_DEFAULT_ALC;
    ctoon_doc *imut = ctoon_mut_doc_imut_copy((ctoon_mut_doc *)(uintptr_t)doc, &a);
    if (unlikely(!imut)) {
        err->code = CTOON_WRITE_ERROR_MEMORY_ALLOCATION;
        err->msg  = MSG_MALLOC;
        if (len) *len = 0;
        return NULL;
    }
    char *result = ctoon_write_opts(imut, opts, alc, len, err);
    ctoon_doc_free(imut);
    return result;
}

ctoon_api bool ctoon_mut_write_file(const char *path,
                                     const ctoon_mut_doc *doc,
                                     const ctoon_write_options *opts,
                                     const ctoon_alc *alc,
                                     ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;
    usize len = 0;
    char *raw = ctoon_mut_write_opts(doc, opts, alc, &len, err);
    if (!raw) return false;
    FILE *f = fopen_safe(path, "wb");
    if (!f) {
        ctoon_alc a = alc ? *alc : CTOON_DEFAULT_ALC;
        a.free(a.ctx, raw);
        err->code = CTOON_WRITE_ERROR_FILE_OPEN;
        err->msg  = "failed to open file";
        return false;
    }
    usize wr = fwrite(raw, 1, len, f);
    fclose(f);
    ctoon_alc a = alc ? *alc : CTOON_DEFAULT_ALC;
    a.free(a.ctx, raw);
    if (wr != len) {
        err->code = CTOON_WRITE_ERROR_FILE_WRITE;
        err->msg  = "failed to write file";
        return false;
    }
    err->code = CTOON_WRITE_SUCCESS; err->msg = NULL;
    return true;
}

ctoon_api bool ctoon_mut_write_fp(FILE *fp,
                                   const ctoon_mut_doc *doc,
                                   const ctoon_write_options *opts,
                                   const ctoon_alc *alc,
                                   ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;
    usize len = 0;
    char *raw = ctoon_mut_write_opts(doc, opts, alc, &len, err);
    if (!raw) return false;
    usize wr = fwrite(raw, 1, len, fp);
    ctoon_alc a = alc ? *alc : CTOON_DEFAULT_ALC;
    a.free(a.ctx, raw);
    if (wr != len) {
        err->code = CTOON_WRITE_ERROR_FILE_WRITE;
        err->msg  = "write to fp failed";
        return false;
    }
    err->code = CTOON_WRITE_SUCCESS; err->msg = NULL;
    return true;
}

ctoon_api char *ctoon_mut_val_write_opts(const ctoon_mut_val *mval,
                                          const ctoon_write_options *opts,
                                          const ctoon_alc *alc,
                                          usize *len,
                                          ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;
    if (unlikely(!mval)) {
        err->code = CTOON_WRITE_ERROR_INVALID_PARAMETER;
        err->msg  = "val is NULL";
        if (len) *len = 0;
        return NULL;
    }
    ctoon_alc a = alc ? *alc : CTOON_DEFAULT_ALC;
    ctoon_doc *imut = ctoon_mut_val_imut_copy((ctoon_mut_val *)(uintptr_t)mval, &a);
    if (unlikely(!imut)) {
        err->code = CTOON_WRITE_ERROR_MEMORY_ALLOCATION;
        err->msg  = MSG_MALLOC;
        if (len) *len = 0;
        return NULL;
    }
    char *result = ctoon_val_write_opts(imut->root, opts, alc, len, err);
    ctoon_doc_free(imut);
    return result;
}

/*---- number-only helper (public) -------------------------------------------*/

ctoon_api char *ctoon_write_number(const ctoon_val *val, char *buf) {
    if (!val || !buf) return NULL;
    if (unsafe_ctoon_get_type((void *)val) != CTOON_TYPE_NUM) return NULL;
    /* max number text: ~40 chars */
    ctoon_write_ctx w;
    memset(&w, 0, sizeof(w));
    w.alc = CTOON_DEFAULT_ALC;
    ctoon_write_resolve_opts(&w, NULL);
    w.buf = (u8 *)buf;
    w.cap = 40;
    /* temporarily borrow buf; don't free */
    bool ok = ctoon_write_num(&w, val);
    if (!ok) return NULL;
    w.buf[w.len] = '\0';
    w.buf = NULL; /* prevent free */
    return buf;
}
#endif /* CTOON_DISABLE_WRITER */




#if !(!defined(CTOON_ENABLE_JSON) || !CTOON_ENABLE_JSON)

/*==============================================================================
 * MARK: - JSON Serializer
 *
 * Traverses the flat ctoon_val pool and emits RFC 8259 JSON.
 * Shares the ctoon_write_ctx buffer infrastructure from the TOON writer.
 *============================================================================*/

/* JSON string writer – always quotes, handles all escape sequences */
static bool cj_write_str(ctoon_write_ctx *w, const char *s, usize len) {
    if (!ctoon_write_char(w, '"')) return false;
    const u8 *p   = (const u8 *)s;
    const u8 *end = p + len;
    while (p < end) {
        u8 c = *p;
        if (likely(c >= 0x20 && c != '"' && c != '\\'
                   && !(c >= 0x80 && w->esc_uni))) {
            const u8 *run = p;
            while (p < end && *p >= 0x20 && *p != '"' && *p != '\\'
                   && !(*p >= 0x80 && w->esc_uni)) p++;
            if (!ctoon_write_bytes(w, run, (usize)(p - run))) return false;
            continue;
        }
        p++;
        switch (c) {
            case '"':  if (!ctoon_write_str_lit(w, "\\\"")) return false; break;
            case '\\': if (!ctoon_write_str_lit(w, "\\\\")) return false; break;
            case '\n': if (!ctoon_write_str_lit(w, "\\n"))  return false; break;
            case '\r': if (!ctoon_write_str_lit(w, "\\r"))  return false; break;
            case '\t': if (!ctoon_write_str_lit(w, "\\t"))  return false; break;
            case '\b': if (!ctoon_write_str_lit(w, "\\b"))  return false; break;
            case '\f': if (!ctoon_write_str_lit(w, "\\f"))  return false; break;
            default:
                if (c < 0x20) {
                    if (!ctoon_write_buf_reserve(w, 7)) return false;
                    w->buf[w->len++] = '\\'; w->buf[w->len++] = 'u';
                    w->buf[w->len++] = '0';  w->buf[w->len++] = '0';
                    w->buf[w->len++] = "0123456789abcdef"[(c >> 4) & 0xF];
                    w->buf[w->len++] = "0123456789abcdef"[c & 0xF];
                } else if (c >= 0x80 && w->esc_uni) {
                    /* Encode as \uXXXX (BMP) or surrogate pair */
                    u32 cp;
                    int extra;
                    if      ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
                    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
                    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
                    else                          { cp = '?';      extra = 0; }
                    for (int k = 0; k < extra && p < end; k++)
                        cp = (cp << 6) | (*p++ & 0x3F);
                    if (!ctoon_write_buf_reserve(w, 13)) return false;
                    if (cp <= 0xFFFF) {
                        w->len += (usize)snprintf(
                            (char *)w->buf + w->len, 8, "\\u%04X", (unsigned)cp);
                    } else {
                        cp -= 0x10000;
                        u32 hi = 0xD800 + (cp >> 10);
                        u32 lo = 0xDC00 + (cp & 0x3FF);
                        w->len += (usize)snprintf(
                            (char *)w->buf + w->len, 14, "\\u%04X\\u%04X",
                            (unsigned)hi, (unsigned)lo);
                    }
                } else {
                    if (!ctoon_write_char(w, c)) return false;
                }
                break;
        }
    }
    return ctoon_write_char(w, '"');
}

/* Forward declaration */
static bool cj_write_val(ctoon_write_ctx *w, const ctoon_val *val, int depth);

static bool cj_write_nl_indent(ctoon_write_ctx *w, int depth) {
    if (w->indent == 0) return true;
    if (!ctoon_write_char(w, '\n')) return false;
    return ctoon_write_indent(w, depth);
}

static bool cj_write_arr(ctoon_write_ctx *w, const ctoon_val *arr, int depth) {
    usize count = ctoon_ctn_child_count(arr);
    if (!ctoon_write_char(w, '[')) return false;
    if (count == 0) return ctoon_write_char(w, ']');
    const ctoon_val *it = ctoon_ctn_first_child(arr);
    for (usize i = 0; i < count; i++) {
        if (!cj_write_nl_indent(w, depth + 1)) return false;
        if (!cj_write_val(w, it, depth + 1)) return false;
        if (i + 1 < count && !ctoon_write_char(w, ',')) return false;
        it = ctoon_val_next_sibling(it);
    }
    if (!cj_write_nl_indent(w, depth)) return false;
    return ctoon_write_char(w, ']');
}

static bool cj_write_obj(ctoon_write_ctx *w, const ctoon_val *obj, int depth) {
    usize nfields = ctoon_ctn_child_count(obj);
    if (!ctoon_write_char(w, '{')) return false;
    if (nfields == 0) return ctoon_write_char(w, '}');
    const ctoon_val *kv = ctoon_ctn_first_child(obj);
    for (usize i = 0; i < nfields; i++) {
        if (!cj_write_nl_indent(w, depth + 1)) return false;
        /* key */
        usize klen = unsafe_ctoon_get_len((void *)kv);
        if (!cj_write_str(w, kv->uni.str, klen)) return false;
        if (w->indent > 0) {
            if (!ctoon_write_str_lit(w, ": ")) return false;
        } else {
            if (!ctoon_write_char(w, ':')) return false;
        }
        /* value */
        const ctoon_val *vv = ctoon_val_next_sibling(kv);
        if (!cj_write_val(w, vv, depth + 1)) return false;
        if (i + 1 < nfields && !ctoon_write_char(w, ',')) return false;
        kv = ctoon_val_next_sibling(vv);
    }
    if (!cj_write_nl_indent(w, depth)) return false;
    return ctoon_write_char(w, '}');
}

static bool cj_write_val(ctoon_write_ctx *w, const ctoon_val *val, int depth) {
    u8 type = unsafe_ctoon_get_type((void *)val);
    switch (type) {
        case CTOON_TYPE_NULL:
            return ctoon_write_str_lit(w, "null");
        case CTOON_TYPE_BOOL:
            return ctoon_write_str_lit(w,
                ((val->tag & CTOON_SUBTYPE_MASK) == CTOON_SUBTYPE_TRUE)
                ? "true" : "false");
        case CTOON_TYPE_NUM:
            return ctoon_write_num(w, val);
        case CTOON_TYPE_STR: {
            usize len = unsafe_ctoon_get_len((void *)val);
            return cj_write_str(w, val->uni.str ? val->uni.str : "", len);
        }
        case CTOON_TYPE_ARR: return cj_write_arr(w, val, depth);
        case CTOON_TYPE_OBJ: return cj_write_obj(w, val, depth);
        default:
            return ctoon_write_str_lit(w, "null");
    }
}

static char *cj_doc_write(const ctoon_val *root,
                            int indent,
                            ctoon_write_flag flags,
                            const ctoon_alc *alc_ptr,
                            usize *len_out,
                            ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;

    ctoon_write_ctx w;
    memset(&w, 0, sizeof(w));
    w.alc       = alc_ptr ? *alc_ptr : CTOON_DEFAULT_ALC;
    w.indent    = (indent > 0) ? indent : 0;
    w.delim     = ',';
    w.flg       = flags;
    w.esc_uni   = (flags & CTOON_WRITE_ESCAPE_UNICODE)      != 0;
    w.inf_null  = (flags & CTOON_WRITE_INF_AND_NAN_AS_NULL) != 0;
    w.inf_lit   = (!w.inf_null) &&
                  (flags & CTOON_WRITE_ALLOW_INF_AND_NAN)   != 0;

    bool ok = cj_write_val(&w, root, 0);

    if (ok && (flags & CTOON_WRITE_NEWLINE_AT_END))
        ok = ctoon_write_char(&w, '\n');

    if (unlikely(!ok)) {
        if (w.buf) w.alc.free(w.alc.ctx, w.buf);
        if (len_out) *len_out = 0;
        err->code = CTOON_WRITE_ERROR_MEMORY_ALLOCATION;
        err->msg  = "JSON write failed";
        return NULL;
    }

    if (!ctoon_write_char(&w, '\0')) {
        w.alc.free(w.alc.ctx, w.buf);
        if (len_out) *len_out = 0;
        err->code = CTOON_WRITE_ERROR_MEMORY_ALLOCATION;
        err->msg  = MSG_MALLOC;
        return NULL;
    }
    w.len--;  /* exclude NUL from reported length */

    if (len_out) *len_out = w.len;
    err->code = CTOON_WRITE_SUCCESS;
    err->msg  = NULL;
    return (char *)w.buf;
}

ctoon_api char *ctoon_write_json(const ctoon_doc *doc,
                                  int indent,
                                  ctoon_write_flag flg,
                                   const ctoon_alc *alc,
                                   size_t *len,
                                   ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;
    if (unlikely(!doc || !doc->root)) {
        err->code = CTOON_WRITE_ERROR_INVALID_PARAMETER;
        err->msg  = "doc is NULL";
        if (len) *len = 0;
        return NULL;
    }
    return cj_doc_write(doc->root, indent, flg, alc, (usize *)len, err);
}

ctoon_api char *ctoon_write_json_mut(const ctoon_mut_doc *doc,
                                      int indent,
                                      ctoon_write_flag flg,
                                       const ctoon_alc *alc,
                                       size_t *len,
                                       ctoon_write_err *err) {
    ctoon_write_err tmp_err;
    if (!err) err = &tmp_err;
    if (unlikely(!doc || !doc->root)) {
        err->code = CTOON_WRITE_ERROR_INVALID_PARAMETER;
        err->msg  = "doc is NULL";
        if (len) *len = 0;
        return NULL;
    }
    ctoon_alc a = alc ? *alc : CTOON_DEFAULT_ALC;
    ctoon_doc *imut = ctoon_mut_doc_imut_copy(
        (ctoon_mut_doc *)(uintptr_t)doc, &a);
    if (unlikely(!imut)) {
        err->code = CTOON_WRITE_ERROR_MEMORY_ALLOCATION;
        err->msg  = MSG_MALLOC;
        if (len) *len = 0;
        return NULL;
    }
    char *result = cj_doc_write(imut->root, indent, flg, alc,
                                 (usize *)len, err);
    ctoon_doc_free(imut);
    return result;
}

#endif /* CTOON_ENABLE_JSON */




#if !CTOON_DISABLE_UTILS

/*==============================================================================
 * MARK: - TOON Pointer API (RFC 6901) (Public)
 *============================================================================*/

/**
 Get a token from TOON pointer string.
 @param ptr [in]  string that points to current token prefix `/`
            [out] string that points to next token prefix `/`, or string end
 @param end [in] end of the entire TOON Pointer string
 @param len [out] unescaped token length
 @param esc [out] number of escaped characters in this token
 @return head of the token, or NULL if syntax error
 */
static_inline const char *ptr_next_token(const char **ptr, const char *end,
                                         usize *len, usize *esc) {
    const char *hdr = *ptr + 1;
    const char *cur = hdr;
    /* skip unescaped characters */
    while (cur < end && *cur != '/' && *cur != '~') cur++;
    if (likely(cur == end || *cur != '~')) {
        /* no escaped characters, return */
        *ptr = cur;
        *len = (usize)(cur - hdr);
        *esc = 0;
        return hdr;
    } else {
        /* handle escaped characters */
        usize esc_num = 0;
        while (cur < end && *cur != '/') {
            if (*cur++ == '~') {
                if (cur == end || (*cur != '0' && *cur != '1')) {
                    *ptr = cur - 1;
                    return NULL;
                }
                esc_num++;
            }
        }
        *ptr = cur;
        *len = (usize)(cur - hdr) - esc_num;
        *esc = esc_num;
        return hdr;
    }
}

/**
 Convert token string to index.
 @param cur [in]  token head
 @param len [in]  token length
 @param idx [out] the index number, or USIZE_MAX if token is '-'
 @return true if token is a valid array index
 */
static_inline bool ptr_token_to_idx(const char *cur, usize len, usize *idx) {
    const char *end = cur + len;
    usize num = 0, add;
    if (unlikely(len == 0 || len > USIZE_SAFE_DIG)) return false;
    if (*cur == '0') {
        if (unlikely(len > 1)) return false;
        *idx = 0;
        return true;
    }
    if (*cur == '-') {
        if (unlikely(len > 1)) return false;
        *idx = USIZE_MAX;
        return true;
    }
    for (; cur < end && (add = (usize)((u8)*cur - (u8)'0')) <= 9; cur++) {
        num = num * 10 + add;
    }
    if (unlikely(num == 0 || cur < end)) return false;
    *idx = num;
    return true;
}

/**
 Compare TOON key with token.
 @param key a string key (ctoon_val or ctoon_mut_val)
 @param token a TOON pointer token
 @param len unescaped token length
 @param esc number of escaped characters in this token
 @return true if `str` is equals to `token`
 */
static_inline bool ptr_token_eq(void *key,
                                const char *token, usize len, usize esc) {
    ctoon_val *val = (ctoon_val *)key;
    if (unsafe_ctoon_get_len(val) != len) return false;
    if (likely(!esc)) {
        return memcmp(val->uni.str, token, len) == 0;
    } else {
        const char *str = val->uni.str;
        for (; len-- > 0; token++, str++) {
            if (*token == '~') {
                if (*str != (*++token == '0' ? '~' : '/')) return false;
            } else {
                if (*str != *token) return false;
            }
        }
        return true;
    }
}

/**
 Get a value from array by token.
 @param arr   an array, should not be NULL or non-array type
 @param token a TOON pointer token
 @param len   unescaped token length
 @param esc   number of escaped characters in this token
 @return value at index, or NULL if token is not index or index is out of range
 */
static_inline ctoon_val *ptr_arr_get(ctoon_val *arr, const char *token,
                                      usize len, usize esc) {
    ctoon_val *val = unsafe_ctoon_get_first(arr);
    usize num = unsafe_ctoon_get_len(arr), idx = 0;
    usize real_len = len - esc; /* unescaped token length */
    if (unlikely(num == 0)) return NULL;
    if (unlikely(!ptr_token_to_idx(token, real_len, &idx))) return NULL;
    if (unlikely(idx >= num)) return NULL;
    if (unsafe_ctoon_arr_is_flat(arr)) {
        return val + idx;
    } else {
        while (idx-- > 0) val = unsafe_ctoon_get_next(val);
        return val;
    }
}

/**
 Get a value from object by token.
 @param obj   [in] an object, should not be NULL or non-object type
 @param token [in] a TOON pointer token
 @param len   [in] unescaped token length
 @param esc   [in] number of escaped characters in this token
 @return value associated with the token, or NULL if no value
 */
static_inline ctoon_val *ptr_obj_get(ctoon_val *obj, const char *token,
                                      usize len, usize esc) {
    ctoon_val *key = unsafe_ctoon_get_first(obj);
    usize num = unsafe_ctoon_get_len(obj);
    if (unlikely(num == 0)) return NULL;
    for (; num > 0; num--, key = unsafe_ctoon_get_next(key + 1)) {
        if (ptr_token_eq(key, token, len, esc)) return key + 1;
    }
    return NULL;
}

/**
 Get a value from array by token.
 @param arr   [in] an array, should not be NULL or non-array type
 @param token [in] a TOON pointer token
 @param len   [in] unescaped token length
 @param esc   [in] number of escaped characters in this token
 @param pre   [out] previous (sibling) value of the returned value
 @param last  [out] whether index is last
 @return value at index, or NULL if token is not index or index is out of range
 */
static_inline ctoon_mut_val *ptr_mut_arr_get(ctoon_mut_val *arr,
                                              const char *token,
                                              usize len, usize esc,
                                              ctoon_mut_val **pre,
                                              bool *last) {
    ctoon_mut_val *val = (ctoon_mut_val *)arr->uni.ptr; /* last (tail) */
    usize real_len = len - esc; /* unescaped token length */
    usize num = unsafe_ctoon_get_len(arr), idx;
    if (last) *last = false;
    if (pre) *pre = NULL;
    if (unlikely(num == 0)) {
        if (last && real_len == 1 && (*token == '0' || *token == '-')) *last = true;
        return NULL;
    }
    if (unlikely(!ptr_token_to_idx(token, real_len, &idx))) return NULL;
    if (last) *last = (idx == num || idx == USIZE_MAX);
    if (unlikely(idx >= num)) return NULL;
    while (idx-- > 0) val = val->next;
    if (pre) *pre = val;
    return val->next;
}

/**
 Get a value from object by token.
 @param obj   [in] an object, should not be NULL or non-object type
 @param token [in] a TOON pointer token
 @param len   [in] unescaped token length
 @param esc   [in] number of escaped characters in this token
 @param pre   [out] previous (sibling) key of the returned value's key
 @return value associated with the token, or NULL if no value
 */
static_inline ctoon_mut_val *ptr_mut_obj_get(ctoon_mut_val *obj,
                                              const char *token,
                                              usize len, usize esc,
                                              ctoon_mut_val **pre) {
    ctoon_mut_val *pre_key = (ctoon_mut_val *)obj->uni.ptr, *key;
    usize num = unsafe_ctoon_get_len(obj);
    if (pre) *pre = NULL;
    if (unlikely(num == 0)) return NULL;
    for (; num > 0; num--, pre_key = key) {
        key = pre_key->next->next;
        if (ptr_token_eq(key, token, len, esc)) {
            if (pre) *pre = pre_key;
            return key->next;
        }
    }
    return NULL;
}

/**
 Create a string value with TOON pointer token.
 @param token [in] a TOON pointer token
 @param len   [in] unescaped token length
 @param esc   [in] number of escaped characters in this token
 @param doc   [in] used for memory allocation when creating value
 @return new string value, or NULL if memory allocation failed
 */
static_inline ctoon_mut_val *ptr_new_key(const char *token,
                                          usize len, usize esc,
                                          ctoon_mut_doc *doc) {
    const char *src = token;
    if (likely(!esc)) {
        return ctoon_mut_strncpy(doc, src, len);
    } else {
        const char *end = src + len + esc;
        char *dst = unsafe_ctoon_mut_str_alc(doc, len + esc);
        char *str = dst;
        if (unlikely(!dst)) return NULL;
        for (; src < end; src++, dst++) {
            if (*src != '~') *dst = *src;
            else *dst = (*++src == '0' ? '~' : '/');
        }
        *dst = '\0';
        return ctoon_mut_strn(doc, str, len);
    }
}

/* macros for ctoon_ptr */
#define return_err(_ret, _code, _pos, _msg) do { \
    if (err) { \
        err->code = CTOON_PTR_ERR_##_code; \
        err->msg = _msg; \
        err->pos = (usize)(_pos); \
    } \
    return _ret; \
} while (false)

#define return_err_resolve(_ret, _pos) \
    return_err(_ret, RESOLVE, _pos, "TOON pointer cannot be resolved")
#define return_err_syntax(_ret, _pos) \
    return_err(_ret, SYNTAX, _pos, "invalid escaped character")
#define return_err_alloc(_ret) \
    return_err(_ret, MEMORY_ALLOCATION, 0, "failed to create value")

ctoon_val *unsafe_ctoon_ptr_getx(ctoon_val *val,
                                   const char *ptr, size_t ptr_len,
                                   ctoon_ptr_err *err) {

    const char *hdr = ptr, *end = ptr + ptr_len, *token;
    usize len, esc;
    ctoon_type type;

    while (true) {
        token = ptr_next_token(&ptr, end, &len, &esc);
        if (unlikely(!token)) return_err_syntax(NULL, ptr - hdr);
        type = unsafe_ctoon_get_type(val);
        if (type == CTOON_TYPE_OBJ) {
            val = ptr_obj_get(val, token, len, esc);
        } else if (type == CTOON_TYPE_ARR) {
            val = ptr_arr_get(val, token, len, esc);
        } else {
            val = NULL;
        }
        if (!val) return_err_resolve(NULL, token - hdr);
        if (ptr == end) return val;
    }
}

ctoon_mut_val *unsafe_ctoon_mut_ptr_getx(
    ctoon_mut_val *val, const char *ptr, size_t ptr_len,
    ctoon_ptr_ctx *ctx, ctoon_ptr_err *err) {

    const char *hdr = ptr, *end = ptr + ptr_len, *token;
    usize len, esc;
    ctoon_mut_val *ctn, *pre = NULL;
    ctoon_type type;
    bool idx_is_last = false;

    while (true) {
        token = ptr_next_token(&ptr, end, &len, &esc);
        if (unlikely(!token)) return_err_syntax(NULL, ptr - hdr);
        ctn = val;
        type = unsafe_ctoon_get_type(val);
        if (type == CTOON_TYPE_OBJ) {
            val = ptr_mut_obj_get(val, token, len, esc, &pre);
        } else if (type == CTOON_TYPE_ARR) {
            val = ptr_mut_arr_get(val, token, len, esc, &pre, &idx_is_last);
        } else {
            val = NULL;
        }
        if (ctx && (ptr == end)) {
            if (type == CTOON_TYPE_OBJ ||
                (type == CTOON_TYPE_ARR && (val || idx_is_last))) {
                ctx->ctn = ctn;
                ctx->pre = pre;
            }
        }
        if (!val) return_err_resolve(NULL, token - hdr);
        if (ptr == end) return val;
    }
}

bool unsafe_ctoon_mut_ptr_putx(
    ctoon_mut_val *val, const char *ptr, size_t ptr_len,
    ctoon_mut_val *new_val, ctoon_mut_doc *doc, bool create_parent,
    bool insert_new, ctoon_ptr_ctx *ctx, ctoon_ptr_err *err) {

    const char *hdr = ptr, *end = ptr + ptr_len, *token;
    usize token_len, esc, ctn_len;
    ctoon_mut_val *ctn, *key, *pre = NULL;
    ctoon_mut_val *sep_ctn = NULL, *sep_key = NULL, *sep_val = NULL;
    ctoon_type ctn_type;
    bool idx_is_last = false;

    /* skip exist parent nodes */
    while (true) {
        token = ptr_next_token(&ptr, end, &token_len, &esc);
        if (unlikely(!token)) return_err_syntax(false, ptr - hdr);
        ctn = val;
        ctn_type = unsafe_ctoon_get_type(ctn);
        if (ctn_type == CTOON_TYPE_OBJ) {
            val = ptr_mut_obj_get(ctn, token, token_len, esc, &pre);
        } else if (ctn_type == CTOON_TYPE_ARR) {
            val = ptr_mut_arr_get(ctn, token, token_len, esc, &pre,
                                  &idx_is_last);
        } else return_err_resolve(false, token - hdr);
        if (!val) break;
        if (ptr == end) break; /* is last token */
    }

    /* create parent nodes if not exist */
    if (unlikely(ptr != end)) { /* not last token */
        if (!create_parent) return_err_resolve(false, token - hdr);

        /* add value at last index if container is array */
        if (ctn_type == CTOON_TYPE_ARR) {
            if (!idx_is_last || !insert_new) {
                return_err_resolve(false, token - hdr);
            }
            val = ctoon_mut_obj(doc);
            if (!val) return_err_alloc(false);

            /* delay attaching until all operations are completed */
            sep_ctn = ctn;
            sep_key = NULL;
            sep_val = val;

            /* move to next token */
            ctn = val;
            val = NULL;
            ctn_type = CTOON_TYPE_OBJ;
            token = ptr_next_token(&ptr, end, &token_len, &esc);
            if (unlikely(!token)) return_err_resolve(false, token - hdr);
        }

        /* container is object, create parent nodes */
        while (ptr != end) { /* not last token */
            key = ptr_new_key(token, token_len, esc, doc);
            if (!key) return_err_alloc(false);
            val = ctoon_mut_obj(doc);
            if (!val) return_err_alloc(false);

            /* delay attaching until all operations are completed */
            if (!sep_ctn) {
                sep_ctn = ctn;
                sep_key = key;
                sep_val = val;
            } else {
                ctoon_mut_obj_add(ctn, key, val);
            }

            /* move to next token */
            ctn = val;
            val = NULL;
            token = ptr_next_token(&ptr, end, &token_len, &esc);
            if (unlikely(!token)) return_err_syntax(false, ptr - hdr);
        }
    }

    /* TOON pointer is resolved, insert or replace target value */
    ctn_len = unsafe_ctoon_get_len(ctn);
    if (ctn_type == CTOON_TYPE_OBJ) {
        if (ctx) ctx->ctn = ctn;
        if (!val || insert_new) {
            /* insert new key-value pair */
            key = ptr_new_key(token, token_len, esc, doc);
            if (unlikely(!key)) return_err_alloc(false);
            if (ctx) ctx->pre = ctn_len ? (ctoon_mut_val *)ctn->uni.ptr : key;
            unsafe_ctoon_mut_obj_add(ctn, key, new_val, ctn_len);
        } else {
            /* replace exist value */
            key = pre->next->next;
            if (ctx) ctx->pre = pre;
            if (ctx) ctx->old = val;
            ctoon_mut_obj_put(ctn, key, new_val);
        }
    } else {
        /* array */
        if (ctx && (val || idx_is_last)) ctx->ctn = ctn;
        if (insert_new) {
            /* append new value */
            if (val) {
                pre->next = new_val;
                new_val->next = val;
                if (ctx) ctx->pre = pre;
                unsafe_ctoon_set_len(ctn, ctn_len + 1);
            } else if (idx_is_last) {
                if (ctx) ctx->pre = ctn_len ?
                    (ctoon_mut_val *)ctn->uni.ptr : new_val;
                ctoon_mut_arr_append(ctn, new_val);
            } else {
                return_err_resolve(false, token - hdr);
            }
        } else {
            /* replace exist value */
            if (!val) return_err_resolve(false, token - hdr);
            if (ctn_len > 1) {
                new_val->next = val->next;
                pre->next = new_val;
                if (ctn->uni.ptr == val) ctn->uni.ptr = new_val;
            } else {
                new_val->next = new_val;
                ctn->uni.ptr = new_val;
                pre = new_val;
            }
            if (ctx) ctx->pre = pre;
            if (ctx) ctx->old = val;
        }
    }

    /* all operations are completed, attach the new components to the target */
    if (unlikely(sep_ctn)) {
        if (sep_key) ctoon_mut_obj_add(sep_ctn, sep_key, sep_val);
        else ctoon_mut_arr_append(sep_ctn, sep_val);
    }
    return true;
}

ctoon_mut_val *unsafe_ctoon_mut_ptr_replacex(
    ctoon_mut_val *val, const char *ptr, size_t len, ctoon_mut_val *new_val,
    ctoon_ptr_ctx *ctx, ctoon_ptr_err *err) {

    ctoon_mut_val *cur_val;
    ctoon_ptr_ctx cur_ctx;
    memset(&cur_ctx, 0, sizeof(cur_ctx));
    if (!ctx) ctx = &cur_ctx;
    cur_val = unsafe_ctoon_mut_ptr_getx(val, ptr, len, ctx, err);
    if (!cur_val) return NULL;

    if (ctoon_mut_is_obj(ctx->ctn)) {
        ctoon_mut_val *key = ctx->pre->next->next;
        ctoon_mut_obj_put(ctx->ctn, key, new_val);
    } else {
        ctoon_ptr_ctx_replace(ctx, new_val);
    }
    ctx->old = cur_val;
    return cur_val;
}

ctoon_mut_val *unsafe_ctoon_mut_ptr_removex(
    ctoon_mut_val *val, const char *ptr, size_t len,
    ctoon_ptr_ctx *ctx, ctoon_ptr_err *err) {

    ctoon_mut_val *cur_val;
    ctoon_ptr_ctx cur_ctx;
    memset(&cur_ctx, 0, sizeof(cur_ctx));
    if (!ctx) ctx = &cur_ctx;
    cur_val = unsafe_ctoon_mut_ptr_getx(val, ptr, len, ctx, err);
    if (cur_val) {
        if (ctoon_mut_is_obj(ctx->ctn)) {
            ctoon_mut_val *key = ctx->pre->next->next;
            ctoon_mut_obj_put(ctx->ctn, key, NULL);
        } else {
            ctoon_ptr_ctx_remove(ctx);
        }
        ctx->pre = NULL;
        ctx->old = cur_val;
    }
    return cur_val;
}

/* macros for ctoon_ptr */
#undef return_err
#undef return_err_resolve
#undef return_err_syntax
#undef return_err_alloc



/*==============================================================================
 * MARK: - TOON Patch API (RFC 6902) (Public)
 *============================================================================*/

/* TOON Patch operation */
typedef enum patch_op {
    PATCH_OP_ADD,       /* path, value */
    PATCH_OP_REMOVE,    /* path */
    PATCH_OP_REPLACE,   /* path, value */
    PATCH_OP_MOVE,      /* from, path */
    PATCH_OP_COPY,      /* from, path */
    PATCH_OP_TEST,      /* path, value */
    PATCH_OP_NONE       /* invalid */
} patch_op;

static patch_op patch_op_get(ctoon_val *op) {
    const char *str = op->uni.str;
    switch (unsafe_ctoon_get_len(op)) {
        case 3:
            if (!memcmp(str, "add", 3)) return PATCH_OP_ADD;
            return PATCH_OP_NONE;
        case 4:
            if (!memcmp(str, "move", 4)) return PATCH_OP_MOVE;
            if (!memcmp(str, "copy", 4)) return PATCH_OP_COPY;
            if (!memcmp(str, "test", 4)) return PATCH_OP_TEST;
            return PATCH_OP_NONE;
        case 6:
            if (!memcmp(str, "remove", 6)) return PATCH_OP_REMOVE;
            return PATCH_OP_NONE;
        case 7:
            if (!memcmp(str, "replace", 7)) return PATCH_OP_REPLACE;
            return PATCH_OP_NONE;
        default:
            return PATCH_OP_NONE;
    }
}

/* macros for ctoon_patch */
#define return_err(_code, _msg) do { \
    if (err->ptr.code == CTOON_PTR_ERR_MEMORY_ALLOCATION) { \
        err->code = CTOON_PATCH_ERROR_MEMORY_ALLOCATION; \
        err->msg = _msg; \
        memset(&err->ptr, 0, sizeof(ctoon_ptr_err)); \
    } else { \
        err->code = CTOON_PATCH_ERROR_##_code; \
        err->msg = _msg; \
        err->idx = iter.idx ? iter.idx - 1 : 0; \
    } \
    return NULL; \
} while (false)

#define return_err_copy() \
    return_err(MEMORY_ALLOCATION, "failed to copy value")
#define return_err_key(_key) \
    return_err(MISSING_KEY, "missing key " _key)
#define return_err_val(_key) \
    return_err(INVALID_MEMBER, "invalid member " _key)

#define ptr_get(_ptr) ctoon_mut_ptr_getx( \
    root, _ptr->uni.str, _ptr##_len, NULL, &err->ptr)
#define ptr_add(_ptr, _val) ctoon_mut_ptr_addx( \
    root, _ptr->uni.str, _ptr##_len, _val, doc, false, NULL, &err->ptr)
#define ptr_remove(_ptr) ctoon_mut_ptr_removex( \
    root, _ptr->uni.str, _ptr##_len, NULL, &err->ptr)
#define ptr_replace(_ptr, _val)ctoon_mut_ptr_replacex( \
    root, _ptr->uni.str, _ptr##_len, _val, NULL, &err->ptr)

ctoon_mut_val *ctoon_patch(ctoon_mut_doc *doc,
                             ctoon_val *orig,
                             ctoon_val *patch,
                             ctoon_patch_err *err) {

    ctoon_mut_val *root;
    ctoon_val *obj;
    ctoon_arr_iter iter;
    ctoon_patch_err err_tmp;
    if (!err) err = &err_tmp;
    memset(err, 0, sizeof(*err));
    memset(&iter, 0, sizeof(iter));

    if (unlikely(!doc || !orig || !patch)) {
        return_err(INVALID_PARAMETER, "input parameter is NULL");
    }
    if (unlikely(!ctoon_is_arr(patch))) {
        return_err(INVALID_PARAMETER, "input patch is not array");
    }
    root = ctoon_val_mut_copy(doc, orig);
    if (unlikely(!root)) return_err_copy();

    /* iterate through the patch array */
    ctoon_arr_iter_init(patch, &iter);
    while ((obj = ctoon_arr_iter_next(&iter))) {
        patch_op op_enum;
        ctoon_val *op, *path, *from = NULL, *value;
        ctoon_mut_val *val = NULL, *test;
        usize path_len, from_len = 0;
        if (unlikely(!unsafe_ctoon_is_obj(obj))) {
            return_err(INVALID_OPERATION, "TOON patch operation is not object");
        }

        /* get required member: op */
        op = ctoon_obj_get(obj, "op");
        if (unlikely(!op)) return_err_key("`op`");
        if (unlikely(!ctoon_is_str(op))) return_err_val("`op`");
        op_enum = patch_op_get(op);

        /* get required member: path */
        path = ctoon_obj_get(obj, "path");
        if (unlikely(!path)) return_err_key("`path`");
        if (unlikely(!ctoon_is_str(path))) return_err_val("`path`");
        path_len = unsafe_ctoon_get_len(path);

        /* get required member: value, from */
        switch ((int)op_enum) {
            case PATCH_OP_ADD: case PATCH_OP_REPLACE: case PATCH_OP_TEST:
                value = ctoon_obj_get(obj, "value");
                if (unlikely(!value)) return_err_key("`value`");
                val = ctoon_val_mut_copy(doc, value);
                if (unlikely(!val)) return_err_copy();
                break;
            case PATCH_OP_MOVE: case PATCH_OP_COPY:
                from = ctoon_obj_get(obj, "from");
                if (unlikely(!from)) return_err_key("`from`");
                if (unlikely(!ctoon_is_str(from))) return_err_val("`from`");
                from_len = unsafe_ctoon_get_len(from);
                break;
            default:
                break;
        }

        /* perform an operation */
        switch ((int)op_enum) {
            case PATCH_OP_ADD: /* add(path, val) */
                if (unlikely(path_len == 0)) { root = val; break; }
                if (unlikely(!ptr_add(path, val))) {
                    return_err(POINTER, "failed to add `path`");
                }
                break;
            case PATCH_OP_REMOVE: /* remove(path) */
                if (unlikely(!ptr_remove(path))) {
                    return_err(POINTER, "failed to remove `path`");
                }
                break;
            case PATCH_OP_REPLACE: /* replace(path, val) */
                if (unlikely(path_len == 0)) { root = val; break; }
                if (unlikely(!ptr_replace(path, val))) {
                    return_err(POINTER, "failed to replace `path`");
                }
                break;
            case PATCH_OP_MOVE: /* val = remove(from), add(path, val) */
                if (unlikely(from_len == 0 && path_len == 0)) break;
                val = ptr_remove(from);
                if (unlikely(!val)) {
                    return_err(POINTER, "failed to remove `from`");
                }
                if (unlikely(path_len == 0)) { root = val; break; }
                if (unlikely(!ptr_add(path, val))) {
                    return_err(POINTER, "failed to add `path`");
                }
                break;
            case PATCH_OP_COPY: /* val = get(from).copy, add(path, val) */
                val = ptr_get(from);
                if (unlikely(!val)) {
                    return_err(POINTER, "failed to get `from`");
                }
                if (unlikely(path_len == 0)) { root = val; break; }
                val = ctoon_mut_val_mut_copy(doc, val);
                if (unlikely(!val)) return_err_copy();
                if (unlikely(!ptr_add(path, val))) {
                    return_err(POINTER, "failed to add `path`");
                }
                break;
            case PATCH_OP_TEST: /* test = get(path), test.eq(val) */
                test = ptr_get(path);
                if (unlikely(!test)) {
                    return_err(POINTER, "failed to get `path`");
                }
                if (unlikely(!ctoon_mut_equals(val, test))) {
                    return_err(EQUAL, "failed to test equal");
                }
                break;
            default:
                return_err(INVALID_MEMBER, "unsupported `op`");
        }
    }
    return root;
}

ctoon_mut_val *ctoon_mut_patch(ctoon_mut_doc *doc,
                                 ctoon_mut_val *orig,
                                 ctoon_mut_val *patch,
                                 ctoon_patch_err *err) {
    ctoon_mut_val *root, *obj;
    ctoon_mut_arr_iter iter;
    ctoon_patch_err err_tmp;
    if (!err) err = &err_tmp;
    memset(err, 0, sizeof(*err));
    memset(&iter, 0, sizeof(iter));

    if (unlikely(!doc || !orig || !patch)) {
        return_err(INVALID_PARAMETER, "input parameter is NULL");
    }
    if (unlikely(!ctoon_mut_is_arr(patch))) {
        return_err(INVALID_PARAMETER, "input patch is not array");
    }
    root = ctoon_mut_val_mut_copy(doc, orig);
    if (unlikely(!root)) return_err_copy();

    /* iterate through the patch array */
    ctoon_mut_arr_iter_init(patch, &iter);
    while ((obj = ctoon_mut_arr_iter_next(&iter))) {
        patch_op op_enum;
        ctoon_mut_val *op, *path, *from = NULL, *value;
        ctoon_mut_val *val = NULL, *test;
        usize path_len, from_len = 0;
        if (!unsafe_ctoon_is_obj(obj)) {
            return_err(INVALID_OPERATION, "TOON patch operation is not object");
        }

        /* get required member: op */
        op = ctoon_mut_obj_get(obj, "op");
        if (unlikely(!op)) return_err_key("`op`");
        if (unlikely(!ctoon_mut_is_str(op))) return_err_val("`op`");
        op_enum = patch_op_get((ctoon_val *)(void *)op);

        /* get required member: path */
        path = ctoon_mut_obj_get(obj, "path");
        if (unlikely(!path)) return_err_key("`path`");
        if (unlikely(!ctoon_mut_is_str(path))) return_err_val("`path`");
        path_len = unsafe_ctoon_get_len(path);

        /* get required member: value, from */
        switch ((int)op_enum) {
            case PATCH_OP_ADD: case PATCH_OP_REPLACE: case PATCH_OP_TEST:
                value = ctoon_mut_obj_get(obj, "value");
                if (unlikely(!value)) return_err_key("`value`");
                val = ctoon_mut_val_mut_copy(doc, value);
                if (unlikely(!val)) return_err_copy();
                break;
            case PATCH_OP_MOVE: case PATCH_OP_COPY:
                from = ctoon_mut_obj_get(obj, "from");
                if (unlikely(!from)) return_err_key("`from`");
                if (unlikely(!ctoon_mut_is_str(from))) {
                    return_err_val("`from`");
                }
                from_len = unsafe_ctoon_get_len(from);
                break;
            default:
                break;
        }

        /* perform an operation */
        switch ((int)op_enum) {
            case PATCH_OP_ADD: /* add(path, val) */
                if (unlikely(path_len == 0)) { root = val; break; }
                if (unlikely(!ptr_add(path, val))) {
                    return_err(POINTER, "failed to add `path`");
                }
                break;
            case PATCH_OP_REMOVE: /* remove(path) */
                if (unlikely(!ptr_remove(path))) {
                    return_err(POINTER, "failed to remove `path`");
                }
                break;
            case PATCH_OP_REPLACE: /* replace(path, val) */
                if (unlikely(path_len == 0)) { root = val; break; }
                if (unlikely(!ptr_replace(path, val))) {
                    return_err(POINTER, "failed to replace `path`");
                }
                break;
            case PATCH_OP_MOVE: /* val = remove(from), add(path, val) */
                if (unlikely(from_len == 0 && path_len == 0)) break;
                val = ptr_remove(from);
                if (unlikely(!val)) {
                    return_err(POINTER, "failed to remove `from`");
                }
                if (unlikely(path_len == 0)) { root = val; break; }
                if (unlikely(!ptr_add(path, val))) {
                    return_err(POINTER, "failed to add `path`");
                }
                break;
            case PATCH_OP_COPY: /* val = get(from).copy, add(path, val) */
                val = ptr_get(from);
                if (unlikely(!val)) {
                    return_err(POINTER, "failed to get `from`");
                }
                if (unlikely(path_len == 0)) { root = val; break; }
                val = ctoon_mut_val_mut_copy(doc, val);
                if (unlikely(!val)) return_err_copy();
                if (unlikely(!ptr_add(path, val))) {
                    return_err(POINTER, "failed to add `path`");
                }
                break;
            case PATCH_OP_TEST: /* test = get(path), test.eq(val) */
                test = ptr_get(path);
                if (unlikely(!test)) {
                    return_err(POINTER, "failed to get `path`");
                }
                if (unlikely(!ctoon_mut_equals(val, test))) {
                    return_err(EQUAL, "failed to test equal");
                }
                break;
            default:
                return_err(INVALID_MEMBER, "unsupported `op`");
        }
    }
    return root;
}

/* macros for ctoon_patch */
#undef return_err
#undef return_err_copy
#undef return_err_key
#undef return_err_val
#undef ptr_get
#undef ptr_add
#undef ptr_remove
#undef ptr_replace



/*==============================================================================
 * MARK: - TOON Merge-Patch API (RFC 7386) (Public)
 *============================================================================*/

ctoon_mut_val *ctoon_merge_patch(ctoon_mut_doc *doc,
                                   ctoon_val *orig,
                                   ctoon_val *patch) {
    usize idx, max;
    ctoon_val *key, *orig_val, *patch_val, local_orig;
    ctoon_mut_val *builder, *mut_key, *mut_val, *merged_val;

    if (unlikely(!ctoon_is_obj(patch))) {
        return ctoon_val_mut_copy(doc, patch);
    }

    builder = ctoon_mut_obj(doc);
    if (unlikely(!builder)) return NULL;

    memset(&local_orig, 0, sizeof(local_orig));
    if (!ctoon_is_obj(orig)) {
        orig = &local_orig;
        orig->tag = builder->tag;
        orig->uni = builder->uni;
    }

    /* If orig is contributing, copy any items not modified by the patch */
    if (orig != &local_orig) {
        ctoon_obj_foreach(orig, idx, max, key, orig_val) {
            patch_val = ctoon_obj_getn(patch,
                                        unsafe_ctoon_get_str(key),
                                        unsafe_ctoon_get_len(key));
            if (!patch_val) {
                mut_key = ctoon_val_mut_copy(doc, key);
                mut_val = ctoon_val_mut_copy(doc, orig_val);
                if (!ctoon_mut_obj_add(builder, mut_key, mut_val)) return NULL;
            }
        }
    }

    /* Merge items modified by the patch. */
    ctoon_obj_foreach(patch, idx, max, key, patch_val) {
        /* null indicates the field is removed. */
        if (unsafe_ctoon_is_null(patch_val)) {
            continue;
        }
        mut_key = ctoon_val_mut_copy(doc, key);
        orig_val = ctoon_obj_getn(orig,
                                   unsafe_ctoon_get_str(key),
                                   unsafe_ctoon_get_len(key));
        merged_val = ctoon_merge_patch(doc, orig_val, patch_val);
        if (!ctoon_mut_obj_add(builder, mut_key, merged_val)) return NULL;
    }

    return builder;
}

ctoon_mut_val *ctoon_mut_merge_patch(ctoon_mut_doc *doc,
                                       ctoon_mut_val *orig,
                                       ctoon_mut_val *patch) {
    usize idx, max;
    ctoon_mut_val *key, *orig_val, *patch_val, local_orig;
    ctoon_mut_val *builder, *mut_key, *mut_val, *merged_val;

    if (unlikely(!ctoon_mut_is_obj(patch))) {
        return ctoon_mut_val_mut_copy(doc, patch);
    }

    builder = ctoon_mut_obj(doc);
    if (unlikely(!builder)) return NULL;

    memset(&local_orig, 0, sizeof(local_orig));
    if (!ctoon_mut_is_obj(orig)) {
        orig = &local_orig;
        orig->tag = builder->tag;
        orig->uni = builder->uni;
    }

    /* If orig is contributing, copy any items not modified by the patch */
    if (orig != &local_orig) {
        ctoon_mut_obj_foreach(orig, idx, max, key, orig_val) {
            patch_val = ctoon_mut_obj_getn(patch,
                                            unsafe_ctoon_get_str(key),
                                            unsafe_ctoon_get_len(key));
            if (!patch_val) {
                mut_key = ctoon_mut_val_mut_copy(doc, key);
                mut_val = ctoon_mut_val_mut_copy(doc, orig_val);
                if (!ctoon_mut_obj_add(builder, mut_key, mut_val)) return NULL;
            }
        }
    }

    /* Merge items modified by the patch. */
    ctoon_mut_obj_foreach(patch, idx, max, key, patch_val) {
        /* null indicates the field is removed. */
        if (unsafe_ctoon_is_null(patch_val)) {
            continue;
        }
        mut_key = ctoon_mut_val_mut_copy(doc, key);
        orig_val = ctoon_mut_obj_getn(orig,
                                       unsafe_ctoon_get_str(key),
                                       unsafe_ctoon_get_len(key));
        merged_val = ctoon_mut_merge_patch(doc, orig_val, patch_val);
        if (!ctoon_mut_obj_add(builder, mut_key, merged_val)) return NULL;
    }

    return builder;
}

#endif /* CTOON_DISABLE_UTILS */
