/*
 * CToon C benchmark.
 *
 * Methodology (same across every language benchmark in this repo, so the
 * numbers are directly comparable):
 *
 *   1. Load every file in the corpus manifest into memory (untimed).
 *   2. Untimed pre-pass: convert each JSON file to TOON once, to have TOON
 *      input ready for the decode benchmark.
 *   3. Timed "JSON -> TOON": repeatedly parse JSON and re-serialise to TOON.
 *   4. Timed "TOON -> JSON": repeatedly parse TOON and re-serialise to JSON.
 *   5. Report throughput — MB/s of the bytes actually processed by
 *      *successful* conversions only (not the full corpus assumed) — and
 *      documents/sec, plus the success rate, so a competitor that fails on
 *      part of the corpus doesn't get an inflated number.
 *
 * The corpus manifest is a newline-separated list of JSON file paths,
 * generated once by benchmarks/CMakeLists.txt so every language reads the
 * exact same files.
 *
 * If CTOON_BENCH_HAVE_TOONC is set, also compares against TOONc
 * (github.com/UsboKirishima/TOONc) — TOON -> JSON only, since TOONc has no
 * JSON parser and no TOON writer of its own.
 */

#define _POSIX_C_SOURCE 200809L /* for open_memstream, used in the TOONc comparison */

#define CTOON_ENABLE_JSON 1
#include "ctoon.h"

#if CTOON_BENCH_HAVE_TOONC
#include "toonc.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if CTOON_BENCH_HAVE_TOONC
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

#ifndef CTOON_BENCH_MANIFEST
#error "CTOON_BENCH_MANIFEST must be defined at compile time"
#endif

#ifndef CTOON_BENCH_REPEATS
#define CTOON_BENCH_REPEATS 20
#endif

typedef struct {
    char   *path;
    char   *data; /* JSON source, owned */
    size_t  len;
    char   *toon; /* TOON re-encoding of `data`, owned, filled in pre-pass */
    size_t  toon_len;
} bench_file;

typedef struct {
    double  seconds;
    long    ops;
    double  bytes; /* bytes of only the files that actually succeeded */
} bench_result;

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

static void report(const char *label, bench_result r, size_t files_total) {
    printf("%-14s %9.2f MB/s %14.0f %8.0f%%  %8.4f s  (x%d reps)\n",
           label,
           r.ops ? r.bytes / r.seconds / 1e6 : 0.0,
           (double)r.ops / r.seconds,
           100.0 * (double)r.ops / ((double)files_total * CTOON_BENCH_REPEATS),
           r.seconds, CTOON_BENCH_REPEATS);
}

int main(void) {
    /* ── Load manifest ── */
    FILE *mf = fopen(CTOON_BENCH_MANIFEST, "r");
    if (!mf) {
        fprintf(stderr, "Cannot open manifest: %s\n", CTOON_BENCH_MANIFEST);
        return 1;
    }

    size_t   cap = 1024, n = 0;
    bench_file *files = (bench_file *)malloc(cap * sizeof(bench_file));
    char line[4096];

    while (fgets(line, sizeof(line), mf)) {
        size_t l = strlen(line);
        while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = '\0';
        if (l == 0) continue;

        size_t len = 0;
        char *data = read_whole_file(line, &len);
        if (!data) continue; /* skip unreadable entries */

        if (n == cap) {
            cap *= 2;
            files = (bench_file *)realloc(files, cap * sizeof(bench_file));
        }
        files[n].path = dup_str(line);
        files[n].data = data;
        files[n].len  = len;
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

    printf("CToon C Benchmark\n");
    printf("Corpus: %zu files, %.2f MB (JSON)\n\n",
           n, (double)total_json_bytes / 1e6);

    /* ── Untimed pre-pass: produce TOON text for each file (with ctoon) ── */
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

    /* ── Phase A: JSON -> TOON (ctoon) ── */
    bench_result res_a = {0, 0, 0};
    double t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (size_t i = 0; i < n; i++) {
            ctoon_doc *doc = ctoon_read_json(files[i].data, files[i].len, 0, NULL, NULL);
            if (!doc) continue;
            size_t len = 0;
            char *toon = ctoon_write(doc, &len);
            if (toon) {
                free(toon);
                res_a.ops++;
                res_a.bytes += (double)files[i].len;
            }
            ctoon_doc_free(doc);
        }
    }
    res_a.seconds = now_seconds() - t0;

    /* ── Phase B: TOON -> JSON (ctoon, using the pre-pass output) ── */
    bench_result res_b = {0, 0, 0};
    t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (size_t i = 0; i < n; i++) {
            if (!files[i].toon) continue;
            ctoon_doc *doc = ctoon_read(files[i].toon, files[i].toon_len, 0);
            if (!doc) continue;
            size_t len = 0;
            char *json = ctoon_doc_to_json(doc, 2, CTOON_WRITE_NOFLAG, NULL, &len, NULL);
            if (json) {
                free(json);
                res_b.ops++;
                res_b.bytes += (double)files[i].toon_len;
            }
            ctoon_doc_free(doc);
        }
    }
    res_b.seconds = now_seconds() - t0;

    /* ── Report ── */
    printf("%-14s %12s %14s %9s %10s %12s\n",
           "Operation", "Throughput", "Docs/sec", "Success", "", "Total time");
    report("JSON -> TOON", res_a, n);
    report("TOON -> JSON", res_b, n);

#if CTOON_BENCH_HAVE_TOONC
    /*
     * Competitor: TOONc (github.com/UsboKirishima/TOONc). It has no JSON
     * parser and no TOON writer -- only TOON -> JSON -- so that is the only
     * direction it can be compared on. It parses the exact same TOON text
     * ctoon produced in the pre-pass above, ensuring the input is identical
     * for both libraries. TOONc writes to a FILE*, so we capture that into
     * memory with open_memstream() for a fair timing comparison (no real
     * file I/O on either side).
     *
     * TOONc's parser is not robust against everything ctoon emits — it
     * segfaults/double-frees on a good chunk of this corpus. Rather than
     * take the whole benchmark process down, each file is first probed in
     * a forked child (isolating any crash), and only files that survive
     * the probe go into the timed comparison. The crash rate itself is
     * reported — that incompatibility is a real, relevant data point.
     */
    printf("\nvs TOONc (TOON -> JSON only — TOONc has no JSON parser or TOON writer)\n");

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

    char *toonc_safe = (char *)calloc(n, 1);
    size_t toonc_safe_count = 0;
    for (size_t i = 0; i < n; i++) {
        if (!files[i].toon) continue;
        pid_t pid = fork();
        if (pid == 0) {
            toonObject *obj = TOONc_parseString(files[i].toon);
            if (obj) {
                char *membuf = NULL;
                size_t memsize = 0;
                FILE *mem = open_memstream(&membuf, &memsize);
                if (mem) { TOONc_toJSON(obj, mem, 0); fclose(mem); free(membuf); }
                TOONc_free(obj);
            }
            _exit(obj ? 0 : 1);
        } else if (pid > 0) {
            int status = 0;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                toonc_safe[i] = 1;
                toonc_safe_count++;
            }
        }
    }

    bench_result res_c = {0, 0, 0};
    double t0c = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (size_t i = 0; i < n; i++) {
            if (!toonc_safe[i]) continue;
            toonObject *obj = TOONc_parseString(files[i].toon);
            if (!obj) continue;

            char *membuf = NULL;
            size_t memsize = 0;
            FILE *mem = open_memstream(&membuf, &memsize);
            if (mem) {
                TOONc_toJSON(obj, mem, 0);
                fclose(mem);
                free(membuf);
                res_c.ops++;
                res_c.bytes += (double)files[i].toon_len;
            }
            TOONc_free(obj);
        }
    }
    res_c.seconds = now_seconds() - t0c;

    fflush(stdout);
    if (devnull >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        dup2(saved_stderr, STDERR_FILENO);
        close(devnull);
        close(saved_stdout);
        close(saved_stderr);
    }

    printf("Files TOONc's parser can safely handle: %zu/%zu (%.0f%%) — "
           "the rest crash its parser on this corpus\n",
           toonc_safe_count, pre_ok, 100.0 * (double)toonc_safe_count / (double)pre_ok);
    report("TOON -> JSON", res_c, toonc_safe_count);
    free(toonc_safe);
#endif

    for (size_t i = 0; i < n; i++) {
        free(files[i].path);
        free(files[i].data);
        free(files[i].toon);
    }
    free(files);
    return 0;
}
