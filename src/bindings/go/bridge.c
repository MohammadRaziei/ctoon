#include "bridge.h"
#include "ctoon.h"

#include <stdlib.h>
#include <string.h>

/* Tags MUST match binding.go's tape* constants exactly. */
enum {
    TAPE_NULL = 0, TAPE_FALSE = 1, TAPE_TRUE = 2,
    TAPE_SINT = 3, TAPE_UINT = 4, TAPE_REAL = 5,
    TAPE_STR  = 6, TAPE_ARR  = 7, TAPE_OBJ  = 8
};

static char *bridge_dup_cstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = (char *)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

/* ─────────────────────────────────────────────────────────────────────
 * Growable output buffer, used for building the tape on the decode side.
 * ───────────────────────────────────────────────────────────────────── */

typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} gbuf;

static int gbuf_reserve(gbuf *b, size_t extra) {
    if (b->len + extra <= b->cap) return 1;
    size_t newcap = b->cap ? b->cap * 2 : 256;
    while (newcap < b->len + extra) newcap *= 2;
    uint8_t *p = (uint8_t *)realloc(b->data, newcap);
    if (!p) return 0;
    b->data = p;
    b->cap = newcap;
    return 1;
}

static int gbuf_u8(gbuf *b, uint8_t v) {
    if (!gbuf_reserve(b, 1)) return 0;
    b->data[b->len++] = v;
    return 1;
}

static int gbuf_u32(gbuf *b, uint32_t v) {
    if (!gbuf_reserve(b, 4)) return 0;
    b->data[b->len++] = (uint8_t)(v);
    b->data[b->len++] = (uint8_t)(v >> 8);
    b->data[b->len++] = (uint8_t)(v >> 16);
    b->data[b->len++] = (uint8_t)(v >> 24);
    return 1;
}

static int gbuf_u64(gbuf *b, uint64_t v) {
    if (!gbuf_reserve(b, 8)) return 0;
    for (int i = 0; i < 8; i++) b->data[b->len++] = (uint8_t)(v >> (8 * i));
    return 1;
}

static int gbuf_bytes(gbuf *b, const void *p, size_t n) {
    if (n == 0) return 1;
    if (!gbuf_reserve(b, n)) return 0;
    memcpy(b->data + b->len, p, n);
    b->len += n;
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────
 * Decode direction: ctoon_val tree -> tape (single native C pass).
 * ───────────────────────────────────────────────────────────────────── */

static int val_to_tape(gbuf *out, ctoon_val *val) {
    if (!val) return gbuf_u8(out, TAPE_NULL);

    switch (ctoon_get_type(val)) {
    case CTOON_TYPE_NULL:
        return gbuf_u8(out, TAPE_NULL);

    case CTOON_TYPE_BOOL:
        return gbuf_u8(out, ctoon_get_bool(val) ? TAPE_TRUE : TAPE_FALSE);

    case CTOON_TYPE_NUM:
        if (ctoon_is_uint(val)) {
            return gbuf_u8(out, TAPE_UINT) && gbuf_u64(out, (uint64_t)ctoon_get_uint(val));
        } else if (ctoon_is_sint(val)) {
            int64_t sv = ctoon_get_sint(val);
            uint64_t bits; memcpy(&bits, &sv, 8);
            return gbuf_u8(out, TAPE_SINT) && gbuf_u64(out, bits);
        } else {
            double dv = ctoon_get_real(val);
            uint64_t bits; memcpy(&bits, &dv, 8);
            return gbuf_u8(out, TAPE_REAL) && gbuf_u64(out, bits);
        }

    case CTOON_TYPE_STR: {
        const char *s = ctoon_get_str(val);
        size_t n = ctoon_get_len(val);
        return gbuf_u8(out, TAPE_STR) && gbuf_u32(out, (uint32_t)n) && gbuf_bytes(out, s, n);
    }

    case CTOON_TYPE_ARR: {
        size_t n = ctoon_arr_size(val);
        if (!gbuf_u8(out, TAPE_ARR) || !gbuf_u32(out, (uint32_t)n)) return 0;
        for (size_t i = 0; i < n; i++) {
            if (!val_to_tape(out, ctoon_arr_get(val, i))) return 0;
        }
        return 1;
    }

    case CTOON_TYPE_OBJ: {
        size_t n = ctoon_obj_size(val);
        if (!gbuf_u8(out, TAPE_OBJ) || !gbuf_u32(out, (uint32_t)n)) return 0;
        ctoon_obj_iter iter;
        ctoon_obj_iter_init(val, &iter);
        while (ctoon_obj_iter_has_next(&iter)) {
            ctoon_val *key_val = ctoon_obj_iter_next(&iter);
            if (!key_val) break;
            ctoon_val *v = ctoon_obj_iter_get_val(key_val);
            const char *ks = ctoon_get_str(key_val);
            size_t klen = ctoon_get_len(key_val);
            if (!gbuf_u32(out, (uint32_t)klen) || !gbuf_bytes(out, ks, klen)) return 0;
            if (!val_to_tape(out, v)) return 0;
        }
        return 1;
    }

    default: {
        /* raw/unknown value kind — best-effort string, matches the old
           per-node valToGo()'s fallback behaviour. */
        const char *raw = ctoon_get_raw(val);
        size_t n = raw ? strlen(raw) : 0;
        return gbuf_u8(out, TAPE_STR) && gbuf_u32(out, (uint32_t)n) && gbuf_bytes(out, raw, n);
    }
    }
}

int ctoon_go_bridge_decode(
    const char *data, size_t len,
    int as_json, uint32_t read_flag,
    uint8_t **out_tape, size_t *out_tape_len,
    char **out_err
) {
    *out_tape = NULL;
    *out_tape_len = 0;
    *out_err = NULL;

    ctoon_doc *doc = NULL;

    if (as_json) {
#if defined(CTOON_ENABLE_JSON) && CTOON_ENABLE_JSON
        ctoon_read_err rerr; memset(&rerr, 0, sizeof(rerr));
        doc = ctoon_read_json((char *)data, len, 0, NULL, &rerr);
        if (!doc) {
            *out_err = bridge_dup_cstr(rerr.msg ? rerr.msg : "ctoon: JSON parse error");
            return 0;
        }
#else
        *out_err = bridge_dup_cstr("ctoon: JSON support not enabled");
        return 0;
#endif
    } else {
        ctoon_read_err rerr; memset(&rerr, 0, sizeof(rerr));
        doc = ctoon_read_opts((char *)data, len, (ctoon_read_flag)read_flag, NULL, &rerr);
        if (!doc) {
            *out_err = bridge_dup_cstr(rerr.msg ? rerr.msg : "ctoon: parse error");
            return 0;
        }
    }

    gbuf buf; memset(&buf, 0, sizeof(buf));
    int ok = val_to_tape(&buf, ctoon_doc_get_root(doc));
    ctoon_doc_free(doc);

    if (!ok) {
        free(buf.data);
        *out_err = bridge_dup_cstr("ctoon: out of memory building tape");
        return 0;
    }

    *out_tape = buf.data;
    *out_tape_len = buf.len;
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────
 * Encode direction: tape -> ctoon_mut_val tree (single native C pass).
 * ───────────────────────────────────────────────────────────────────── */

typedef struct {
    const uint8_t *buf;
    size_t len;
    size_t pos;
} treader;

static int tr_u8(treader *r, uint8_t *out) {
    if (r->pos + 1 > r->len) return 0;
    *out = r->buf[r->pos++];
    return 1;
}

static int tr_u32(treader *r, uint32_t *out) {
    if (r->pos + 4 > r->len) return 0;
    const uint8_t *p = r->buf + r->pos;
    *out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    r->pos += 4;
    return 1;
}

static int tr_u64(treader *r, uint64_t *out) {
    if (r->pos + 8 > r->len) return 0;
    const uint8_t *p = r->buf + r->pos;
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= ((uint64_t)p[i]) << (8 * i);
    r->pos += 8;
    *out = v;
    return 1;
}

/* Returns a pointer into the tape buffer itself (zero-copy) — valid only
   for the duration of this bridge call, which is exactly how it's used
   below (ctoon_mut_strncpy copies it immediately). */
static const uint8_t *tr_bytes(treader *r, size_t n) {
    if (n == 0) return (const uint8_t *)"";
    if (r->pos + n > r->len) return NULL;
    const uint8_t *p = r->buf + r->pos;
    r->pos += n;
    return p;
}

static ctoon_mut_val *tape_to_mut(ctoon_mut_doc *doc, treader *r) {
    uint8_t tag;
    if (!tr_u8(r, &tag)) return NULL;

    switch (tag) {
    case TAPE_NULL:  return ctoon_mut_null(doc);
    case TAPE_FALSE: return ctoon_mut_false(doc);
    case TAPE_TRUE:  return ctoon_mut_true(doc);

    case TAPE_SINT: {
        uint64_t bits;
        if (!tr_u64(r, &bits)) return NULL;
        int64_t v; memcpy(&v, &bits, 8);
        return ctoon_mut_sint(doc, v);
    }
    case TAPE_UINT: {
        uint64_t v;
        if (!tr_u64(r, &v)) return NULL;
        return ctoon_mut_uint(doc, v);
    }
    case TAPE_REAL: {
        uint64_t bits;
        if (!tr_u64(r, &bits)) return NULL;
        double v; memcpy(&v, &bits, 8);
        return ctoon_mut_real(doc, v);
    }
    case TAPE_STR: {
        uint32_t n;
        if (!tr_u32(r, &n)) return NULL;
        const uint8_t *p = tr_bytes(r, n);
        if (!p) return NULL;
        return ctoon_mut_strncpy(doc, (const char *)p, n);
    }
    case TAPE_ARR: {
        uint32_t n;
        if (!tr_u32(r, &n)) return NULL;
        ctoon_mut_val *arr = ctoon_mut_arr(doc);
        if (!arr) return NULL;
        for (uint32_t i = 0; i < n; i++) {
            ctoon_mut_val *child = tape_to_mut(doc, r);
            if (!child || !ctoon_mut_arr_append(arr, child)) return NULL;
        }
        return arr;
    }
    case TAPE_OBJ: {
        uint32_t n;
        if (!tr_u32(r, &n)) return NULL;
        ctoon_mut_val *obj = ctoon_mut_obj(doc);
        if (!obj) return NULL;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t klen;
            if (!tr_u32(r, &klen)) return NULL;
            const uint8_t *kp = tr_bytes(r, klen);
            if (!kp) return NULL;
            ctoon_mut_val *key_val = ctoon_mut_strncpy(doc, (const char *)kp, klen);
            if (!key_val) return NULL;
            ctoon_mut_val *child = tape_to_mut(doc, r);
            if (!child || !ctoon_mut_obj_put(obj, key_val, child)) return NULL;
        }
        return obj;
    }
    default:
        return NULL;
    }
}

int ctoon_go_bridge_encode(
    const uint8_t *tape, size_t tape_len,
    int as_json, int indent, uint32_t write_flag, uint32_t delimiter,
    char **out_data, size_t *out_len,
    char **out_err
) {
    *out_data = NULL;
    *out_len = 0;
    *out_err = NULL;

    ctoon_mut_doc *doc = ctoon_mut_doc_new(NULL);
    if (!doc) {
        *out_err = bridge_dup_cstr("ctoon: failed to create mutable document");
        return 0;
    }

    treader r; r.buf = tape; r.len = tape_len; r.pos = 0;
    ctoon_mut_val *root = tape_to_mut(doc, &r);
    if (!root) {
        ctoon_mut_doc_free(doc);
        *out_err = bridge_dup_cstr("ctoon: malformed tape (internal error — please report)");
        return 0;
    }
    ctoon_mut_doc_set_root(doc, root);

    char *raw = NULL;
    size_t len = 0;

    if (as_json) {
#if defined(CTOON_ENABLE_JSON) && CTOON_ENABLE_JSON
        ctoon_write_err werr; memset(&werr, 0, sizeof(werr));
        raw = ctoon_write_json_mut(doc, indent, (ctoon_write_flag)write_flag, NULL, &len, &werr);
        if (!raw) {
            *out_err = bridge_dup_cstr(werr.msg ? werr.msg : "ctoon: JSON write error");
            ctoon_mut_doc_free(doc);
            return 0;
        }
#else
        ctoon_mut_doc_free(doc);
        *out_err = bridge_dup_cstr("ctoon: JSON support not enabled");
        return 0;
#endif
    } else {
        ctoon_write_options wopts; memset(&wopts, 0, sizeof(wopts));
        wopts.flag = (ctoon_write_flag)write_flag;
        wopts.delimiter = (ctoon_delimiter)delimiter;
        ctoon_write_err werr; memset(&werr, 0, sizeof(werr));
        raw = ctoon_mut_write_opts(doc, &wopts, NULL, &len, &werr);
        if (!raw) {
            *out_err = bridge_dup_cstr(werr.msg ? werr.msg : "ctoon: write error");
            ctoon_mut_doc_free(doc);
            return 0;
        }
    }

    ctoon_mut_doc_free(doc);
    *out_data = raw;
    *out_len = len;
    return 1;
}

void ctoon_go_bridge_free(void *ptr) {
    free(ptr);
}
