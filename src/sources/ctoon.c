/*==============================================================================
 |  CToon C Library – Implementation
 |  Copyright (c) 2026 CToon Project, MIT License
 *============================================================================*/

/* Enable POSIX extensions for strdup */
#define _POSIX_C_SOURCE 200809L

#include "../../include/ctoon.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

/* ============================================================
 * Design: ctoon_val IS the node – no wrapper needed
 *
 * The public typedef  struct ctoon_val  is defined here as the
 * actual node struct.  This keeps ctoon_val* valid across multiple
 * lookups without aliasing bugs from a shared temporary.
 * ============================================================ */

/* ============================================================
 * Arena allocator
 * ============================================================ */

#define CTOON_ARENA_CHUNK_SIZE (64u * 1024u)

typedef struct ctoon_arena_chunk {
    struct ctoon_arena_chunk *next;
    size_t used;
    size_t cap;
} ctoon_arena_chunk;

typedef struct {
    ctoon_arena_chunk *head;
    size_t total_allocated;
    size_t max_memory;
} ctoon_arena;

static ctoon_arena_chunk *arena_chunk_new(size_t min_size) {
    size_t cap = (min_size > CTOON_ARENA_CHUNK_SIZE) ? min_size : CTOON_ARENA_CHUNK_SIZE;
    ctoon_arena_chunk *c = (ctoon_arena_chunk *)calloc(1, sizeof(ctoon_arena_chunk) + cap);
    if (!c) return NULL;
    c->next = NULL;
    c->used = 0;
    c->cap  = cap;
    return c;
}

static void *arena_alloc(ctoon_arena *a, size_t size) {
    size_t align = sizeof(void *);
    size = (size + align - 1u) & ~(align - 1u);

    if (a->max_memory && a->total_allocated + size > a->max_memory) return NULL;

    if (!a->head || a->head->used + size > a->head->cap) {
        ctoon_arena_chunk *c = arena_chunk_new(size);
        if (!c) return NULL;
        c->next              = a->head;
        a->head              = c;
        a->total_allocated  += c->cap;
    }

    void *ptr = (char *)(a->head + 1) + a->head->used;
    a->head->used += size;
    return ptr;
}

static void arena_free_all(ctoon_arena *a) {
    ctoon_arena_chunk *c = a->head;
    while (c) {
        ctoon_arena_chunk *next = c->next;
        free(c);
        c = next;
    }
    a->head             = NULL;
    a->total_allocated  = 0;
}

/* ============================================================
 * ctoon_val (node) definition
 *
 * The public header forward-declares  struct ctoon_val.
 * We define it here so it is fully opaque to callers.
 * ============================================================ */

typedef struct ctoon_kv {
    struct ctoon_val *key;   /* always CTOON_TYPE_STRING */
    struct ctoon_val *value;
} ctoon_kv;

struct ctoon_val {
    ctoon_type type;
    union {
        /* CTOON_TYPE_NUMBER
         * num/u64/s64 share the same 8 bytes; subtype selects which is active.
         * f64 naming (IEEE 754 double) preferred over 'num' for clarity. */
        struct {
            union {
                double   f64; /**< CTOON_SUBTYPE_REAL  */
                uint64_t u64; /**< CTOON_SUBTYPE_UINT  */
                int64_t  s64; /**< CTOON_SUBTYPE_SINT  */
            };
            uint8_t subtype; /**< ctoon_subtype tag */
        } number;
        /* CTOON_TYPE_STRING / CTOON_TYPE_RAW */
        struct {
            const char *ptr;
            size_t      len;
        } str;
        /* CTOON_TYPE_ARRAY */
        struct {
            struct ctoon_val **items;
            size_t             count;
            size_t             cap;
        } arr;
        /* CTOON_TYPE_OBJECT */
        struct {
            ctoon_kv *pairs;
            size_t    count;
            size_t    cap;
        } obj;
    } u;
};

struct ctoon_doc {
    ctoon_arena   arena;
    ctoon_val    *root;
    char         *error_msg;
    size_t        error_pos;
    size_t        val_count;
};

/* ============================================================
 * Document
 * ============================================================ */

ctoon_doc *ctoon_doc_new(size_t max_memory) {
    ctoon_doc *doc = (ctoon_doc *)calloc(1, sizeof(ctoon_doc));
    if (!doc) return NULL;
    doc->arena.max_memory = max_memory;
    return doc;
}

void ctoon_doc_free(ctoon_doc *doc) {
    if (!doc) return;
    arena_free_all(&doc->arena);
    free(doc->error_msg);
    free(doc);
}

ctoon_val *ctoon_doc_get_root(ctoon_doc *doc) {
    return doc ? doc->root : NULL;
}

void ctoon_doc_set_root(ctoon_doc *doc, ctoon_val *root) {
    if (doc) doc->root = root;
}

size_t ctoon_doc_get_val_count(ctoon_doc *doc) {
    return doc ? doc->val_count : 0;
}

const char *ctoon_doc_get_error(ctoon_doc *doc) {
    return doc ? doc->error_msg : NULL;
}

size_t ctoon_doc_get_error_pos(ctoon_doc *doc) {
    return doc ? doc->error_pos : 0;
}

/* ============================================================
 * Node allocation
 * ============================================================ */

static ctoon_val *node_new(ctoon_doc *doc, ctoon_type type) {
    ctoon_val *n = (ctoon_val *)arena_alloc(&doc->arena, sizeof(ctoon_val));
    if (!n) return NULL;
    n->type = type;
    doc->val_count++;
    return n;
}

static char *arena_strdup(ctoon_doc *doc, const char *s, size_t len) {
    char *buf = (char *)arena_alloc(&doc->arena, len + 1u);
    if (!buf) return NULL;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

/* ============================================================
 * Type queries
 * ============================================================ */

ctoon_type ctoon_get_type(ctoon_val *val) {
    return val ? val->type : CTOON_TYPE_NULL;
}

ctoon_subtype ctoon_get_subtype(ctoon_val *val) {
    if (!val || val->type != CTOON_TYPE_NUMBER) return CTOON_SUBTYPE_NONE;
    return (ctoon_subtype)val->u.number.subtype;
}

bool ctoon_is_null  (ctoon_val *v) { return ctoon_get_type(v) == CTOON_TYPE_NULL;   }
bool ctoon_is_true  (ctoon_val *v) { return ctoon_get_type(v) == CTOON_TYPE_TRUE;   }
bool ctoon_is_false (ctoon_val *v) { return ctoon_get_type(v) == CTOON_TYPE_FALSE;  }
bool ctoon_is_bool  (ctoon_val *v) { ctoon_type t = ctoon_get_type(v); return t == CTOON_TYPE_TRUE || t == CTOON_TYPE_FALSE; }
bool ctoon_is_num   (ctoon_val *v) { return ctoon_get_type(v) == CTOON_TYPE_NUMBER; }
bool ctoon_is_str   (ctoon_val *v) { return ctoon_get_type(v) == CTOON_TYPE_STRING; }
bool ctoon_is_arr   (ctoon_val *v) { return ctoon_get_type(v) == CTOON_TYPE_ARRAY;  }
bool ctoon_is_obj   (ctoon_val *v) { return ctoon_get_type(v) == CTOON_TYPE_OBJECT; }
bool ctoon_is_ctn   (ctoon_val *v) { return ctoon_is_arr(v) || ctoon_is_obj(v);     }
bool ctoon_is_raw   (ctoon_val *v) { return ctoon_get_type(v) == CTOON_TYPE_RAW;    }

bool ctoon_is_uint(ctoon_val *v) { return ctoon_is_num(v) && ctoon_get_subtype(v) == CTOON_SUBTYPE_UINT; }
bool ctoon_is_sint(ctoon_val *v) { return ctoon_is_num(v) && ctoon_get_subtype(v) == CTOON_SUBTYPE_SINT; }
bool ctoon_is_int (ctoon_val *v) {
    if (!ctoon_is_num(v)) return false;
    ctoon_subtype st = ctoon_get_subtype(v);
    return st == CTOON_SUBTYPE_UINT || st == CTOON_SUBTYPE_SINT;
}
bool ctoon_is_real(ctoon_val *v) { return ctoon_is_num(v) && ctoon_get_subtype(v) == CTOON_SUBTYPE_REAL; }

/* ============================================================
 * Value accessors
 * ============================================================ */

bool        ctoon_get_bool(ctoon_val *v) { return ctoon_is_true(v); }
uint64_t    ctoon_get_uint(ctoon_val *v) {
    if (!ctoon_is_num(v)) return 0;
    switch ((ctoon_subtype)v->u.number.subtype) {
        case CTOON_SUBTYPE_SINT: return (uint64_t)v->u.number.s64;
        case CTOON_SUBTYPE_REAL: return (uint64_t)v->u.number.f64;
        default:                 return v->u.number.u64;
    }
}
int64_t     ctoon_get_sint(ctoon_val *v) {
    if (!ctoon_is_num(v)) return 0;
    switch ((ctoon_subtype)v->u.number.subtype) {
        case CTOON_SUBTYPE_UINT: return (int64_t)v->u.number.u64;
        case CTOON_SUBTYPE_REAL: return (int64_t)v->u.number.f64;
        default:                 return v->u.number.s64;
    }
}
int         ctoon_get_int (ctoon_val *v) { return (!ctoon_is_num(v)) ? 0 : (int)v->u.number.s64; }
double      ctoon_get_real(ctoon_val *v) {
    if (!ctoon_is_num(v)) return 0.0;
    switch ((ctoon_subtype)v->u.number.subtype) {
        case CTOON_SUBTYPE_UINT: return (double)v->u.number.u64;
        case CTOON_SUBTYPE_SINT: return (double)v->u.number.s64;
        default:                 return v->u.number.f64;
    }
}
double      ctoon_get_num (ctoon_val *v) { return ctoon_get_real(v); }

const char *ctoon_get_str(ctoon_val *v) { return ctoon_is_str(v) ? v->u.str.ptr : NULL; }
size_t      ctoon_get_len(ctoon_val *v) { return ctoon_is_str(v) ? v->u.str.len : 0;    }

bool ctoon_equals_str(ctoon_val *v, const char *s) {
    if (!ctoon_is_str(v) || !s) return false;
    return strcmp(v->u.str.ptr, s) == 0;
}
bool ctoon_equals_strn(ctoon_val *v, const char *s, size_t len) {
    if (!ctoon_is_str(v) || !s) return false;
    return v->u.str.len == len && memcmp(v->u.str.ptr, s, len) == 0;
}
const char *ctoon_get_raw(ctoon_val *v) { return ctoon_is_raw(v) ? v->u.str.ptr : NULL; }

/* ============================================================
 * Array access
 * ============================================================ */

size_t ctoon_arr_size(ctoon_val *v) { return ctoon_is_arr(v) ? v->u.arr.count : 0; }

ctoon_val *ctoon_arr_get(ctoon_val *v, size_t idx) {
    if (!ctoon_is_arr(v) || idx >= v->u.arr.count) return NULL;
    return v->u.arr.items[idx];
}

ctoon_val *ctoon_arr_get_first(ctoon_val *v) { return ctoon_arr_get(v, 0); }

static _Thread_local struct { ctoon_val *arr; size_t idx; } s_arr_iter;

ctoon_val *ctoon_arr_get_next(ctoon_val *v) {
    if (!ctoon_is_arr(v)) return NULL;
    if (s_arr_iter.arr != v) { s_arr_iter.arr = v; s_arr_iter.idx = 0; }
    if (s_arr_iter.idx >= v->u.arr.count) { s_arr_iter.arr = NULL; s_arr_iter.idx = 0; return NULL; }
    return v->u.arr.items[s_arr_iter.idx++];
}

/* ============================================================
 * Object access
 * ============================================================ */

size_t ctoon_obj_size(ctoon_val *v) { return ctoon_is_obj(v) ? v->u.obj.count : 0; }

ctoon_val *ctoon_obj_get(ctoon_val *v, const char *key) {
    return (key) ? ctoon_obj_getn(v, key, strlen(key)) : NULL;
}

ctoon_val *ctoon_obj_getn(ctoon_val *v, const char *key, size_t len) {
    if (!ctoon_is_obj(v) || !key) return NULL;
    for (size_t i = 0; i < v->u.obj.count; i++) {
        ctoon_val *k = v->u.obj.pairs[i].key;
        if (k->u.str.len == len && memcmp(k->u.str.ptr, key, len) == 0)
            return v->u.obj.pairs[i].value;
    }
    return NULL;
}

static _Thread_local struct { ctoon_val *obj; size_t idx; } s_obj_iter;

ctoon_val *ctoon_obj_iter_get(ctoon_val *v, ctoon_val **key) {
    if (!ctoon_is_obj(v) || v->u.obj.count == 0) return NULL;
    s_obj_iter.obj = v;
    s_obj_iter.idx = 1;
    if (key) *key = v->u.obj.pairs[0].key;
    return v->u.obj.pairs[0].value;
}

ctoon_val *ctoon_obj_iter_next(ctoon_val *v, ctoon_val **key) {
    if (!ctoon_is_obj(v)) return NULL;
    if (s_obj_iter.obj != v) { s_obj_iter.obj = v; s_obj_iter.idx = 0; }
    if (s_obj_iter.idx >= v->u.obj.count) { s_obj_iter.obj = NULL; s_obj_iter.idx = 0; return NULL; }
    size_t i = s_obj_iter.idx++;
    if (key) *key = v->u.obj.pairs[i].key;
    return v->u.obj.pairs[i].value;
}

/* ============================================================
 * Value construction
 * ============================================================ */

ctoon_val *ctoon_new_null  (ctoon_doc *d) { return node_new(d, CTOON_TYPE_NULL); }
ctoon_val *ctoon_new_bool  (ctoon_doc *d, bool v) { return node_new(d, v ? CTOON_TYPE_TRUE : CTOON_TYPE_FALSE); }

static ctoon_val *new_num(ctoon_doc *d, double f64, int64_t s64, uint64_t u64, ctoon_subtype st) {
    ctoon_val *n = node_new(d, CTOON_TYPE_NUMBER);
    if (!n) return NULL;
    switch (st) {
        case CTOON_SUBTYPE_UINT: n->u.number.u64 = u64; break;
        case CTOON_SUBTYPE_SINT: n->u.number.s64 = s64; break;
        default:                 n->u.number.f64 = f64; break;
    }
    n->u.number.subtype = (uint8_t)st;
    return n;
}
ctoon_val *ctoon_new_uint(ctoon_doc *d, uint64_t v) { return new_num(d, (double)v, (int64_t)v, v, CTOON_SUBTYPE_UINT); }
ctoon_val *ctoon_new_sint(ctoon_doc *d, int64_t  v) {
    ctoon_subtype st = (v >= 0) ? CTOON_SUBTYPE_UINT : CTOON_SUBTYPE_SINT;
    return new_num(d, (double)v, v, (uint64_t)v, st);
}
ctoon_val *ctoon_new_real(ctoon_doc *d, double v) { return new_num(d, v, (int64_t)v, (uint64_t)(v>0?v:0), CTOON_SUBTYPE_REAL); }

ctoon_val *ctoon_new_strn(ctoon_doc *d, const char *v, size_t len) {
    if (!v) return ctoon_new_null(d);
    ctoon_val *n = node_new(d, CTOON_TYPE_STRING);
    if (!n) return NULL;
    char *buf = arena_strdup(d, v, len);
    if (!buf) return NULL;
    n->u.str.ptr = buf;
    n->u.str.len = len;
    return n;
}
ctoon_val *ctoon_new_str(ctoon_doc *d, const char *v) { return v ? ctoon_new_strn(d, v, strlen(v)) : ctoon_new_null(d); }
ctoon_val *ctoon_new_arr (ctoon_doc *d) { return node_new(d, CTOON_TYPE_ARRAY);  }
ctoon_val *ctoon_new_obj(ctoon_doc *d) { return node_new(d, CTOON_TYPE_OBJECT); }

bool ctoon_arr_append(ctoon_doc *doc, ctoon_val *arr, ctoon_val *val) {
    if (!doc || !arr || !val || arr->type != CTOON_TYPE_ARRAY) return false;
    if (arr->u.arr.count >= arr->u.arr.cap) {
        size_t nc = arr->u.arr.cap ? arr->u.arr.cap * 2u : 4u;
        ctoon_val **ni = (ctoon_val **)arena_alloc(&doc->arena, sizeof(ctoon_val *) * nc);
        if (!ni) return false;
        if (arr->u.arr.items)
            memcpy(ni, arr->u.arr.items, sizeof(ctoon_val *) * arr->u.arr.count);
        arr->u.arr.items = ni;
        arr->u.arr.cap   = nc;
    }
    arr->u.arr.items[arr->u.arr.count++] = val;
    return true;
}

bool ctoon_obj_setn(ctoon_doc *doc, ctoon_val *obj,
                       const char *key, size_t key_len, ctoon_val *val) {
    if (!doc || !obj || !key || !val || obj->type != CTOON_TYPE_OBJECT) return false;
    /* update existing */
    for (size_t i = 0; i < obj->u.obj.count; i++) {
        ctoon_val *k = obj->u.obj.pairs[i].key;
        if (k->u.str.len == key_len && memcmp(k->u.str.ptr, key, key_len) == 0) {
            obj->u.obj.pairs[i].value = val;
            return true;
        }
    }
    /* new entry */
    if (obj->u.obj.count >= obj->u.obj.cap) {
        size_t nc = obj->u.obj.cap ? obj->u.obj.cap * 2u : 4u;
        ctoon_kv *np = (ctoon_kv *)arena_alloc(&doc->arena, sizeof(ctoon_kv) * nc);
        if (!np) return false;
        if (obj->u.obj.pairs)
            memcpy(np, obj->u.obj.pairs, sizeof(ctoon_kv) * obj->u.obj.count);
        obj->u.obj.pairs = np;
        obj->u.obj.cap   = nc;
    }
    ctoon_val *kn = node_new(doc, CTOON_TYPE_STRING);
    if (!kn) return false;
    char *buf = arena_strdup(doc, key, key_len);
    if (!buf) return false;
    kn->u.str.ptr = buf;
    kn->u.str.len = key_len;
    obj->u.obj.pairs[obj->u.obj.count].key   = kn;
    obj->u.obj.pairs[obj->u.obj.count].value = val;
    obj->u.obj.count++;
    return true;
}

bool ctoon_obj_set(ctoon_doc *doc, ctoon_val *obj, const char *key, ctoon_val *val) {
    return key ? ctoon_obj_setn(doc, obj, key, strlen(key), val) : false;
}

/* ============================================================
 * TOON Parser
 * ============================================================ */

#define TOON_INDENT 2

typedef struct {
    const char *src;
    size_t      len;
    size_t      pos;
    ctoon_doc  *doc;
    int         indent;       /* spaces per indentation level (default 2) */
    char        active_delim; /* current array delimiter: ',', '|', or '\t' */
} tp;

static bool   tp_eof  (const tp *p) { return p->pos >= p->len; }
static char   tp_peek (const tp *p) { return tp_eof(p) ? '\0' : p->src[p->pos]; }
static char   tp_next (tp *p)       { return tp_eof(p) ? '\0' : p->src[p->pos++]; }

static void tp_skip_spaces(tp *p) {
    while (!tp_eof(p) && p->src[p->pos] == ' ') p->pos++;
}
static void tp_skip_to_eol(tp *p) {
    while (!tp_eof(p) && p->src[p->pos] != '\n') p->pos++;
}
static void tp_skip_nl(tp *p) {
    if (!tp_eof(p) && p->src[p->pos] == '\r') p->pos++;
    if (!tp_eof(p) && p->src[p->pos] == '\n') p->pos++;
}
static bool tp_is_eol(const tp *p) {
    return tp_eof(p) || p->src[p->pos] == '\n' || p->src[p->pos] == '\r';
}
static int tp_measure_indent(const tp *p) {
    size_t i = p->pos;
    while (i < p->len && p->src[i] == ' ') i++;
    return (int)((i - p->pos) / p->indent);
}
static bool tp_consume_indent(tp *p, int depth) {
    int sp = depth * p->indent;
    for (int i = 0; i < sp; i++) {
        if (tp_eof(p) || p->src[p->pos] != ' ') return false;
        p->pos++;
    }
    return true;
}
static bool tp_set_error(tp *p, const char *msg) {
    if (!p->doc->error_msg) {
        p->doc->error_msg = strdup(msg);
        p->doc->error_pos = p->pos;
    }
    return false;
}

/* ---- string unescape ---------------------------------------- */

static char *tp_parse_quoted_str(tp *p, size_t *out_len) {
    if (tp_peek(p) != '"') return NULL;
    p->pos++;
    /* measure output length */
    size_t raw_len = 0;
    {
        size_t i = p->pos;
        while (i < p->len && p->src[i] != '"' && p->src[i] != '\n') {
            if (p->src[i] == '\\') i++;
            raw_len++;
            i++;
        }
    }
    char *buf = (char *)arena_alloc(&p->doc->arena, raw_len + 1u);
    if (!buf) return NULL;
    size_t j = 0;
    while (!tp_eof(p) && tp_peek(p) != '"' && tp_peek(p) != '\n') {
        char c = tp_next(p);
        if (c == '\\') {
            char e = tp_next(p);
            switch (e) {
                case 'n': buf[j++] = '\n'; break;
                case 'r': buf[j++] = '\r'; break;
                case 't': buf[j++] = '\t'; break;
                case '\\': buf[j++] = '\\'; break;
                case '"': buf[j++] = '"'; break;
                default: tp_set_error(p, "Invalid escape sequence"); return NULL;
            }
        } else { buf[j++] = c; }
    }
    buf[j] = '\0';
    if (out_len) *out_len = j;
    if (tp_peek(p) != '"') { tp_set_error(p, "Unterminated string"); return NULL; }
    p->pos++;
    return buf;
}

/* ---- primitive token ---------------------------------------- */

static ctoon_val *tp_parse_primitive(tp *p, const char *stop_chars) {
    tp_skip_spaces(p);
    if (tp_is_eol(p)) { tp_set_error(p, "Expected value"); return NULL; }

    if (tp_peek(p) == '"') {
        size_t slen = 0;
        char *str = tp_parse_quoted_str(p, &slen);
        if (!str) return NULL;
        ctoon_val *n = node_new(p->doc, CTOON_TYPE_STRING);
        if (!n) return NULL;
        n->u.str.ptr = str;
        n->u.str.len = slen;
        return n;
    }

    size_t start = p->pos;
    while (!tp_eof(p) && p->src[p->pos] != '\n' && p->src[p->pos] != '\r') {
        char c = p->src[p->pos];
        if (stop_chars && strchr(stop_chars, c)) break;
        p->pos++;
    }
    size_t tok_len = p->pos - start;
    while (tok_len > 0 && p->src[start + tok_len - 1] == ' ') tok_len--;
    if (tok_len == 0) { tp_set_error(p, "Expected value"); return NULL; }

    const char *tok = p->src + start;

    if (tok_len == 4 && memcmp(tok, "null",  4) == 0) return node_new(p->doc, CTOON_TYPE_NULL);
    if (tok_len == 4 && memcmp(tok, "true",  4) == 0) return node_new(p->doc, CTOON_TYPE_TRUE);
    if (tok_len == 5 && memcmp(tok, "false", 5) == 0) return node_new(p->doc, CTOON_TYPE_FALSE);

    /* try number */
    char tmp[64];
    if (tok_len < sizeof(tmp)) {
        memcpy(tmp, tok, tok_len); tmp[tok_len] = '\0';

        /* integer check: optional leading minus, then only digits */
        bool is_int = true;
        size_t di = (tmp[0] == '-') ? 1u : 0u;
        if (di == tok_len) is_int = false;
        for (size_t k = di; k < tok_len && is_int; k++)
            if (!isdigit((unsigned char)tmp[k])) is_int = false;

        if (is_int) {
            errno = 0;
            char *end = NULL;
            long long iv = strtoll(tmp, &end, 10);
            if (end == tmp + tok_len && errno == 0) {
                ctoon_val *n = node_new(p->doc, CTOON_TYPE_NUMBER);
                if (!n) return NULL;
                if (iv >= 0) n->u.number.u64 = (uint64_t)iv;
                else        n->u.number.s64 = (int64_t)iv;
                n->u.number.subtype = (uint8_t)(iv >= 0 ? CTOON_SUBTYPE_UINT : CTOON_SUBTYPE_SINT);
                return n;
            }
        }

        char *end = NULL;
        double dv = strtod(tmp, &end);
        if (end == tmp + tok_len) {
            ctoon_val *n = node_new(p->doc, CTOON_TYPE_NUMBER);
            if (!n) return NULL;
            n->u.number.f64     = dv;
            n->u.number.subtype = (uint8_t)CTOON_SUBTYPE_REAL;
            return n;
        }
    }

    /* unquoted string */
    ctoon_val *n = node_new(p->doc, CTOON_TYPE_STRING);
    if (!n) return NULL;
    char *buf = arena_strdup(p->doc, tok, tok_len);
    if (!buf) return NULL;
    n->u.str.ptr = buf;
    n->u.str.len = tok_len;
    return n;
}

/* ---- forward declarations ----------------------------------- */
static ctoon_val *tp_parse_object    (tp *p, int depth);
static ctoon_val *tp_parse_array_body(tp *p, int arr_depth);

/* ---- inline array ------------------------------------------- */

static ctoon_val *tp_parse_inline_array(tp *p) {
    ctoon_val *arr = node_new(p->doc, CTOON_TYPE_ARRAY);
    if (!arr) return NULL;
    tp_skip_spaces(p);
    if (tp_is_eol(p)) return arr;
    while (!tp_is_eol(p)) {
        tp_skip_spaces(p);
        if (tp_is_eol(p)) break;
        char stop_d[2] = { p->active_delim, '\0' };
        ctoon_val *item = tp_parse_primitive(p, stop_d);
        if (!item) return NULL;
        if (!ctoon_arr_append(p->doc, arr, item)) return NULL;
        tp_skip_spaces(p);
        if (tp_peek(p) == p->active_delim) { p->pos++; tp_skip_spaces(p); }
        else break;
    }
    return arr;
}

/* ---- tabular rows ------------------------------------------- */

static bool tp_parse_tabular_rows(tp *p, int depth, long expected,
                                    const char **fields, int nfields,
                                    ctoon_val *arr) {
    long parsed = 0;
    while (parsed < expected) {
        if (tp_measure_indent(p) != depth) break;
        size_t save = p->pos;
        if (!tp_consume_indent(p, depth)) { p->pos = save; break; }

        ctoon_val *obj = node_new(p->doc, CTOON_TYPE_OBJECT);
        if (!obj) return false;

        char tab_delim[2] = { p->active_delim, '\0' };
        for (int fi = 0; fi < nfields; fi++) {
            tp_skip_spaces(p);
            ctoon_val *item = tp_parse_primitive(p, tab_delim);
            if (!item) return false;
            if (fi < nfields - 1) {
                if (tp_peek(p) != p->active_delim) {
                    tp_set_error(p, "Expected delimiter"); return false;
                }
                p->pos++;
            }
            if (!ctoon_obj_setn(p->doc, obj, fields[fi], strlen(fields[fi]), item)) return false;
        }

        tp_skip_to_eol(p); tp_skip_nl(p);
        if (!ctoon_arr_append(p->doc, arr, obj)) return false;
        parsed++;
    }
    return true;
}

/* ---- list items --------------------------------------------- */

static bool tp_parse_list_items(tp *p, int depth, ctoon_val *arr) {
    /* depth = indentation level of "- " markers.
     * Continuation fields of an object-item are at depth+1. */
    while (!tp_eof(p)) {
        /* skip blank lines */
        {
            bool blank = true;
            for (size_t i = p->pos; i < p->len && p->src[i] != '\n' && p->src[i] != '\r'; i++)
                if (p->src[i] != ' ') { blank = false; break; }
            if (blank) { tp_skip_to_eol(p); tp_skip_nl(p); continue; }
        }

        int ind = tp_measure_indent(p);
        if (ind != depth) break;

        size_t save = p->pos;
        if (!tp_consume_indent(p, depth)) { p->pos = save; break; }
        if (p->pos + 1u >= p->len || p->src[p->pos] != '-' || p->src[p->pos + 1] != ' ') {
            p->pos = save;
            break;
        }
        p->pos += 2; /* skip "- " */
        tp_skip_spaces(p);

        /* Determine item type by looking for colon or array bracket on this line */
        bool item_has_colon = false;
        bool item_has_bracket = false;
        {
            bool iq = false;
            for (size_t i = p->pos; i < p->len && p->src[i] != '\n' && p->src[i] != '\r'; i++) {
                if (p->src[i] == '"'  ) iq = !iq;
                if (!iq && p->src[i] == ':') { item_has_colon   = true; break; }
                if (!iq && p->src[i] == '[') { item_has_bracket = true; break; }
            }
        }

        ctoon_val *item = NULL;

        if (!item_has_colon && !item_has_bracket) {
            /* Primitive list item */
            item = tp_parse_primitive(p, NULL);
            if (!item) return false;
            if (!tp_is_eol(p)) tp_skip_to_eol(p);
            tp_skip_nl(p);
        } else {
            /* Object list item: collect all fields.
             * First field is on the same line as "- ".
             * Continuation fields are on subsequent lines at depth+1. */
            item = node_new(p->doc, CTOON_TYPE_OBJECT);
            if (!item) return false;

            bool first_field = true;
            while (true) {
                if (first_field) {
                    /* Already consumed indent + "- ", sitting on first key */
                    first_field = false;
                } else {
                    /* Skip blank lines */
                    bool done = false;
                    for (;;) {
                        if (tp_eof(p)) { done = true; break; }
                        bool blank = true;
                        for (size_t i = p->pos; i < p->len && p->src[i] != '\n' && p->src[i] != '\r'; i++)
                            if (p->src[i] != ' ') { blank = false; break; }
                        if (!blank) break;
                        tp_skip_to_eol(p); tp_skip_nl(p);
                    }
                    if (done) break;

                    /* Continuation field must be at depth+1 and NOT "- " */
                    if (tp_measure_indent(p) != depth + 1) break;
                    size_t cont_save = p->pos;
                    if (!tp_consume_indent(p, depth + 1)) { p->pos = cont_save; break; }
                    if (p->pos + 1u < p->len && p->src[p->pos] == '-' && p->src[p->pos + 1] == ' ') {
                        p->pos = cont_save;
                        break;
                    }
                }

                /* Parse one key */
                char *key = NULL; size_t kl = 0;
                if (tp_peek(p) == '"'  ) {
                    key = tp_parse_quoted_str(p, &kl);
                    if (!key) return false;
                } else {
                    size_t ks = p->pos;
                    while (!tp_eof(p) && p->src[p->pos] != ':' &&
                           p->src[p->pos] != '[' && p->src[p->pos] != '\n') p->pos++;
                    kl = p->pos - ks;
                    while (kl > 0 && p->src[ks + kl - 1] == ' ') kl--;
                    key = arena_strdup(p->doc, p->src + ks, kl);
                    if (!key) return false;
                }

                tp_skip_spaces(p);
                ctoon_val *val = NULL;
                int field_depth = depth + 1;

                if (tp_peek(p) == '[') {
                    p->pos++;
                    val = tp_parse_array_body(p, field_depth);
                    if (!val) return false;
                } else if (tp_peek(p) == ':') {
                    p->pos++;
                    tp_skip_spaces(p);
                    if (tp_is_eol(p)) {
                        tp_skip_nl(p);
                        val = tp_parse_object(p, field_depth + 1);
                        if (!val) return false;
                    } else {
                        val = tp_parse_primitive(p, NULL);
                        if (!val) return false;
                        if (!tp_is_eol(p)) tp_skip_to_eol(p);
                        tp_skip_nl(p);
                    }
                } else {
                    break; /* malformed, stop */
                }

                if (!ctoon_obj_setn(p->doc, item, key, kl, val)) return false;
            }
        }

        if (!ctoon_arr_append(p->doc, arr, item)) return false;
    }
    return true;
}

/* ---- array header parse ------------------------------------- */

/* Called after '[' has been consumed. Returns array or NULL on error. */
static ctoon_val *tp_parse_array_body(tp *p, int arr_depth) {
    /* read length */
    size_t ns = p->pos;
    while (!tp_eof(p) && isdigit((unsigned char)p->src[p->pos])) p->pos++;
    size_t nl = p->pos - ns;
    if (nl == 0) { tp_set_error(p, "Expected array length"); return NULL; }
    char nbuf[32];
    if (nl >= sizeof(nbuf)) { tp_set_error(p, "Array length too large"); return NULL; }
    memcpy(nbuf, p->src + ns, nl); nbuf[nl] = '\0';
    long expected = atol(nbuf);

    /* Read optional delimiter from bracket: [N|], [N\t] etc.
     * Default is comma; only update active_delim if explicitly specified. */
    if (!tp_eof(p) && p->src[p->pos] != ']') {
        char d = p->src[p->pos];
        if (d == '|' || d == '\t') p->active_delim = d;
        p->pos++;
    } else {
        /* No delimiter marker → reset to comma for this array */
        p->active_delim = ',';
    }

    if (tp_peek(p) != ']') { tp_set_error(p, "Expected ']'"); return NULL; }
    p->pos++;

    /* optional {fields} */
    const char **fields = NULL;
    int nfields = 0;
    if (tp_peek(p) == '{') {
        p->pos++;
        char *field_buf[32];
        while (tp_peek(p) != '}' && !tp_is_eol(p)) {
            tp_skip_spaces(p);
            size_t fs = p->pos;
            /* field names delimited by the same delimiter as values */
            while (!tp_eof(p) && p->src[p->pos] != p->active_delim && p->src[p->pos] != '}') p->pos++;
            size_t fl = p->pos - fs;
            while (fl > 0 && p->src[fs + fl - 1] == ' ') fl--;
            char *fname = arena_strdup(p->doc, p->src + fs, fl);
            if (!fname) return NULL;
            if (nfields < 32) field_buf[nfields++] = fname;
            if (tp_peek(p) == p->active_delim) p->pos++;
        }
        if (tp_peek(p) == '}') p->pos++;
        fields = (const char **)arena_alloc(&p->doc->arena, sizeof(const char *) * (size_t)nfields);
        if (!fields) return NULL;
        for (int i = 0; i < nfields; i++) fields[i] = field_buf[i];
    }

    tp_skip_spaces(p);
    if (tp_peek(p) != ':') { tp_set_error(p, "Expected ':'"); return NULL; }
    p->pos++;
    tp_skip_spaces(p);

    ctoon_val *arr = node_new(p->doc, CTOON_TYPE_ARRAY);
    if (!arr) return NULL;

    if (fields && nfields > 0) {
        /* tabular – rows on next lines */
        tp_skip_to_eol(p); tp_skip_nl(p);
        if (!tp_parse_tabular_rows(p, arr_depth + 1, expected, fields, nfields, arr)) return NULL;
    } else if (!tp_is_eol(p)) {
        /* inline primitive array */
        ctoon_val *tmp = tp_parse_inline_array(p);
        if (!tmp) return NULL;
        arr->u.arr.items = tmp->u.arr.items;
        arr->u.arr.count = tmp->u.arr.count;
        arr->u.arr.cap   = tmp->u.arr.cap;
    } else {
        /* list items on next lines */
        tp_skip_nl(p);
        if (!tp_parse_list_items(p, arr_depth + 1, arr)) return NULL;
    }
    return arr;
}

/* ---- value dispatch ----------------------------------------- */


static ctoon_val *tp_parse_object(tp *p, int depth) {
    ctoon_val *obj = node_new(p->doc, CTOON_TYPE_OBJECT);
    if (!obj) return NULL;

    while (!tp_eof(p)) {
        /* skip blank lines */
        bool blank = true;
        for (size_t i = p->pos; i < p->len && p->src[i] != '\n' && p->src[i] != '\r'; i++)
            if (p->src[i] != ' ') { blank = false; break; }
        if (blank) { tp_skip_to_eol(p); tp_skip_nl(p); continue; }

        int ind = tp_measure_indent(p);
        if (ind < depth) break;
        if (ind > depth) { tp_skip_to_eol(p); tp_skip_nl(p); continue; }
        if (!tp_consume_indent(p, depth)) break;

        /* parse key */
        char *key = NULL; size_t kl = 0;
        if (tp_peek(p) == '"') {
            key = tp_parse_quoted_str(p, &kl);
            if (!key) return NULL;
        } else {
            size_t ks = p->pos;
            while (!tp_eof(p) && p->src[p->pos] != ':' && p->src[p->pos] != '[' && p->src[p->pos] != '\n') p->pos++;
            kl = p->pos - ks;
            while (kl > 0 && p->src[ks + kl - 1] == ' ') kl--;
            key = arena_strdup(p->doc, p->src + ks, kl);
            if (!key) return NULL;
        }

        tp_skip_spaces(p);
        ctoon_val *val = NULL;

        if (tp_peek(p) == '[') {
            p->pos++;
            val = tp_parse_array_body(p, depth);
            if (!val) return NULL;
        } else if (tp_peek(p) == ':') {
            p->pos++;
            tp_skip_spaces(p);
            if (tp_is_eol(p)) {
                tp_skip_nl(p);
                val = tp_parse_object(p, depth + 1);
                if (!val) return NULL;
            } else {
                val = tp_parse_primitive(p, NULL);
                if (!val) return NULL;
                tp_skip_to_eol(p); tp_skip_nl(p);
            }
        } else {
            tp_set_error(p, "Expected ':' or '[' after key"); return NULL;
        }

        if (!ctoon_obj_setn(p->doc, obj, key, kl, val)) return NULL;
    }
    return obj;
}

/* ---- public entry ------------------------------------------- */

ctoon_doc *ctoon_read(const char *dat, size_t len) {
    if (!dat) return NULL;
    ctoon_doc *doc = ctoon_doc_new(0);
    if (!doc) return NULL;

    tp p = {NULL, 0, 0, NULL, 2, ','};
    p.src = dat; p.len = len; p.doc = doc;

    /* skip leading blank lines */
    while (p.pos < len) {
        bool blank = true;
        for (size_t i = p.pos; i < len && dat[i] != '\n' && dat[i] != '\r'; i++)
            if (dat[i] != ' ') { blank = false; break; }
        if (!blank) break;
        while (p.pos < len && dat[p.pos] != '\n') p.pos++;
        if (p.pos < len) p.pos++;
    }

    if (p.pos >= len) {
        doc->root = node_new(doc, CTOON_TYPE_OBJECT);
        return doc;
    }

    const char *cur = dat + p.pos;
    size_t rem = len - p.pos;

    if (*cur == '[') {
        p.pos++;
        doc->root = tp_parse_array_body(&p, 0);
    } else {
        bool has_colon = false;
        bool iq = false;
        for (size_t i = 0; i < rem && cur[i] != '\n' && cur[i] != '\r'; i++) {
            if (cur[i] == '"') iq = !iq;
            if (!iq && cur[i] == ':') { has_colon = true; break; }
        }
        doc->root = has_colon ? tp_parse_object(&p, 0) : tp_parse_primitive(&p, NULL);
    }

    if (!doc->root) { ctoon_doc_free(doc); return NULL; }
    return doc;
}

ctoon_doc *ctoon_read_file(const char *path) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsz < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)fsz + 1u);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1u, (size_t)fsz, f);
    fclose(f);
    buf[rd] = '\0';
    ctoon_doc *doc = ctoon_read(buf, rd);
    free(buf);
    return doc;
}

/* ============================================================
 * Serialiser with delimiter / indent / length-marker options
 * ============================================================ */

/* Writer context carrying options */
typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
    /* options */
    char   delim;    /* ',' '\t' or '|'          */
    int    indent;   /* spaces per level          */
    bool   lmarker;  /* prepend '#' to lengths    */
} tw;

static bool tw_grow(tw *w, size_t n) {
    if (w->len + n <= w->cap) return true;
    size_t nc = w->cap ? w->cap * 2u : 256u;
    while (nc < w->len + n) nc *= 2u;
    char *nb = (char *)realloc(w->buf, nc);
    if (!nb) return false;
    w->buf = nb; w->cap = nc; return true;
}
static bool tw_put(tw *w, const char *s, size_t n) {
    if (!tw_grow(w, n)) return false;
    memcpy(w->buf + w->len, s, n); w->len += n; return true;
}
static bool tw_putc(tw *w, char c) { return tw_put(w, &c, 1); }
static bool tw_puts(tw *w, const char *s) { return tw_put(w, s, strlen(s)); }
static bool tw_indent(tw *w, int d) {
    for (int i = 0; i < d * w->indent; i++) if (!tw_putc(w, ' ')) return false;
    return true;
}

static bool tw_write_str(tw *w, const char *s, size_t len, bool is_key) {
    bool q = (len == 0);
    if (!q && len > 0 && (s[0] == ' ' || s[len-1] == ' ')) q = true;
    if (!q && len >= 2 && s[0] == '-' && s[1] == ' ') q = true;
    for (size_t i = 0; i < len && !q; i++) {
        char c = s[i];
        if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t') q = true;
        if (is_key  && (c == ':' || c == '[' || c == '{')) q = true;
        if (!is_key && (c == w->delim || c == ':' || c == '[' || c == '{' || c == '}')) q = true;
    }
    if (!is_key && !q) {
        if ((len==4&&!memcmp(s,"null",4))||(len==4&&!memcmp(s,"true",4))||
            (len==5&&!memcmp(s,"false",5))) q = true;
        if (!q && len > 0 && len < 64) {
            char tmp[64]; memcpy(tmp,s,len); tmp[len]='\0';
            char *e = NULL; strtod(tmp, &e);
            if (e == tmp+len) q = true;
        }
    }
    if (q) {
        if (!tw_putc(w, '"')) return false;
        for (size_t i = 0; i < len; i++) {
            char c = s[i];
            if      (c=='"')  { if (!tw_puts(w,"\\\"")) return false; }
            else if (c=='\\') { if (!tw_puts(w,"\\\\")) return false; }
            else if (c=='\n') { if (!tw_puts(w,"\\n"))  return false; }
            else if (c=='\r') { if (!tw_puts(w,"\\r"))  return false; }
            else if (c=='\t') { if (!tw_puts(w,"\\t"))  return false; }
            else { if (!tw_putc(w, c)) return false; }
        }
        return tw_putc(w, '"');
    }
    return tw_put(w, s, len);
}

static bool tw_write_node(tw *w, ctoon_val *n, int depth);


/* Format array bracket: [N], [#N], [N|], [#N	] etc.
 * Per spec: delimiter marker inside bracket only when delimiter != comma. */
static void tw_fmt_bracket(tw *w, size_t count, bool close, char *out, size_t outsz) {
    char dmark[3] = {'\0', '\0', '\0'};
    if (w->delim != ',') { dmark[0] = w->delim; }  /* e.g. '|' or '\t' */
    if (close) {
        snprintf(out, outsz, w->lmarker ? "[#%zu%s]: " : "[%zu%s]: ", count, dmark);
    } else {
        snprintf(out, outsz, w->lmarker ? "[#%zu%s]{" : "[%zu%s]{", count, dmark);
    }
}
static bool tw_write_num(tw *w, ctoon_val *n) {
    char buf[64];
    ctoon_subtype st = (ctoon_subtype)n->u.number.subtype;
    if      (st == CTOON_SUBTYPE_UINT) snprintf(buf,sizeof(buf),"%" PRIu64, n->u.number.u64);
    else if (st == CTOON_SUBTYPE_SINT) snprintf(buf,sizeof(buf),"%" PRId64, n->u.number.s64);
    else {
        double d = n->u.number.f64;
        if (!isinf(d) && !isnan(d) && d == (double)(long long)d)
            snprintf(buf,sizeof(buf),"%lld",(long long)d);
        else
            snprintf(buf,sizeof(buf),"%.15g",d);
    }
    return tw_puts(w, buf);
}

/* Check whether array qualifies for tabular encoding */
static bool tw_is_tabular(ctoon_val *arr) {
    if (arr->u.arr.count == 0) return false;
    ctoon_val *first = arr->u.arr.items[0];
    if (first->type != CTOON_TYPE_OBJECT || first->u.obj.count == 0) return false;
    for (size_t r = 0; r < arr->u.arr.count; r++) {
        ctoon_val *row = arr->u.arr.items[r];
        if (row->type != CTOON_TYPE_OBJECT) return false;
        if (row->u.obj.count != first->u.obj.count) return false;
        for (size_t c = 0; c < row->u.obj.count; c++) {
            ctoon_val *k0 = first->u.obj.pairs[c].key;
            ctoon_val *kc = row->u.obj.pairs[c].key;
            if (k0->u.str.len != kc->u.str.len ||
                memcmp(k0->u.str.ptr, kc->u.str.ptr, k0->u.str.len)) return false;
            ctoon_type vt = row->u.obj.pairs[c].value->type;
            if (vt == CTOON_TYPE_ARRAY || vt == CTOON_TYPE_OBJECT) return false;
        }
    }
    return true;
}

static bool tw_write_arr_content(tw *w, ctoon_val *arr, int depth);
static bool tw_write_obj(tw *w, ctoon_val *n, int depth);

/* Object item following a "- " marker */
static bool tw_write_obj_as_list_item(tw *w, ctoon_val *obj, int depth) {
    for (size_t i = 0; i < obj->u.obj.count; i++) {
        ctoon_val *kn = obj->u.obj.pairs[i].key;
        ctoon_val *vn = obj->u.obj.pairs[i].value;
        if (i > 0) {
            if (!tw_putc(w, '\n')) return false;
            if (!tw_indent(w, depth + 2)) return false;
        }
        if (!tw_write_str(w, kn->u.str.ptr, kn->u.str.len, true)) return false;
        if (vn->type == CTOON_TYPE_OBJECT) {
            if (!tw_putc(w, ':')) return false;
            if (!tw_putc(w, '\n')) return false;
            if (!tw_write_obj(w, vn, depth + 3)) return false;
        } else if (vn->type == CTOON_TYPE_ARRAY) {
            char hdr[48];
            snprintf(hdr, sizeof(hdr), w->lmarker ? "[#%zu]: " : "[%zu]: ", vn->u.arr.count);
            if (!tw_puts(w, hdr)) return false;
            if (!tw_write_arr_content(w, vn, depth + 2)) return false;
        } else {
            if (!tw_puts(w, ": ")) return false;
            if (!tw_write_node(w, vn, depth + 2)) return false;
        }
    }
    return true;
}

static bool tw_write_arr_content(tw *w, ctoon_val *arr, int depth) {
    if (arr->u.arr.count == 0) return true;

    /* Primitive inline */
    bool all_prim = true;
    for (size_t i = 0; i < arr->u.arr.count && all_prim; i++) {
        ctoon_type t = arr->u.arr.items[i]->type;
        if (t == CTOON_TYPE_ARRAY || t == CTOON_TYPE_OBJECT) all_prim = false;
    }
    if (all_prim) {
        for (size_t i = 0; i < arr->u.arr.count; i++) {
            if (i > 0 && !tw_putc(w, w->delim)) return false;
            if (!tw_write_node(w, arr->u.arr.items[i], 0)) return false;
        }
        return true;
    }

    /* Tabular */
    if (tw_is_tabular(arr)) {
        ctoon_val *first = arr->u.arr.items[0];
        size_t ncols = first->u.obj.count;
        if (!tw_putc(w, '\n')) return false;
        for (size_t r = 0; r < arr->u.arr.count; r++) {
            if (!tw_indent(w, depth + 1)) return false;
            for (size_t c = 0; c < ncols; c++) {
                if (c > 0 && !tw_putc(w, w->delim)) return false;
                if (!tw_write_node(w, arr->u.arr.items[r]->u.obj.pairs[c].value, 0)) return false;
            }
            if (r + 1 < arr->u.arr.count && !tw_putc(w, '\n')) return false;
        }
        return true;
    }

    /* List items */
    if (!tw_putc(w, '\n')) return false;
    for (size_t i = 0; i < arr->u.arr.count; i++) {
        if (!tw_indent(w, depth + 1)) return false;
        if (!tw_puts(w, "- ")) return false;
        ctoon_val *item = arr->u.arr.items[i];
        if (item->type == CTOON_TYPE_OBJECT)
            { if (!tw_write_obj_as_list_item(w, item, depth)) return false; }
        else
            { if (!tw_write_node(w, item, depth + 1)) return false; }
        if (i + 1 < arr->u.arr.count && !tw_putc(w, '\n')) return false;
    }
    return true;
}

static bool tw_write_obj(tw *w, ctoon_val *n, int depth) {
    for (size_t i = 0; i < n->u.obj.count; i++) {
        if (i > 0 && !tw_putc(w, '\n')) return false;
        if (!tw_indent(w, depth)) return false;
        ctoon_val *kn = n->u.obj.pairs[i].key;
        ctoon_val *vn = n->u.obj.pairs[i].value;
        if (!tw_write_str(w, kn->u.str.ptr, kn->u.str.len, true)) return false;

        if (vn->type == CTOON_TYPE_ARRAY) {
            /* Write array header: key[N]{f1,f2}: or key[N]: */
            if (tw_is_tabular(vn) && vn->u.arr.count > 0) {
                ctoon_val *first = vn->u.arr.items[0];
                size_t nc = first->u.obj.count;
                char hdr[64]; tw_fmt_bracket(w, vn->u.arr.count, false, hdr, sizeof(hdr));
                if (!tw_puts(w, hdr)) return false;
                for (size_t c = 0; c < nc; c++) {
                    if (c > 0 && !tw_putc(w, w->delim)) return false;
                    ctoon_val *fk = first->u.obj.pairs[c].key;
                    if (!tw_write_str(w, fk->u.str.ptr, fk->u.str.len, true)) return false;
                }
                if (!tw_puts(w, "}:")) return false;
            } else {
                char hdr[64]; tw_fmt_bracket(w, vn->u.arr.count, true, hdr, sizeof(hdr));
                if (!tw_puts(w, hdr)) return false;
            }
            if (!tw_write_arr_content(w, vn, depth)) return false;
        } else if (vn->type == CTOON_TYPE_OBJECT) {
            if (!tw_putc(w, ':')) return false;
            if (!tw_putc(w, '\n')) return false;
            if (!tw_write_obj(w, vn, depth + 1)) return false;
        } else {
            if (!tw_puts(w, ": ")) return false;
            if (!tw_write_node(w, vn, depth)) return false;
        }
    }
    return true;
}

static bool tw_write_node(tw *w, ctoon_val *n, int depth) {
    if (!n) return tw_puts(w, "null");
    switch (n->type) {
        case CTOON_TYPE_NULL:   return tw_puts(w, "null");
        case CTOON_TYPE_TRUE:   return tw_puts(w, "true");
        case CTOON_TYPE_FALSE:  return tw_puts(w, "false");
        case CTOON_TYPE_NUMBER: return tw_write_num(w, n);
        case CTOON_TYPE_STRING: return tw_write_str(w, n->u.str.ptr, n->u.str.len, false);
        case CTOON_TYPE_ARRAY: {
            char hdr[64]; tw_fmt_bracket(w, n->u.arr.count, true, hdr, sizeof(hdr));
            if (!tw_puts(w, hdr)) return false;
            return tw_write_arr_content(w, n, depth);
        }
        case CTOON_TYPE_OBJECT: return tw_write_obj(w, n, depth);
        default: return tw_puts(w, "null");
    }
}

static char delim_char(ctoon_delimiter d) {
    switch (d) {
        case CTOON_DELIMITER_TAB:  return '\t';
        case CTOON_DELIMITER_PIPE: return '|';
        default:                   return ',';
    }
}

/* ============================================================
 * New canonical write functions
 * ============================================================ */

char *ctoon_write(ctoon_doc *doc, size_t *len) {
    ctoon_write_options opts = { CTOON_WRITE_NOFLAG, CTOON_DELIMITER_COMMA, 2 };
    return ctoon_write_opts(doc, &opts, len, NULL);
}

char *ctoon_write_opts(ctoon_doc *doc, const ctoon_write_options *opts,
                        size_t *len, ctoon_err *err) {
    if (!doc || !doc->root) {
        if (len) *len = 0;
        if (err) { err->msg = "empty document"; err->pos = 0; }
        return NULL;
    }
    tw w = {0};
    w.delim   = opts ? delim_char(opts->delimiter) : ',';
    w.indent  = (opts && opts->indent > 0) ? opts->indent : 2;
    w.lmarker = opts ? !!(opts->flag & CTOON_WRITE_LENGTH_MARKER) : false;

    if (!tw_write_node(&w, doc->root, 0)) {
        free(w.buf);
        if (len) *len = 0;
        if (err) { err->msg = "serialise failed"; err->pos = 0; }
        return NULL;
    }
    if (!tw_grow(&w, 1)) { free(w.buf); if (len) *len = 0; return NULL; }
    w.buf[w.len] = '\0';
    if (len) *len = w.len;
    if (err) { err->msg = NULL; err->pos = 0; }
    return w.buf;
}

bool ctoon_write_file(ctoon_doc *doc, const char *path) {
    size_t len = 0;
    char *raw = ctoon_write(doc, &len);
    if (!raw) return false;
    FILE *f = fopen(path, "wb");
    if (!f) { free(raw); return false; }
    size_t wr = fwrite(raw, 1, len, f);
    fclose(f); free(raw);
    return wr == len;
}

bool ctoon_write_file_opts(ctoon_doc *doc, const char *path,
                             const ctoon_write_options *opts, ctoon_err *err) {
    size_t len = 0;
    char *raw = ctoon_write_opts(doc, opts, &len, err);
    if (!raw) return false;
    FILE *f = fopen(path, "wb");
    if (!f) { free(raw); if (err) { err->msg = "cannot open file"; err->pos = 0; } return false; }
    size_t wr = fwrite(raw, 1, len, f);
    fclose(f); free(raw);
    if (wr != len) { if (err) { err->msg = "write failed"; err->pos = 0; } return false; }
    if (err) { err->msg = NULL; err->pos = 0; }
    return true;
}

/* ============================================================
 * High-level decode / encode
 * ============================================================ */

ctoon_doc *ctoon_decode(const char *dat, size_t len) {
    return ctoon_read(dat, len);
}
ctoon_doc *ctoon_decode_flg(const char *dat, size_t len, ctoon_read_flag flg) {
    return ctoon_read_flg(dat, len, flg);
}
ctoon_doc *ctoon_decode_opts(const char *dat, size_t len,
                               const ctoon_read_options *opts, ctoon_err *err) {
    return ctoon_read_opts(dat, len, opts, err);
}
ctoon_doc *ctoon_decode_file(const char *path) {
    return ctoon_read_file(path);
}
ctoon_doc *ctoon_decode_file_opts(const char *path,
                                   const ctoon_read_options *opts, ctoon_err *err) {
    return ctoon_read_file_opts(path, opts, err);
}

char *ctoon_encode(ctoon_doc *doc, size_t *len) {
    return ctoon_write(doc, len);
}
char *ctoon_encode_flg(ctoon_doc *doc, ctoon_write_flag flg, size_t *len) {
    return ctoon_write_flg(doc, flg, len);
}
char *ctoon_encode_opts(ctoon_doc *doc, const ctoon_write_options *opts,
                         size_t *len, ctoon_err *err) {
    return ctoon_write_opts(doc, opts, len, err);
}
bool ctoon_encode_file(ctoon_doc *doc, const char *path) {
    return ctoon_write_file(doc, path);
}
bool ctoon_encode_file_opts(ctoon_doc *doc, const char *path,
                               const ctoon_write_options *opts, ctoon_err *err) {
    return ctoon_write_file_opts(doc, path, opts, err);
}

/* backwards-compat */
ctoon_doc *ctoon_read_toon(const char *d,size_t n){return ctoon_read(d,n);}
ctoon_doc *ctoon_read_toon_file(const char *p){return ctoon_read_file(p);}
char *ctoon_write_toon(ctoon_doc *doc,size_t *n){return ctoon_write(doc,n);}
bool ctoon_write_toon_file(const char *p,ctoon_doc *doc){return ctoon_write_file(doc,p);}
ctoon_val *ctoon_new_array(ctoon_doc *d){return ctoon_new_arr(d);}
ctoon_val *ctoon_new_object(ctoon_doc *d){return ctoon_new_obj(d);}
bool ctoon_array_append(ctoon_doc *d,ctoon_val *a,ctoon_val *v){return ctoon_arr_append(d,a,v);}
bool ctoon_object_set(ctoon_doc *d,ctoon_val *o,const char *k,ctoon_val *v){return ctoon_obj_set(d,o,k,v);}
bool ctoon_object_setn(ctoon_doc *d,ctoon_val *o,const char *k,size_t kl,ctoon_val *v){return ctoon_obj_setn(d,o,k,kl,v);}

/* Missing stubs */
ctoon_doc *ctoon_read_flg(const char *d,size_t n,ctoon_read_flag f){(void)f;return ctoon_read(d,n);}
ctoon_doc *ctoon_read_opts(const char *dat, size_t len,
                             const ctoon_read_options *opts, ctoon_err *err) {
    if (!dat) { if (err) { err->msg = "NULL input"; err->pos = 0; } return NULL; }

    ctoon_doc *doc = ctoon_doc_new(0);
    if (!doc) { if (err) { err->msg = "alloc failed"; err->pos = 0; } return NULL; }

    tp p = {NULL, 0, 0, NULL, 2, ','};
    p.src = dat; p.len = len; p.doc = doc;
    if (opts && opts->indent > 0) p.indent = opts->indent;

    /* skip leading blank lines */
    while (p.pos < len) {
        bool blank = true;
        for (size_t i = p.pos; i < len && dat[i] != '\n' && dat[i] != '\r'; i++)
            if (dat[i] != ' ') { blank = false; break; }
        if (!blank) break;
        while (p.pos < len && dat[p.pos] != '\n') p.pos++;
        if (p.pos < len) p.pos++;
    }

    if (p.pos >= len) {
        doc->root = node_new(doc, CTOON_TYPE_OBJECT);
        if (err) { err->msg = NULL; err->pos = 0; }
        return doc;
    }

    const char *cur = dat + p.pos;
    size_t rem = len - p.pos;

    if (*cur == '[') {
        p.pos++;
        doc->root = tp_parse_array_body(&p, 0);
    } else {
        bool has_colon = false;
        bool iq = false;
        for (size_t i = 0; i < rem && cur[i] != '\n' && cur[i] != '\r'; i++) {
            if (cur[i] == '"') iq = !iq;
            if (!iq && cur[i] == ':') { has_colon = true; break; }
        }
        doc->root = has_colon ? tp_parse_object(&p, 0) : tp_parse_primitive(&p, NULL);
    }

    if (!doc->root) {
        if (err) { err->msg = "parse failed"; err->pos = p.pos; }
        ctoon_doc_free(doc);
        return NULL;
    }
    if (err) { err->msg = NULL; err->pos = 0; }
    return doc;
}
ctoon_doc *ctoon_read_file_opts(const char *p,const ctoon_read_options *o,ctoon_err *e){
    ctoon_doc *doc=ctoon_read_file(p);
    if(!doc&&e){e->msg="read failed";e->pos=0;}
    else if(e){e->msg=NULL;e->pos=0;}
    (void)o; return doc;
}

ctoon_val *ctoon_obj_get_key_at(ctoon_val *obj, size_t idx) {
    if (!ctoon_is_obj(obj) || idx >= obj->u.obj.count) return NULL;
    return obj->u.obj.pairs[idx].key;
}
ctoon_val *ctoon_obj_get_val_at(ctoon_val *obj, size_t idx) {
    if (!ctoon_is_obj(obj) || idx >= obj->u.obj.count) return NULL;
    return obj->u.obj.pairs[idx].value;
}
