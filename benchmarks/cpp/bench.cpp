/*
 * CToon C++ benchmark.
 *
 * No competing C++ implementation exists in the ecosystem, so this is a
 * solo run — but it follows the exact same methodology, corpus, and JSON
 * output schema as every other language benchmark here, so the numbers
 * stay comparable across a rewrite of the wrapper.
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
#ifndef CTOON_BENCH_RESULTS_JSON
#error "CTOON_BENCH_RESULTS_JSON must be defined at compile time"
#endif
#ifndef CTOON_BENCH_REPEATS
#define CTOON_BENCH_REPEATS 20
#endif

namespace {

struct BenchFile {
    std::string path;
    std::string json;
    std::string toon;
};

struct Result {
    std::string library, operation;
    double throughput_mb_s, docs_per_sec, success_rate, total_time_s;
};

std::vector<Result> g_results;

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

void record(const char *library, const char *operation,
            double bytes, long ops, double seconds, long attempted) {
    Result r;
    r.library = library;
    r.operation = operation;
    r.throughput_mb_s = ops ? bytes / seconds / 1e6 : 0.0;
    r.docs_per_sec = static_cast<double>(ops) / seconds;
    r.success_rate = attempted ? static_cast<double>(ops) / static_cast<double>(attempted) : 0.0;
    r.total_time_s = seconds;
    g_results.push_back(r);

    std::printf("%-8s %-14s %9.2f MB/s %14.0f %8.0f%%  %8.4f s  (x%d reps)\n",
                library, operation, r.throughput_mb_s, r.docs_per_sec,
                r.success_rate * 100.0, seconds, CTOON_BENCH_REPEATS);
}

void write_results_json(size_t n_files, size_t total_json_bytes) {
    std::ofstream f(CTOON_BENCH_RESULTS_JSON);
    if (!f) {
        std::fprintf(stderr, "warning: could not write %s\n", CTOON_BENCH_RESULTS_JSON);
        return;
    }
    f << "{\n  \"language\": \"cpp\",\n";
    f << "  \"corpus\": {\"files\": " << n_files << ", \"bytes\": " << total_json_bytes << "},\n";
    f << "  \"results\": [\n";
    for (size_t i = 0; i < g_results.size(); i++) {
        const Result &r = g_results[i];
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "    {\"library\": \"%s\", \"operation\": \"%s\", "
            "\"throughput_mb_s\": %.4f, \"docs_per_sec\": %.2f, "
            "\"success_rate\": %.4f, \"total_time_s\": %.6f}%s\n",
            r.library.c_str(), r.operation.c_str(), r.throughput_mb_s,
            r.docs_per_sec, r.success_rate, r.total_time_s,
            (i + 1 < g_results.size()) ? "," : "");
        f << buf;
    }
    f << "  ]\n}\n";
    std::printf("\nResults written to %s\n", CTOON_BENCH_RESULTS_JSON);
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

    std::printf("CToon Benchmarks — C++\n");
    std::printf("Corpus: %zu files, %.2f MB (JSON)\n\n",
                 files.size(), static_cast<double>(total_json_bytes) / 1e6);
    std::printf("%-8s %-14s %12s %14s %9s %10s %12s\n",
                 "Library", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time");

    std::size_t total_toon_bytes = 0;
    for (auto &f : files) {
        try {
            auto doc = ctoon::document::from_json(f.json);
            f.toon = doc.to_string().str();
            total_toon_bytes += f.toon.size();
        } catch (const ctoon::error &) {
        }
    }

    long ops_a = 0; double bytes_a = 0;
    double t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (const auto &f : files) {
            try {
                auto doc = ctoon::document::from_json(f.json);
                auto result = doc.to_string();
                (void)result.size();
                ops_a++;
                bytes_a += static_cast<double>(f.json.size());
            } catch (const ctoon::error &) {
            }
        }
    }
    record("ctoon", "json_to_toon", bytes_a, ops_a, now_seconds() - t0,
           static_cast<long>(files.size()) * CTOON_BENCH_REPEATS);

    long ops_b = 0; double bytes_b = 0;
    t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (const auto &f : files) {
            if (f.toon.empty()) continue;
            try {
                auto doc = ctoon::document::parse(f.toon);
                auto result = doc.to_json(2);
                (void)result.size();
                ops_b++;
                bytes_b += static_cast<double>(f.toon.size());
            } catch (const ctoon::error &) {
            }
        }
    }
    record("ctoon", "toon_to_json", bytes_b, ops_b, now_seconds() - t0,
           static_cast<long>(files.size()) * CTOON_BENCH_REPEATS);

    write_results_json(files.size(), total_json_bytes);
    return 0;
}
