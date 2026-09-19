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

#include <algorithm>
#include <cctype>
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

struct ScalingResult {
    std::string library, operation;
    double size_bytes, throughput_mb_s, docs_per_sec, success_rate;
};

std::vector<Result> g_results;
std::vector<ScalingResult> g_scaling;
constexpr int SCALING_NBUCKETS = 5;
constexpr int SCALING_REPS = 5;

// ------------------------------------------------------------ order check --
// Same fixed, deliberately non-alphabetical sample and same check as
// bench.c / bench.py -- see bench.py's docstring for the rationale.
const std::string kOrderCheckSampleJson =
    R"({"zebra": 1, "apple": 2, "mango": 3, )"
    R"("nested": {"beta": true, "alpha": false}, )"
    R"("list": [{"z": 1, "a": 2}, {"z": 3, "a": 4}]})";

std::vector<std::string> extract_key_order(const std::string &text) {
    std::vector<std::string> keys;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '"') {
            size_t start = i + 1;
            size_t end = text.find('"', start);
            if (end == std::string::npos) break;
            size_t q = end + 1;
            while (q < text.size() && std::isspace(static_cast<unsigned char>(text[q]))) q++;
            if (q < text.size() && text[q] == ':') {
                keys.push_back(text.substr(start, end - start));
            }
            i = end + 1;
        } else {
            i++;
        }
    }
    return keys;
}

bool order_preserved(const std::string &output_json) {
    return extract_key_order(kOrderCheckSampleJson) == extract_key_order(output_json);
}

struct OrderCheckResult { std::string library; bool preserved; std::string detail; };
std::vector<OrderCheckResult> g_order_checks;

void run_order_check() {
    try {
        auto doc = ctoon::document::from_json(kOrderCheckSampleJson);
        std::string toon = doc.to_string().str();
        auto doc2 = ctoon::document::parse(toon);
        auto json_result = doc2.to_json(2);
        std::string json_out = json_result.str();
        g_order_checks.push_back({"ctoon", order_preserved(json_out), ""});
    } catch (const ctoon::error &e) {
        g_order_checks.push_back({"ctoon", false, std::string("exception: ") + e.what()});
    }
}

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

void record_scaling(const char *library, const char *operation, double size_bytes,
                     double bytes, long ops, double seconds, long attempted) {
    ScalingResult r;
    r.library = library;
    r.operation = operation;
    r.size_bytes = size_bytes;
    r.throughput_mb_s = ops ? bytes / seconds / 1e6 : 0.0;
    r.docs_per_sec = static_cast<double>(ops) / seconds;
    r.success_rate = attempted ? static_cast<double>(ops) / static_cast<double>(attempted) : 0.0;
    g_scaling.push_back(r);
}

/* Sort the corpus by file size, split into SCALING_NBUCKETS equal-COUNT
   buckets, re-measure each on its own (fewer reps than the main
   comparison) -- see bench.c's identical scheme for the full rationale. */
void bench_scaling(std::vector<const BenchFile *> &sorted) {
    size_t n = sorted.size();
    size_t base = n / SCALING_NBUCKETS, rem = n % SCALING_NBUCKETS, start = 0;
    for (int b = 0; b < SCALING_NBUCKETS; b++) {
        size_t count = base + (static_cast<size_t>(b) < rem ? 1 : 0);
        if (count == 0) continue;
        auto bucket_begin = sorted.begin() + static_cast<long>(start);
        double size_bytes = static_cast<double>(sorted[start + count / 2]->json.size());

        long ops = 0; double bytes = 0;
        double t0 = now_seconds();
        for (int rep = 0; rep < SCALING_REPS; rep++) {
            for (auto it = bucket_begin; it != bucket_begin + static_cast<long>(count); ++it) {
                const BenchFile &f = **it;
                try {
                    auto doc = ctoon::document::from_json(f.json);
                    auto result = doc.to_string();
                    (void)result.size();
                    ops++; bytes += static_cast<double>(f.json.size());
                } catch (const ctoon::error &) {}
            }
        }
        record_scaling("ctoon", "json_to_toon", size_bytes, bytes, ops,
                        now_seconds() - t0, static_cast<long>(count) * SCALING_REPS);

        ops = 0; bytes = 0; t0 = now_seconds();
        for (int rep = 0; rep < SCALING_REPS; rep++) {
            for (auto it = bucket_begin; it != bucket_begin + static_cast<long>(count); ++it) {
                const BenchFile &f = **it;
                if (f.toon.empty()) continue;
                try {
                    auto doc = ctoon::document::parse(f.toon);
                    auto result = doc.to_json(2);
                    (void)result.size();
                    ops++; bytes += static_cast<double>(f.toon.size());
                } catch (const ctoon::error &) {}
            }
        }
        record_scaling("ctoon", "toon_to_json", size_bytes, bytes, ops,
                        now_seconds() - t0, static_cast<long>(count) * SCALING_REPS);

        ops = 0; bytes = 0; t0 = now_seconds();
        for (int rep = 0; rep < SCALING_REPS; rep++) {
            for (auto it = bucket_begin; it != bucket_begin + static_cast<long>(count); ++it) {
                const BenchFile &f = **it;
                try {
                    auto doc1 = ctoon::document::from_json(f.json);
                    std::string toon = doc1.to_string().str();
                    auto doc2 = ctoon::document::parse(toon);
                    auto result = doc2.to_json(2);
                    (void)result.size();
                    ops++; bytes += static_cast<double>(f.json.size());
                } catch (const ctoon::error &) {}
            }
        }
        record_scaling("ctoon", "roundtrip", size_bytes, bytes, ops,
                        now_seconds() - t0, static_cast<long>(count) * SCALING_REPS);

        start += count;
    }
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
    f << "  ]\n,\n  \"scaling\": [\n";
    for (size_t i = 0; i < g_scaling.size(); i++) {
        const ScalingResult &r = g_scaling[i];
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "    {\"library\": \"%s\", \"operation\": \"%s\", "
            "\"size_bytes\": %.1f, \"throughput_mb_s\": %.4f, "
            "\"docs_per_sec\": %.2f, \"success_rate\": %.4f}%s\n",
            r.library.c_str(), r.operation.c_str(), r.size_bytes,
            r.throughput_mb_s, r.docs_per_sec, r.success_rate,
            (i + 1 < g_scaling.size()) ? "," : "");
        f << buf;
    }
    f << "  ]\n,";
    f << "\n  \"order_check\": [\n";
    for (size_t i = 0; i < g_order_checks.size(); i++) {
        const OrderCheckResult &r = g_order_checks[i];
        char buf[256];
        std::snprintf(buf, sizeof(buf), "    {\"library\": \"%s\", \"preserved\": %s, \"detail\": \"%s\"}%s\n",
                      r.library.c_str(), r.preserved ? "true" : "false", r.detail.c_str(),
                      (i + 1 < g_order_checks.size()) ? "," : "");
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

    // roundtrip: json -> toon -> (parse toon) -> json, chained as one
    // operation per file rather than the two legs above run separately.
    long ops_c = 0; double bytes_c = 0;
    t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (const auto &f : files) {
            try {
                auto doc1 = ctoon::document::from_json(f.json);
                std::string toon = doc1.to_string().str();
                auto doc2 = ctoon::document::parse(toon);
                auto result = doc2.to_json(2);
                (void)result.size();
                ops_c++;
                bytes_c += static_cast<double>(f.json.size());
            } catch (const ctoon::error &) {
            }
        }
    }
    record("ctoon", "roundtrip", bytes_c, ops_c, now_seconds() - t0,
           static_cast<long>(files.size()) * CTOON_BENCH_REPEATS);

    std::vector<const BenchFile *> sorted;
    sorted.reserve(files.size());
    for (const auto &f : files) sorted.push_back(&f);
    std::sort(sorted.begin(), sorted.end(),
              [](const BenchFile *a, const BenchFile *b) { return a->json.size() < b->json.size(); });
    bench_scaling(sorted);
    run_order_check();

    write_results_json(files.size(), total_json_bytes);
    return 0;
}
