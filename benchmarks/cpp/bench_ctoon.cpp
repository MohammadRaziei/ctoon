/*
 * CToon C++ benchmark.
 *
 * We're the reference implementation for the C++ binding, so this isn't run
 * against any competing library — it just tracks the wrapper's own
 * throughput (and the RAII/exception overhead over the raw C API) across
 * changes to ctoon.hpp.
 *
 * Same methodology as every other language benchmark in this repo — see
 * benchmarks/README.md — so the numbers line up with the C, Python, and Go
 * results for the same corpus.
 */

#define CTOON_ENABLE_JSON 1
#include "ctoon.hpp"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef CTOON_BENCH_MANIFEST
#error "CTOON_BENCH_MANIFEST must be defined at compile time"
#endif

#ifndef CTOON_BENCH_REPEATS
#define CTOON_BENCH_REPEATS 20
#endif

namespace {

struct BenchFile {
    std::string path;
    std::string json;
    std::string toon; // filled in during the pre-pass
};

double now_seconds() {
    return static_cast<double>(std::clock()) / static_cast<double>(CLOCKS_PER_SEC);
}

bool read_whole_file(const std::string &path, std::string &out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

} // namespace

int main() {
    std::ifstream manifest(CTOON_BENCH_MANIFEST);
    if (!manifest) {
        std::fprintf(stderr, "Cannot open manifest: %s\n", CTOON_BENCH_MANIFEST);
        return 1;
    }

    std::vector<BenchFile> files;
    std::string line;
    while (std::getline(manifest, line)) {
        if (line.empty()) continue;
        BenchFile bf;
        bf.path = line;
        if (!read_whole_file(line, bf.json)) continue;
        files.push_back(std::move(bf));
    }

    if (files.empty()) {
        std::fprintf(stderr, "Corpus manifest is empty - nothing to benchmark.\n");
        return 1;
    }

    std::size_t total_json_bytes = 0;
    for (const auto &f : files) total_json_bytes += f.json.size();

    std::printf("CToon C++ Benchmark\n");
    std::printf("Corpus: %zu files, %.2f MB (JSON)\n\n",
                 files.size(), static_cast<double>(total_json_bytes) / 1e6);

    // ── Untimed pre-pass: produce TOON text for each file ──
    std::size_t total_toon_bytes = 0;
    for (auto &f : files) {
        try {
            auto doc = ctoon::document::from_json(f.json);
            f.toon = doc.to_string().str();
            total_toon_bytes += f.toon.size();
        } catch (const ctoon::error &) {
            // Skip files that don't round-trip (e.g. a scalar-only JSON
            // document, which some corpora include as edge cases).
        }
    }

    // ── Phase A: JSON -> TOON ──
    double t0 = now_seconds();
    long ops_a = 0;
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (const auto &f : files) {
            try {
                auto doc = ctoon::document::from_json(f.json);
                auto result = doc.to_string();
                (void)result.size();
                ops_a++;
            } catch (const ctoon::error &) {
                // counted as skipped, not an error - matches the pre-pass
            }
        }
    }
    double t_json_to_toon = now_seconds() - t0;
    double bytes_a = static_cast<double>(total_json_bytes) * CTOON_BENCH_REPEATS;

    // ── Phase B: TOON -> JSON (using the pre-pass output) ──
    t0 = now_seconds();
    long ops_b = 0;
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (const auto &f : files) {
            if (f.toon.empty()) continue;
            try {
                auto doc = ctoon::document::parse(f.toon);
                auto result = doc.to_json(2);
                (void)result.size();
                ops_b++;
            } catch (const ctoon::error &) {
            }
        }
    }
    double t_toon_to_json = now_seconds() - t0;
    double bytes_b = static_cast<double>(total_toon_bytes) * CTOON_BENCH_REPEATS;

    // ── Report ──
    std::printf("%-14s %12s %14s %12s\n", "Operation", "Throughput", "Docs/sec", "Total time");
    std::printf("%-14s %9.2f MB/s %14.0f %9.4f s  (x%d reps)\n",
                 "JSON -> TOON", bytes_a / t_json_to_toon / 1e6,
                 static_cast<double>(ops_a) / t_json_to_toon, t_json_to_toon, CTOON_BENCH_REPEATS);
    std::printf("%-14s %9.2f MB/s %14.0f %9.4f s  (x%d reps)\n",
                 "TOON -> JSON", bytes_b / t_toon_to_json / 1e6,
                 static_cast<double>(ops_b) / t_toon_to_json, t_toon_to_json, CTOON_BENCH_REPEATS);

    return 0;
}
