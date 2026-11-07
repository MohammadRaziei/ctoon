/*
 * CToon C benchmark.
 *
 * Every implementation here - ctoon included - is treated as one more
 * peer entry, fetched from its own GitHub repo, tested the same way. No
 * "ctoon vs X" framing: a shared result table, one row per
 * (library, operation) pair.
 *
 * Methodology (same across every language benchmark in this repo):
 *   1. Load every file in the corpus manifest into memory (untimed).
 *   2. Untimed pre-pass: convert each JSON file to TOON once (with ctoon,
 *      since it's the only library here with a JSON parser), to have TOON
 *      input ready for the decode benchmark.
 *   3. Timed "json_to_toon": repeatedly parse JSON and re-serialise to TOON.
 *   4. Timed "toon_to_json": repeatedly parse TOON and re-serialise to JSON.
 *      Measures interop with ctoon's specific TOON output.
 *   5. Timed "roundtrip": json -> toon -> parse toon -> json, chained as
 *      one operation, using ctoon's own encoder/decoder together.
 *      Measures self-consistency, not interop.
 *   6. Timed "toon_to_struct" / "struct_to_toon": the same directions with
 *      the text format taken out -- parse the pre-pass TOON into ctoon's
 *      in-memory structure (ctoon_doc) and stop; serialise a ctoon_doc
 *      built untimed in the pre-pass. TOONc has no equivalent, so no rows.
 *   7. Report throughput - MB/s of bytes actually processed by
 *      *successful* conversions only - and documents/sec, plus success
 *      rate, so a library that fails part of the corpus doesn't get an
 *      inflated number.
 *
 * TOONc (github.com/UsboKirishima/TOONc) has no JSON parser and no TOON
 * writer - only toon_to_json is meaningful for it. Its parser also isn't
 * robust against everything ctoon emits (it crashes on part of the
 * corpus), so each file is first probed in a forked child before being
 * included in the timed run - see bench_toonc().
 *
 * If CTOON_BENCH_LOG_FILE is defined (non-empty), one line per failure is
 * written there - only for the first repeat of each timed phase (and the
 * untimed pre-pass), not all 20. Real error messages are captured where
 * ctoon's API provides one; TOONc's API doesn't return one at all, so its
 * log lines just note that parsing failed.
 *
 * If the CTOON_BENCH_ONLY environment variable is set to a library name
 * ("ctoon" or "TOONc"), only that library's benchmark runs (and the results
 * JSON is NOT written) -- used by the memory harness to get one library's
 * peak RSS in a fresh process, uncontaminated by the other library ever
 * having run in the same address space. Normal timed runs leave this unset.
 */

#define _POSIX_C_SOURCE 200809L /* for open_memstream, used for TOONc */

#define CTOON_ENABLE_JSON 1
#include "ctoon.h"
#include "toonc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef CTOON_BENCH_MANIFEST
#error "CTOON_BENCH_MANIFEST must be defined at compile time"
#endif
#ifndef CTOON_BENCH_RESULTS_JSON
#error "CTOON_BENCH_RESULTS_JSON must be defined at compile time"
#endif
#ifndef CTOON_BENCH_REPEATS
#define CTOON_BENCH_REPEATS 20
#endif

typedef struct {
    char   *path;
    char   *data;
    size_t  len;
    char   *toon;
    size_t  toon_len;
    ctoon_doc *doc;   /* ctoon's own in-memory structure for this file, built
                         UNTIMED in the pre-pass; the input of struct_to_toon */
} bench_file;

typedef struct {
    char   library[32];
    char   operation[16];
    double throughput_mb_s;
    double docs_per_sec;
    double success_rate;
    double total_time_s;
} bench_result;

static bench_result g_results[16];
static int g_result_count = 0;
static FILE *g_log = NULL;

/* Scaling: same (library, operation) pairs, but measured on NBUCKETS
   subsets of the corpus split by file size, so the report can plot
   throughput vs. input size as a line, not just one aggregate bar. */
#define SCALING_NBUCKETS 5
#define SCALING_REPS 5

typedef struct {
    char   library[32];
    char   operation[16];
    double size_bytes;      /* representative (median) size of this bucket */
    double throughput_mb_s;
    double docs_per_sec;
    double success_rate;
} scaling_result;

static scaling_result g_scaling[5 /*ops*/ * 2 /*libs*/ * SCALING_NBUCKETS];
static int g_scaling_count = 0;

static void record_scaling(const char *library, const char *operation,
                            double size_bytes, double bytes, long ops,
                            double seconds, long attempted) {
    scaling_result *r = &g_scaling[g_scaling_count++];
    snprintf(r->library, sizeof(r->library), "%s", library);
    snprintf(r->operation, sizeof(r->operation), "%s", operation);
    r->size_bytes = size_bytes;
    r->throughput_mb_s = ops ? bytes / seconds / 1e6 : 0.0;
    r->docs_per_sec = (double)ops / seconds;
    r->success_rate = attempted ? (double)ops / (double)attempted : 0.0;
}

static int cmp_bench_file_ptr_by_len(const void *a, const void *b) {
    const bench_file *fa = *(const bench_file * const *)a;
    const bench_file *fb = *(const bench_file * const *)b;
    if (fa->len < fb->len) return -1;
    if (fa->len > fb->len) return 1;
    return 0;
}

/* ------------------------------------------------------------ order check --
 * Key order, checked one direction at a time against the shared samples in
 * benchmarks/order_check/ (sample.json / sample.toon: deliberately
 * non-alphabetical keys, a nested object and a tabular array):
 *   json_to_toon -- keys of the TOON output must follow the order of sample.json
 *   toon_to_json -- keys of the JSON output must follow the order of sample.toon
 * Both reduce to "first-occurrence order of the keys" (a key repeated in every
 * row of an array counts once), so the tabular header form `list[2]{z,a}:`
 * and the expanded `- z: 1` form compare equal. Hand-written scanners, no
 * regex dependency. */
#define ORDER_CHECK_MAX_KEYS 32
#define ORDER_KEY_LEN 32
typedef char order_keys[ORDER_CHECK_MAX_KEYS][ORDER_KEY_LEN];

static int order_add_key(order_keys keys, int n, const char *s, size_t len) {
    while (len && (*s == ' ' || *s == '\t')) { s++; len--; }
    while (len && (s[len - 1] == ' ' || s[len - 1] == '\t')) len--;
    if (len >= 2 && s[0] == '"' && s[len - 1] == '"') { s++; len -= 2; }
    if (!len) return n;
    if (len >= ORDER_KEY_LEN) len = ORDER_KEY_LEN - 1;
    for (int i = 0; i < n; i++)
        if (strlen(keys[i]) == len && memcmp(keys[i], s, len) == 0) return n;
    if (n >= ORDER_CHECK_MAX_KEYS) return n;
    memcpy(keys[n], s, len);
    keys[n][len] = '\0';
    return n + 1;
}

/* Every `"key":` in a JSON text, first occurrence only. */
static int order_json_keys(const char *text, order_keys keys) {
    int n = 0;
    for (const char *p = text; *p; ) {
        if (*p != '"') { p++; continue; }
        const char *start = p + 1, *q = start;
        while (*q && *q != '"') q += (*q == '\\' && q[1]) ? 2 : 1;
        if (!*q) break;
        const char *r = q + 1;
        while (*r == ' ' || *r == '\t' || *r == '\n' || *r == '\r') r++;
        if (*r == ':') n = order_add_key(keys, n, start, (size_t)(q - start));
        p = q + 1;
    }
    return n;
}

/* Line by line: strip indent and `- ` list markers, then `key:`,
   `key[N]:` or `key[N]{f1,f2}:` -- the key and, for the tabular form, the
   fields in the braces. Row lines (`1,2`) have no `:` after the token. */
static int order_toon_keys(const char *text, order_keys keys) {
    int n = 0;
    const char *p = text;
    while (*p) {
        const char *eol = strchr(p, '\n');
        const char *end = eol ? eol : p + strlen(p);
        const char *s = p;
        while (s < end && *s == ' ') s++;
        while (s + 1 < end && s[0] == '-' && s[1] == ' ') { s += 2; while (s < end && *s == ' ') s++; }
        const char *k = s, *q = s;
        if (q < end && *q == '"') {
            q++;
            while (q < end && *q != '"') q += (*q == '\\' && q + 1 < end) ? 2 : 1;
            if (q < end) q++;
        } else {
            while (q < end && !strchr(" \t:[{,\"", *q)) q++;
        }
        const char *key_end = q, *fields = NULL, *fields_end = NULL;
        if (q < end && *q == '[') { while (q < end && *q != ']') q++; if (q < end) q++; }
        if (q < end && *q == '{') {
            fields = q + 1;
            while (q < end && *q != '}') q++;
            fields_end = q;
            if (q < end) q++;
        }
        if (q < end && *q == ':' && key_end > k) {
            n = order_add_key(keys, n, k, (size_t)(key_end - k));
            if (fields) {
                const char *f = fields;
                while (f <= fields_end) {
                    const char *g = f;
                    while (g < fields_end && *g != ',' && *g != '|' && *g != '\t') g++;
                    n = order_add_key(keys, n, f, (size_t)(g - f));
                    f = g + 1;
                }
            }
        }
        p = eol ? eol + 1 : end;
    }
    return n;
}

typedef struct {
    char library[32];
    char operation[16];
    bool preserved;
} order_check_result;
static order_check_result g_order_checks[16];
static int g_order_check_count = 0;

static void record_order_check(const char *library, const char *operation, bool preserved) {
    order_check_result *r = &g_order_checks[g_order_check_count++];
    snprintf(r->library, sizeof(r->library), "%s", library);
    snprintf(r->operation, sizeof(r->operation), "%s", operation);
    r->preserved = preserved;
}

/* Compare the key order found in `got` against `expected`. */
static void record_order_keys(const char *library, const char *operation,
                              order_keys expected, int ne, order_keys got, int ng) {
    bool same = (ne == ng);
    for (int i = 0; same && i < ne; i++) same = strcmp(expected[i], got[i]) == 0;
    record_order_check(library, operation, same);
}

static char *order_read_sample(const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", CTOON_BENCH_ORDER_DIR, name);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    char *buf = (char *)malloc((size_t)n + 1);
    if (buf) { size_t got = fread(buf, 1, (size_t)n, f); buf[got] = '\0'; }
    fclose(f);
    return buf;
}

static void log_fail(const char *library, const char *operation,
                      const char *path, const char *err_msg) {
    if (!g_log) return;
    fprintf(g_log, "[%s] [%s] FILE: %s ERROR: %s\n",
            library, operation, path, err_msg ? err_msg : "(no error detail available)");
}

static void record(const char *library, const char *operation,
                    double bytes, long ops, double seconds, long attempted) {
    bench_result *r = &g_results[g_result_count++];
    snprintf(r->library, sizeof(r->library), "%s", library);
    snprintf(r->operation, sizeof(r->operation), "%s", operation);
    r->throughput_mb_s = ops ? bytes / seconds / 1e6 : 0.0;
    r->docs_per_sec = (double)ops / seconds;
    r->success_rate = attempted ? (double)ops / (double)attempted : 0.0;
    r->total_time_s = seconds;

    printf("%-8s %-14s %9.2f MB/s %14.0f %8.0f%%  %8.4f s  (x%d reps)\n",
           library, operation, r->throughput_mb_s, r->docs_per_sec,
           r->success_rate * 100.0, seconds, CTOON_BENCH_REPEATS);
}

static char *read_whole_file(const char *path, size_t *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size < 0) { fclose(fp); return NULL; }
    fseek(fp, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t rd = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    buf[rd] = '\0';
    *out_len = rd;
    return buf;
}

static double now_seconds(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

static char *dup_str(const char *s) {
    size_t len = strlen(s) + 1;
    char *out = (char *)malloc(len);
    if (out) memcpy(out, s, len);
    return out;
}

/* ------------------------------------------------------------------ dump --
 * Untimed. Writes what each operation actually produced for each file to
 *   <results dir>/dump/c/<library>/<operation>/<file index>.txt
 * (nothing for a file the library failed on) so report/verify_outputs.mjs
 * can judge whether the output is CORRECT, not merely error-free -- see that
 * script's header. Disabled in the CTOON_BENCH_ONLY memory-measurement mode,
 * which must not touch the filesystem or the timing. */
static char g_dump_root[4096];   /* "" => disabled */

static void dump_init(void) {
    const char *res = CTOON_BENCH_RESULTS_JSON;
    const char *slash = strrchr(res, '/');
    size_t dirlen = slash ? (size_t)(slash - res) : 0;
    if (dirlen == 0 || dirlen > sizeof(g_dump_root) - 16) return;
    snprintf(g_dump_root, sizeof g_dump_root, "%.*s/dump/c", (int)dirlen, res);
}

static void mkdir_p(const char *path) {
    char tmp[4352];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0777); *p = '/'; }
    mkdir(tmp, 0777);
}

static void dump_path(char *out, size_t cap, const char *lib, const char *op, size_t idx) {
    snprintf(out, cap, "%s/%s/%s/%zu.txt", g_dump_root, lib, op, idx);
}

/* Call once per (library, operation) before dumping its files. */
static void dump_prepare(const char *lib, const char *op) {
    char dir[4352];
    snprintf(dir, sizeof dir, "%s/%s/%s", g_dump_root, lib, op);
    mkdir_p(dir);
}

/* Drop a stale file from a previous run, so "no file" always means "failed". */
static void dump_clear(const char *lib, const char *op, size_t idx) {
    char path[4608];
    dump_path(path, sizeof path, lib, op, idx);
    unlink(path);
}

static void dump_write(const char *lib, const char *op, size_t idx, const char *data, size_t len) {
    char path[4608];
    dump_path(path, sizeof path, lib, op, idx);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    if (len) fwrite(data, 1, len, f);
    fclose(f);
}

static void dump_ctoon(bench_file *files, size_t n) {
    if (!g_dump_root[0]) return;
    dump_prepare("ctoon", "json_to_toon");
    dump_prepare("ctoon", "toon_to_json");
    dump_prepare("ctoon", "roundtrip");
    dump_prepare("ctoon", "toon_to_struct");
    dump_prepare("ctoon", "struct_to_toon");
    for (size_t i = 0; i < n; i++) {
        dump_clear("ctoon", "toon_to_struct", i);
        dump_clear("ctoon", "struct_to_toon", i);

        /* struct_to_toon: write the pre-built structure */
        if (files[i].doc) {
            size_t len = 0;
            char *toon = ctoon_write(files[i].doc, &len);
            if (toon) { dump_write("ctoon", "struct_to_toon", i, toon, len); free(toon); }
        }

        /* toon_to_struct: the decoded structure, serialised to JSON
           (untimed) only so the verifier can compare it to the original. */
        if (files[i].toon) {
            ctoon_doc *d = ctoon_read(files[i].toon, files[i].toon_len, 0);
            if (d) {
                size_t len = 0;
                ctoon_write_err werr; memset(&werr, 0, sizeof(werr));
                char *json = ctoon_doc_to_json(d, 2, CTOON_WRITE_NOFLAG, NULL, &len, &werr);
                if (json) { dump_write("ctoon", "toon_to_struct", i, json, len); free(json); }
                ctoon_doc_free(d);
            }
        }

        dump_clear("ctoon", "json_to_toon", i);
        dump_clear("ctoon", "toon_to_json", i);
        dump_clear("ctoon", "roundtrip", i);

        /* json_to_toon */
        {
            size_t len = 0;
            char *toon = ctoon_json_to_toon(files[i].data, files[i].len, &len);
            if (toon) { dump_write("ctoon", "json_to_toon", i, toon, len); free(toon); }
        }

        /* toon_to_json */
        if (files[i].toon) {
            size_t len = 0;
            char *json = ctoon_toon_to_json(files[i].toon, files[i].toon_len, &len);
            if (json) { dump_write("ctoon", "toon_to_json", i, json, len); free(json); }
        }

        /* roundtrip: json -> toon -> json */
        {
            size_t tlen = 0;
            char *toon = ctoon_json_to_toon(files[i].data, files[i].len, &tlen);
            if (!toon) continue;
            size_t jlen = 0;
            char *json = ctoon_toon_to_json(toon, tlen, &jlen);
            free(toon);
            if (json) { dump_write("ctoon", "roundtrip", i, json, jlen); free(json); }
        }
    }
}

static void bench_ctoon(bench_file *files, size_t n) {
    dump_ctoon(files, n);
    /* json_to_toon / toon_to_json / roundtrip call ctoon's one-shot text
       conversion functions directly -- the same entry points every language
       binding wraps (ctoon::json_to_toon, ctoon.json_to_toon, ...). */
    long ops = 0; double bytes = 0;
    double t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (size_t i = 0; i < n; i++) {
            size_t len = 0;
            char *toon = ctoon_json_to_toon(files[i].data, files[i].len, &len);
            if (toon) { free(toon); ops++; bytes += (double)files[i].len; }
            else if (rep == 0) log_fail("ctoon", "json_to_toon", files[i].path, "ctoon_json_to_toon failed");
        }
    }
    record("ctoon", "json_to_toon", bytes, ops, now_seconds() - t0, (long)n * CTOON_BENCH_REPEATS);

    ops = 0; bytes = 0;
    t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (size_t i = 0; i < n; i++) {
            if (!files[i].toon) continue;
            size_t len = 0;
            char *json = ctoon_toon_to_json(files[i].toon, files[i].toon_len, &len);
            if (json) { free(json); ops++; bytes += (double)files[i].toon_len; }
            else if (rep == 0) log_fail("ctoon", "toon_to_json", files[i].path, "ctoon_toon_to_json failed");
        }
    }
    record("ctoon", "toon_to_json", bytes, ops, now_seconds() - t0, (long)n * CTOON_BENCH_REPEATS);

    /* toon_to_struct: parse the pre-pass TOON into ctoon's in-memory
       structure and free it -- no JSON text anywhere in the timed region. */
    ops = 0; bytes = 0;
    t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (size_t i = 0; i < n; i++) {
            if (!files[i].toon) continue;
            ctoon_doc *doc = ctoon_read(files[i].toon, files[i].toon_len, 0);
            if (!doc) { if (rep == 0) log_fail("ctoon", "toon_to_struct", files[i].path, "ctoon_read failed"); continue; }
            ops++; bytes += (double)files[i].toon_len;
            ctoon_doc_free(doc);
        }
    }
    record("ctoon", "toon_to_struct", bytes, ops, now_seconds() - t0, (long)n * CTOON_BENCH_REPEATS);

    /* struct_to_toon: serialise the structure built (untimed) in the
       pre-pass -- no parsing in the timed region. Weighted by the JSON
       size, same convention as json_to_toon. */
    ops = 0; bytes = 0;
    t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (size_t i = 0; i < n; i++) {
            if (!files[i].doc) continue;
            size_t len = 0;
            char *toon = ctoon_write(files[i].doc, &len);
            if (toon) { free(toon); ops++; bytes += (double)files[i].len; }
            else if (rep == 0) log_fail("ctoon", "struct_to_toon", files[i].path, "ctoon_write failed");
        }
    }
    record("ctoon", "struct_to_toon", bytes, ops, now_seconds() - t0, (long)n * CTOON_BENCH_REPEATS);

    /* roundtrip: ctoon_json_to_toon, then ctoon_toon_to_json on its output,
       chained as one operation per file. */
    ops = 0; bytes = 0;
    t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (size_t i = 0; i < n; i++) {
            size_t tlen = 0;
            char *toon = ctoon_json_to_toon(files[i].data, files[i].len, &tlen);
            if (!toon) { if (rep == 0) log_fail("ctoon", "roundtrip", files[i].path, "ctoon_json_to_toon failed"); continue; }
            size_t jlen = 0;
            char *json = ctoon_toon_to_json(toon, tlen, &jlen);
            free(toon);
            if (json) { free(json); ops++; bytes += (double)files[i].len; }
            else if (rep == 0) log_fail("ctoon", "roundtrip", files[i].path, "ctoon_toon_to_json failed");
        }
    }
    record("ctoon", "roundtrip", bytes, ops, now_seconds() - t0, (long)n * CTOON_BENCH_REPEATS);
}

static void bench_toonc(bench_file *files, size_t n, size_t pre_ok) {
    /* TOONc prints its own parser diagnostics straight to stdout/stderr;
       silence that so it doesn't spam the console or skew the timing. */
    fflush(stdout);
    int devnull = open("/dev/null", O_WRONLY);
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);
    if (devnull >= 0) {
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
    }

    /* Probe each file in a forked child first: TOONc's parser crashes
       (invalid free) on part of the corpus, and we don't want that to
       take the whole benchmark process down. */
    char *safe = (char *)calloc(n, 1);
    size_t safe_count = 0;
    for (size_t i = 0; i < n; i++) {
        if (!files[i].toon) continue;
        pid_t pid = fork();
        if (pid == 0) {
            toonObject *obj = TOONc_parseString(files[i].toon);
            if (obj) {
                char *membuf = NULL; size_t memsize = 0;
                FILE *mem = open_memstream(&membuf, &memsize);
                if (mem) { TOONc_toJSON(obj, mem, 0); fclose(mem); free(membuf); }
                TOONc_free(obj);
            }
            _exit(obj ? 0 : 1);
        } else if (pid > 0) {
            int status = 0;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                safe[i] = 1;
                safe_count++;
            } else if (g_log) {
                /* Restore stdio just long enough to write the log line;
                   TOONc's API gives no error message, only pass/fail. */
                dup2(saved_stdout, STDOUT_FILENO);
                dup2(saved_stderr, STDERR_FILENO);
                log_fail("TOONc", "toon_to_json", files[i].path,
                         WIFSIGNALED(status) ? "parser crashed (signal)" : "parser rejected input");
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
            }
        }
    }

    if (g_dump_root[0]) {
        dump_prepare("TOONc", "toon_to_json");
        for (size_t i = 0; i < n; i++) {
            dump_clear("TOONc", "toon_to_json", i);
            if (!safe[i]) continue;
            toonObject *obj = TOONc_parseString(files[i].toon);
            if (!obj) continue;
            char *membuf = NULL; size_t memsize = 0;
            FILE *mem = open_memstream(&membuf, &memsize);
            if (mem) {
                TOONc_toJSON(obj, mem, 0);
                fclose(mem);
                dump_write("TOONc", "toon_to_json", i, membuf, memsize);
                free(membuf);
            }
            TOONc_free(obj);
        }
    }

    long ops = 0; double bytes = 0;
    double t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (size_t i = 0; i < n; i++) {
            if (!safe[i]) continue;
            toonObject *obj = TOONc_parseString(files[i].toon);
            if (!obj) continue;
            char *membuf = NULL; size_t memsize = 0;
            FILE *mem = open_memstream(&membuf, &memsize);
            if (mem) {
                TOONc_toJSON(obj, mem, 0);
                fclose(mem);
                free(membuf);
                ops++;
                bytes += (double)files[i].toon_len;
            }
            TOONc_free(obj);
        }
    }
    double seconds = now_seconds() - t0;

    fflush(stdout);
    if (devnull >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        dup2(saved_stderr, STDERR_FILENO);
        close(devnull);
        close(saved_stdout);
        close(saved_stderr);
    }

    printf("(TOONc's parser safely handles %zu/%zu TOON files - the rest crash "
           "its parser on this corpus, and are excluded from the timed run)\n",
           safe_count, pre_ok);
    record("TOONc", "toon_to_json", bytes, ops, seconds, (long)n * CTOON_BENCH_REPEATS);
    free(safe);
}

/* ---------------------------------------------------------------- scaling --
 * Sort the corpus by file size, split into SCALING_NBUCKETS equal-COUNT
 * buckets, and re-measure each bucket on its own (fewer reps than the
 * main comparison -- this runs NBUCKETS times, so keeping SCALING_REPS
 * small keeps the whole benchmark's wall time reasonable). Each bucket's
 * x-axis position is the MEDIAN file size within it, not its edges --
 * a single representative point per bucket, for a line chart.
 */
static void bench_ctoon_scaling(bench_file **sorted, size_t n) {
    size_t base = n / SCALING_NBUCKETS, rem = n % SCALING_NBUCKETS, start = 0;
    for (int b = 0; b < SCALING_NBUCKETS; b++) {
        size_t count = base + (b < (int)rem ? 1 : 0);
        if (count == 0) { continue; }
        bench_file **bucket = sorted + start;
        double size_bytes = (double)bucket[count / 2]->len;

        long ops = 0; double bytes = 0;
        for (int rep = 0; rep < SCALING_REPS; rep++) {
            for (size_t i = 0; i < count; i++) {
                size_t len = 0;
                char *toon = ctoon_json_to_toon(bucket[i]->data, bucket[i]->len, &len);
                if (toon) { free(toon); ops++; bytes += (double)bucket[i]->len; }
            }
        }
        double t0 = now_seconds();
        for (int rep = 0; rep < SCALING_REPS; rep++) {
            for (size_t i = 0; i < count; i++) {
                size_t len = 0;
                char *toon = ctoon_json_to_toon(bucket[i]->data, bucket[i]->len, &len);
                if (toon) free(toon);
            }
        }
        double seconds = now_seconds() - t0;
        record_scaling("ctoon", "json_to_toon", size_bytes, bytes, ops, seconds, (long)count * SCALING_REPS);

        ops = 0; bytes = 0; t0 = now_seconds();
        for (int rep = 0; rep < SCALING_REPS; rep++) {
            for (size_t i = 0; i < count; i++) {
                if (!bucket[i]->toon) continue;
                size_t len = 0;
                char *json = ctoon_toon_to_json(bucket[i]->toon, bucket[i]->toon_len, &len);
                if (json) { free(json); ops++; bytes += (double)bucket[i]->toon_len; }
            }
        }
        seconds = now_seconds() - t0;
        record_scaling("ctoon", "toon_to_json", size_bytes, bytes, ops, seconds, (long)count * SCALING_REPS);

        ops = 0; bytes = 0; t0 = now_seconds();
        for (int rep = 0; rep < SCALING_REPS; rep++) {
            for (size_t i = 0; i < count; i++) {
                size_t tlen = 0;
                char *toon = ctoon_json_to_toon(bucket[i]->data, bucket[i]->len, &tlen);
                if (!toon) continue;
                size_t jlen = 0;
                char *json = ctoon_toon_to_json(toon, tlen, &jlen);
                free(toon);
                if (json) { free(json); ops++; bytes += (double)bucket[i]->len; }
            }
        }
        seconds = now_seconds() - t0;
        record_scaling("ctoon", "roundtrip", size_bytes, bytes, ops, seconds, (long)count * SCALING_REPS);

        ops = 0; bytes = 0; t0 = now_seconds();
        for (int rep = 0; rep < SCALING_REPS; rep++) {
            for (size_t i = 0; i < count; i++) {
                if (!bucket[i]->toon) continue;
                ctoon_doc *doc = ctoon_read(bucket[i]->toon, bucket[i]->toon_len, 0);
                if (!doc) continue;
                ops++; bytes += (double)bucket[i]->toon_len;
                ctoon_doc_free(doc);
            }
        }
        seconds = now_seconds() - t0;
        record_scaling("ctoon", "toon_to_struct", size_bytes, bytes, ops, seconds, (long)count * SCALING_REPS);

        ops = 0; bytes = 0; t0 = now_seconds();
        for (int rep = 0; rep < SCALING_REPS; rep++) {
            for (size_t i = 0; i < count; i++) {
                if (!bucket[i]->doc) continue;
                size_t len = 0;
                char *toon = ctoon_write(bucket[i]->doc, &len);
                if (toon) { free(toon); ops++; bytes += (double)bucket[i]->len; }
            }
        }
        seconds = now_seconds() - t0;
        record_scaling("ctoon", "struct_to_toon", size_bytes, bytes, ops, seconds, (long)count * SCALING_REPS);

        start += count;
    }
}

static void bench_toonc_scaling(bench_file **sorted, size_t n) {
    fflush(stdout);
    int devnull = open("/dev/null", O_WRONLY);
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);
    if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); }

    size_t base = n / SCALING_NBUCKETS, rem = n % SCALING_NBUCKETS, start = 0;
    for (int b = 0; b < SCALING_NBUCKETS; b++) {
        size_t count = base + (b < (int)rem ? 1 : 0);
        if (count == 0) { continue; }
        bench_file **bucket = sorted + start;
        double size_bytes = (double)bucket[count / 2]->len;

        /* Same crash-safety probe as bench_toonc(), scoped to this bucket. */
        char *safe = (char *)calloc(count, 1);
        for (size_t i = 0; i < count; i++) {
            if (!bucket[i]->toon) continue;
            pid_t pid = fork();
            if (pid == 0) {
                toonObject *obj = TOONc_parseString(bucket[i]->toon);
                if (obj) {
                    char *membuf = NULL; size_t memsize = 0;
                    FILE *mem = open_memstream(&membuf, &memsize);
                    if (mem) { TOONc_toJSON(obj, mem, 0); fclose(mem); free(membuf); }
                    TOONc_free(obj);
                }
                _exit(obj ? 0 : 1);
            } else if (pid > 0) {
                int status = 0;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) safe[i] = 1;
            }
        }

        long ops = 0; double bytes = 0;
        double t0 = now_seconds();
        for (int rep = 0; rep < SCALING_REPS; rep++) {
            for (size_t i = 0; i < count; i++) {
                if (!safe[i]) continue;
                toonObject *obj = TOONc_parseString(bucket[i]->toon);
                if (!obj) continue;
                char *membuf = NULL; size_t memsize = 0;
                FILE *mem = open_memstream(&membuf, &memsize);
                if (mem) { TOONc_toJSON(obj, mem, 0); fclose(mem); free(membuf); ops++; bytes += (double)bucket[i]->toon_len; }
                TOONc_free(obj);
            }
        }
        double seconds = now_seconds() - t0;
        record_scaling("TOONc", "toon_to_json", size_bytes, bytes, ops, seconds, (long)count * SCALING_REPS);
        free(safe);
        start += count;
    }

    fflush(stdout);
    if (devnull >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        dup2(saved_stderr, STDERR_FILENO);
        close(devnull); close(saved_stdout); close(saved_stderr);
    }
}

static void run_order_checks(void) {
    char *json = order_read_sample("sample.json"), *toon = order_read_sample("sample.toon");
    if (!json || !toon) {
        fprintf(stderr, "warning: order-check samples not found in %s\n", CTOON_BENCH_ORDER_DIR);
        free(json); free(toon);
        return;
    }
    order_keys expected, got;
    int ne = order_json_keys(json, expected), ng;

    /* ctoon, JSON -> TOON */
    size_t out_len = 0;
    char *out = ctoon_json_to_toon(json, strlen(json), &out_len);
    if (out) {
        ng = order_toon_keys(out, got);
        record_order_keys("ctoon", "json_to_toon", expected, ne, got, ng);
        free(out);
    } else {
        record_order_check("ctoon", "json_to_toon", false);
    }

    /* ctoon, TOON -> JSON */
    out = ctoon_toon_to_json(toon, strlen(toon), &out_len);
    if (out) {
        ng = order_json_keys(out, got);
        record_order_keys("ctoon", "toon_to_json", expected, ne, got, ng);
        free(out);
    } else {
        record_order_check("ctoon", "toon_to_json", false);
    }

    /* TOONc has no JSON parser, so only TOON -> JSON exists for it. */
    fflush(stdout);
    int devnull = open("/dev/null", O_WRONLY);
    int saved_stdout = dup(STDOUT_FILENO);
    if (devnull >= 0) dup2(devnull, STDOUT_FILENO);
    toonObject *obj = TOONc_parseString(toon);
    if (devnull >= 0) { dup2(saved_stdout, STDOUT_FILENO); close(devnull); close(saved_stdout); }
    if (obj) {
        char *membuf = NULL; size_t memsize = 0;
        FILE *mem = open_memstream(&membuf, &memsize);
        if (mem) {
            TOONc_toJSON(obj, mem, 0);
            fclose(mem);
            ng = order_json_keys(membuf, got);
            record_order_keys("TOONc", "toon_to_json", expected, ne, got, ng);
            free(membuf);
        } else {
            record_order_check("TOONc", "toon_to_json", false);
        }
        TOONc_free(obj);
    } else {
        record_order_check("TOONc", "toon_to_json", false);
    }
    free(json); free(toon);
}

static void write_results_json(size_t n_files, size_t total_json_bytes) {
    FILE *f = fopen(CTOON_BENCH_RESULTS_JSON, "w");
    if (!f) { fprintf(stderr, "warning: could not write %s\n", CTOON_BENCH_RESULTS_JSON); return; }
    fprintf(f, "{\n  \"language\": \"c\",\n");
    fprintf(f, "  \"corpus\": {\"files\": %zu, \"bytes\": %zu},\n", n_files, total_json_bytes);
    fprintf(f, "  \"results\": [\n");
    for (int i = 0; i < g_result_count; i++) {
        bench_result *r = &g_results[i];
        fprintf(f,
            "    {\"library\": \"%s\", \"operation\": \"%s\", "
            "\"throughput_mb_s\": %.4f, \"docs_per_sec\": %.2f, "
            "\"success_rate\": %.4f, \"total_time_s\": %.6f}%s\n",
            r->library, r->operation, r->throughput_mb_s, r->docs_per_sec,
            r->success_rate, r->total_time_s, (i + 1 < g_result_count) ? "," : "");
    }
    fprintf(f, "  ]\n,");
    fprintf(f, "\n  \"scaling\": [\n");
    for (int i = 0; i < g_scaling_count; i++) {
        scaling_result *r = &g_scaling[i];
        fprintf(f,
            "    {\"library\": \"%s\", \"operation\": \"%s\", "
            "\"size_bytes\": %.1f, \"throughput_mb_s\": %.4f, "
            "\"docs_per_sec\": %.2f, \"success_rate\": %.4f}%s\n",
            r->library, r->operation, r->size_bytes, r->throughput_mb_s,
            r->docs_per_sec, r->success_rate, (i + 1 < g_scaling_count) ? "," : "");
    }
    fprintf(f, "  ]\n,");
    fprintf(f, "\n  \"order_check\": [\n");
    for (int i = 0; i < g_order_check_count; i++) {
        order_check_result *r = &g_order_checks[i];
        fprintf(f, "    {\"library\": \"%s\", \"operation\": \"%s\", \"preserved\": %s}%s\n",
                r->library, r->operation, r->preserved ? "true" : "false",
                (i + 1 < g_order_check_count) ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
    printf("\nResults written to %s\n", CTOON_BENCH_RESULTS_JSON);
}

int main(int argc, char **argv) {
    if (argc >= 2 && argv[1][0] != '\0') {
        g_log = fopen(argv[1], "w");
        if (!g_log) fprintf(stderr, "warning: could not open log file %s\n", argv[1]);
    }

    FILE *mf = fopen(CTOON_BENCH_MANIFEST, "r");
    if (!mf) {
        fprintf(stderr, "Cannot open manifest: %s\n", CTOON_BENCH_MANIFEST);
        return 1;
    }

    size_t cap = 1024, n = 0;
    bench_file *files = (bench_file *)malloc(cap * sizeof(bench_file));
    char line[4096];

    while (fgets(line, sizeof(line), mf)) {
        size_t l = strlen(line);
        while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = '\0';
        if (l == 0) continue;

        size_t len = 0;
        char *data = read_whole_file(line, &len);
        if (!data) continue;

        if (n == cap) { cap *= 2; files = (bench_file *)realloc(files, cap * sizeof(bench_file)); }
        files[n].path = dup_str(line);
        files[n].data = data;
        files[n].len = len;
        files[n].toon = NULL;
        files[n].toon_len = 0;
        files[n].doc = NULL;
        n++;
    }
    fclose(mf);

    if (n == 0) {
        fprintf(stderr, "Corpus manifest is empty - nothing to benchmark.\n");
        return 1;
    }

    size_t total_json_bytes = 0;
    for (size_t i = 0; i < n; i++) total_json_bytes += files[i].len;

    printf("CToon Benchmarks - C\n");
    printf("Corpus: %zu files, %.2f MB (JSON)\n\n", n, (double)total_json_bytes / 1e6);
    printf("%-8s %-14s %12s %14s %9s %10s %12s\n",
           "Library", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time");

    /* Untimed pre-pass: produce TOON text for each file with ctoon, since
       it's the only library here that can also parse JSON. */
    size_t pre_ok = 0;
    for (size_t i = 0; i < n; i++) {
        ctoon_read_err rerr; memset(&rerr, 0, sizeof(rerr));
        ctoon_doc *doc = ctoon_read_json(files[i].data, files[i].len, 0, NULL, &rerr);
        if (!doc) { log_fail("ctoon", "pre_pass", files[i].path, rerr.msg); continue; }
        size_t len = 0;
        char *toon = ctoon_write(doc, &len);
        if (!toon) { ctoon_doc_free(doc); log_fail("ctoon", "pre_pass", files[i].path, "ctoon_write failed"); continue; }
        files[i].doc = doc;     /* kept alive for struct_to_toon */
        files[i].toon = toon;
        files[i].toon_len = len;
        pre_ok++;
    }
    if (pre_ok == 0) {
        fprintf(stderr, "No file in the corpus could be converted - aborting.\n");
        return 1;
    }

    bool run_ctoon = true, run_toonc = true;
    const char *only = getenv("CTOON_BENCH_ONLY");
    if (!only || !only[0]) dump_init();
    if (only && only[0]) {
        run_ctoon = (strcmp(only, "ctoon") == 0);
        run_toonc = (strcmp(only, "TOONc") == 0);
    }
    if (run_ctoon) bench_ctoon(files, n);
    if (run_toonc) bench_toonc(files, n, pre_ok);

    /* Scaling: same libraries, re-measured across size buckets so the
       report can plot throughput vs. input size. Skipped in memory-only
       mode (CTOON_BENCH_ONLY set) -- that run only needs a peak RSS
       number, not more timing work. */
    if (!only || !only[0]) {
        bench_file **sorted = (bench_file **)malloc(n * sizeof(bench_file *));
        for (size_t i = 0; i < n; i++) sorted[i] = &files[i];
        qsort(sorted, n, sizeof(bench_file *), cmp_bench_file_ptr_by_len);
        bench_ctoon_scaling(sorted, n);
        bench_toonc_scaling(sorted, n);
        free(sorted);
        run_order_checks();
    }

    if (!only || !only[0]) write_results_json(n, total_json_bytes);

    if (g_log) {
        fclose(g_log);
        printf("Debug log written to %s\n", argv[1]);
    }

    for (size_t i = 0; i < n; i++) {
        free(files[i].path);
        free(files[i].data);
        free(files[i].toon);
    }
    free(files);
    return 0;
}
