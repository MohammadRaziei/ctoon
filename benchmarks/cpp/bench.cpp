/*
 * CToon C++ benchmark.
 *
 * No competing C++ implementation exists in the ecosystem, so this is a
 * solo run - but it follows the exact same methodology, corpus, and JSON
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

#include <sys/stat.h>
#include <unistd.h>

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
    // ctoon's in-memory structure for this file, built UNTIMED in the
    // pre-pass; the input of struct_to_toon. Empty (has_doc == false) if the
    // file failed to parse.
    ctoon::document doc;
    bool has_doc = false;
};

struct Result {
    std::string library, operation;
    double throughput_mb_s, docs_per_sec, success_rate, total_time_s;
};

std::vector<Result> g_results;

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
        auto toon = ctoon::json_to_toon(kOrderCheckSampleJson);
        auto json_result = ctoon::toon_to_json(toon.view());
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

// NOTE: no size-scaling pass here. Only the C benchmark measures the
// speed-vs-file-size curve (see bench.c); every other language reports the
// single whole-corpus bar chart, so the languages stay comparable.

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

// Untimed. Writes what each operation actually produced for each file to
//   <results dir>/dump/cpp/<library>/<operation>/<file index>.txt
// (nothing for a file the library failed on) so report/verify_outputs.mjs can
// judge whether the output is CORRECT, not merely error-free -- see that
// script's header. (POSIX calls rather than std::filesystem: this benchmark
// is deliberately built as C++11.)
static void mkdir_p(const std::string &path) {
    for (std::size_t i = 1; i < path.size(); i++)
        if (path[i] == '/') mkdir(path.substr(0, i).c_str(), 0777);
    mkdir(path.c_str(), 0777);
}

static void dump_outputs(const std::vector<BenchFile> &files) {
    std::string results = CTOON_BENCH_RESULTS_JSON;
    std::size_t slash = results.rfind('/');
    if (slash == std::string::npos) return;
    const std::string root = results.substr(0, slash) + "/dump/cpp/ctoon";
    const char *ops[] = {"json_to_toon", "toon_to_json", "roundtrip", "toon_to_struct", "struct_to_toon"};
    for (const char *op : ops) {
        mkdir_p(root + "/" + op);
        for (std::size_t i = 0; i < files.size(); i++)   // drop stale files: "no file" must mean "failed"
            unlink((root + "/" + op + "/" + std::to_string(i) + ".txt").c_str());
    }

    auto write = [&](const char *op, std::size_t i, const std::string &text) {
        std::ofstream out(root + "/" + op + "/" + std::to_string(i) + ".txt", std::ios::binary);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
    };

    for (std::size_t i = 0; i < files.size(); i++) {
        const BenchFile &f = files[i];
        try {
            write("json_to_toon", i, ctoon::json_to_toon(f.json).str());
        } catch (const ctoon::error &) {}

        if (!f.toon.empty()) {
            try {
                write("toon_to_json", i, ctoon::toon_to_json(f.toon).str());
            } catch (const ctoon::error &) {}
        }

        try {
            auto toon = ctoon::json_to_toon(f.json);
            write("roundtrip", i, ctoon::toon_to_json(toon.view()).str());
        } catch (const ctoon::error &) {}

        // struct_to_toon: serialise the pre-built structure.
        if (f.has_doc) {
            try { write("struct_to_toon", i, f.doc.to_string().str()); } catch (const ctoon::error &) {}
        }

        // toon_to_struct: the decoded structure serialised to JSON (untimed),
        // only so the verifier can compare it to the original document.
        if (!f.toon.empty()) {
            try {
                auto doc = ctoon::document::parse(f.toon);
                write("toon_to_struct", i, doc.to_json(2).str());
            } catch (const ctoon::error &) {}
        }
    }
}

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

    std::printf("CToon Benchmarks - C++\n");
    std::printf("Corpus: %zu files, %.2f MB (JSON)\n\n",
                 files.size(), static_cast<double>(total_json_bytes) / 1e6);
    std::printf("%-8s %-14s %12s %14s %9s %10s %12s\n",
                 "Library", "Operation", "Throughput", "Docs/sec", "Success", "", "Total time");

    std::size_t total_toon_bytes = 0;
    for (auto &f : files) {
        try {
            f.doc = ctoon::document::from_json(f.json);
            f.has_doc = true;
            f.toon = f.doc.to_string().str();
            total_toon_bytes += f.toon.size();
        } catch (const ctoon::error &) {
        }
    }

    dump_outputs(files);

    long ops_a = 0; double bytes_a = 0;
    double t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (const auto &f : files) {
            try {
                auto result = ctoon::json_to_toon(f.json);
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
                auto result = ctoon::toon_to_json(f.toon);
                (void)result.size();
                ops_b++;
                bytes_b += static_cast<double>(f.toon.size());
            } catch (const ctoon::error &) {
            }
        }
    }
    record("ctoon", "toon_to_json", bytes_b, ops_b, now_seconds() - t0,
           static_cast<long>(files.size()) * CTOON_BENCH_REPEATS);

    // toon_to_struct: parse the pre-pass TOON into ctoon's in-memory
    // structure and drop it -- no JSON text in the timed region.
    long ops_s = 0; double bytes_s = 0;
    t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (const auto &f : files) {
            if (f.toon.empty()) continue;
            try {
                auto doc = ctoon::document::parse(f.toon);
                (void)doc.raw();
                ops_s++;
                bytes_s += static_cast<double>(f.toon.size());
            } catch (const ctoon::error &) {
            }
        }
    }
    record("ctoon", "toon_to_struct", bytes_s, ops_s, now_seconds() - t0,
           static_cast<long>(files.size()) * CTOON_BENCH_REPEATS);

    // struct_to_toon: serialise the structure built (untimed) in the
    // pre-pass -- no parsing in the timed region. Weighted by the JSON size,
    // same convention as json_to_toon.
    long ops_w = 0; double bytes_w = 0;
    t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (const auto &f : files) {
            if (!f.has_doc) continue;
            try {
                auto result = f.doc.to_string();
                (void)result.size();
                ops_w++;
                bytes_w += static_cast<double>(f.json.size());
            } catch (const ctoon::error &) {
            }
        }
    }
    record("ctoon", "struct_to_toon", bytes_w, ops_w, now_seconds() - t0,
           static_cast<long>(files.size()) * CTOON_BENCH_REPEATS);

    // roundtrip: ctoon::json_to_toon then ctoon::toon_to_json on its output,
    // chained as one operation per file.
    long ops_c = 0; double bytes_c = 0;
    t0 = now_seconds();
    for (int rep = 0; rep < CTOON_BENCH_REPEATS; rep++) {
        for (const auto &f : files) {
            try {
                auto toon = ctoon::json_to_toon(f.json);
                auto result = ctoon::toon_to_json(toon.view());
                (void)result.size();
                ops_c++;
                bytes_c += static_cast<double>(f.json.size());
            } catch (const ctoon::error &) {
            }
        }
    }
    record("ctoon", "roundtrip", bytes_c, ops_c, now_seconds() - t0,
           static_cast<long>(files.size()) * CTOON_BENCH_REPEATS);

    run_order_check();

    write_results_json(files.size(), total_json_bytes);
    return 0;
}
