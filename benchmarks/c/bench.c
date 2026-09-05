/*
 * CToon C benchmark.
 *
 * Every implementation here — ctoon included — is treated as one more
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
 *   5. Report throughput — MB/s of bytes actually processed by
 *      *successful* conversions only — and documents/sec, plus success
 *      rate, so a library that fails part of the corpus doesn't get an
 *      inflated number.
 *
 * TOONc (github.com/UsboKirishima/TOONc) has no JSON parser and no TOON
 * writer — only toon_to_json is meaningful for it. Its parser also isn't
 * robust against everything ctoon emits (it crashes on part of the
 * corpus), so each file is first probed in a forked child before being
 * included in the timed run — see bench_toonc().
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

static void bench_ctoon(bench_file *files, size_t n) {
    long ops = 0; double bytes = 0;
    double t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (size_t i = 0; i < n; i++) {
            ctoon_doc *doc = ctoon_read_json(files[i].data, files[i].len, 0, NULL, NULL);
            if (!doc) continue;
            size_t len = 0;
            char *toon = ctoon_write(doc, &len);
            if (toon) { free(toon); ops++; bytes += (double)files[i].len; }
            ctoon_doc_free(doc);
        }
    }
    record("ctoon", "json_to_toon", bytes, ops, now_seconds() - t0, (long)n * CTOON_BENCH_REPEATS);

    ops = 0; bytes = 0;
    t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (size_t i = 0; i < n; i++) {
            if (!files[i].toon) continue;
            ctoon_doc *doc = ctoon_read(files[i].toon, files[i].toon_len, 0);
            if (!doc) continue;
            size_t len = 0;
            char *json = ctoon_doc_to_json(doc, 2, CTOON_WRITE_NOFLAG, NULL, &len, NULL);
            if (json) { free(json); ops++; bytes += (double)files[i].toon_len; }
            ctoon_doc_free(doc);
        }
    }
    record("ctoon", "toon_to_json", bytes, ops, now_seconds() - t0, (long)n * CTOON_BENCH_REPEATS);
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
            }
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

    printf("(TOONc's parser safely handles %zu/%zu TOON files — the rest crash "
           "its parser on this corpus, and are excluded from the timed run)\n",
           safe_count, pre_ok);
    record("TOONc", "toon_to_json", bytes, ops, seconds, (long)n * CTOON_BENCH_REPEATS);
    free(safe);
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
    fprintf(f, "  ]\n}\n");
    fclose(f);
    printf("\nResults written to %s\n", CTOON_BENCH_RESULTS_JSON);
}

int main(void) {
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
        n++;
    }
    fclose(mf);

    if (n == 0) {
        fprintf(stderr, "Corpus manifest is empty — nothing to benchmark.\n");
        return 1;
    }

    size_t total_json_bytes = 0;
    for (size_t i = 0; i < n; i++) total_json_bytes += files[i].len;

    printf("CToon Benchmarks — C\n");
    printf("Corpus: %zu files, %.2f MB (JSON)\n\n", n, (double)total_json_bytes / 1e6);
    printf("%-8s %-14s %12s %14s %9s %10s %12s\n",
           "Library", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time");

    /* Untimed pre-pass: produce TOON text for each file with ctoon, since
       it's the only library here that can also parse JSON. */
    size_t pre_ok = 0;
    for (size_t i = 0; i < n; i++) {
        ctoon_doc *doc = ctoon_read_json(files[i].data, files[i].len, 0, NULL, NULL);
        if (!doc) continue;
        size_t len = 0;
        char *toon = ctoon_write(doc, &len);
        ctoon_doc_free(doc);
        if (!toon) continue;
        files[i].toon = toon;
        files[i].toon_len = len;
        pre_ok++;
    }
    if (pre_ok == 0) {
        fprintf(stderr, "No file in the corpus could be converted — aborting.\n");
        return 1;
    }

    bench_ctoon(files, n);
    bench_toonc(files, n, pre_ok);

    write_results_json(n, total_json_bytes);

    for (size_t i = 0; i < n; i++) {
        free(files[i].path);
        free(files[i].data);
        free(files[i].toon);
    }
    free(files);
    return 0;
}
