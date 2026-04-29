/*==============================================================================
 |  CToon CLI – Convert between TOON and JSON formats
 |  Copyright (c) 2026 CToon Project, MIT License
 *============================================================================*/

#include "ctoon.h"
#include "CLI11.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

namespace {

/* ============================================================
 * Portable filesystem helpers (C++11 compatible)
 * ============================================================ */

/**
 * @brief Get file extension (C++11 compatible).
 * @param path File path.
 * @return Extension in lowercase (e.g., ".json", ".toon"), or empty string.
 */
std::string getFileExtension(const std::string &path) {
    std::string::size_type dot = path.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot == path.size() - 1) {
        return std::string();
    }
    std::string ext = path.substr(dot + 1);
    /* Convert to lowercase */
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   static_cast<int (*)(int)>(std::tolower));
    return "." + ext;
}

/**
 * @brief Check if file exists (C++11 compatible).
 * @param path File path.
 * @return true if file exists.
 */
bool fileExists(const std::string &path) {
    std::ifstream f(path);
    return f.good();
}

/* ============================================================
 * File I/O
 * ============================================================ */

std::string readFile(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

void writeFile(const std::string &path, const std::string &content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot write file: " + path);
    f << content;
}

/* ============================================================
 * TOON → JSON  (ctoon_mut_doc_to_json)
 * ============================================================ */

std::string toonToJson(const std::string &input, int indent, bool strict) {
    ctoon_read_flag flg  = strict ? CTOON_READ_NOFLAG : CTOON_READ_ALLOW_INF_AND_NAN;
    ctoon_read_err  rerr;
    memset(&rerr, 0, sizeof(rerr));
    ctoon_doc *idoc = ctoon_read_opts(const_cast<char *>(input.data()),
                                       input.size(), flg, NULL, &rerr);
    if (!idoc)
        throw std::runtime_error(std::string("TOON parse error at pos ")
                                  + std::to_string(rerr.pos) + ": "
                                  + (rerr.msg ? rerr.msg : "unknown"));

    /* Convert immutable doc to mutable */
    ctoon_mut_doc *tdoc = ctoon_doc_mut_copy(idoc, NULL);
    ctoon_doc_free(idoc);
    if (!tdoc) throw std::runtime_error("ctoon_doc_mut_copy failed");

    /* Use ctoon_mut_doc_to_json directly — no yyjson needed */
    ctoon_write_flag jflag = CTOON_WRITE_NOFLAG;
    ctoon_write_err werr;
    memset(&werr, 0, sizeof(werr));
    size_t len = 0;
    char *raw = ctoon_mut_doc_to_json(tdoc, indent > 0 ? indent : 0,
                                      jflag, NULL, &len, &werr);
    ctoon_mut_doc_free(tdoc);
    if (!raw)
        throw std::runtime_error(std::string("JSON write error: ")
                                  + (werr.msg ? werr.msg : "unknown"));

    std::string result(raw, len);
    free(raw);
    return result;
}

/* ============================================================
 * JSON → TOON  (uses ctoon_read_json — no yyjson dependency)
 * ============================================================ */

std::string jsonToToon(const std::string &input,
                       const ctoon_write_options &enc_opts) {
    /* Parse JSON directly into a ctoon immutable doc via ctoon_read_json.
     * ctoon has a built-in JSON reader, so no yyjson is needed. */
    ctoon_read_err rerr;
    memset(&rerr, 0, sizeof(rerr));
    ctoon_doc *idoc = ctoon_read_json(
        const_cast<char *>(input.data()), input.size(),
        CTOON_READ_NOFLAG, NULL, &rerr);
    if (!idoc)
        throw std::runtime_error(std::string("JSON parse error at pos ")
                                  + std::to_string(rerr.pos) + ": "
                                  + (rerr.msg ? rerr.msg : "unknown"));

    /* Convert immutable → mutable so we can write as TOON */
    ctoon_mut_doc *tdoc = ctoon_doc_mut_copy(idoc, NULL);
    ctoon_doc_free(idoc);
    if (!tdoc) throw std::runtime_error("ctoon_doc_mut_copy failed");

    ctoon_write_err werr;
    memset(&werr, 0, sizeof(werr));
    size_t len = 0;
    char  *raw = ctoon_mut_write_opts(tdoc, &enc_opts, NULL, &len, &werr);
    ctoon_mut_doc_free(tdoc);
    if (!raw)
        throw std::runtime_error(std::string("TOON write error: ")
                                  + (werr.msg ? werr.msg : "unknown"));

    std::string result(raw, len);
    free(raw);
    return result;
}

/* ============================================================
 * Auto-detect operation from extension or content
 * ============================================================ */

enum Operation {
    OP_ENCODE,   /* JSON → TOON */
    OP_DECODE    /* TOON → JSON */
};

Operation detectOp(const std::string &path, const std::string &content,
                   bool forceEncode, bool forceDecode) {
    if (forceEncode) return OP_ENCODE;
    if (forceDecode) return OP_DECODE;

    if (!path.empty() && path != "-") {
        std::string ext = getFileExtension(path);
        if (ext == ".json") return OP_ENCODE;
        if (ext == ".toon") return OP_DECODE;
    }

    /* Content sniffing: JSON starts with { [ " digit or - */
    std::string::const_iterator it = content.begin();
    while (it != content.end()) {
        char c = *it;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++it;
            continue;
        }
        if (c == '{' || c == '[' || c == '"' ||
            (c >= '0' && c <= '9') || c == '-')
            return OP_ENCODE;
        break;
    }
    return OP_DECODE;
}

/* ============================================================
 * Stats
 * ============================================================ */

void printStats(const std::string &input, const std::string &output,
                Operation op) {
    size_t in_sz  = input.size();
    size_t out_sz = output.size();
    double ratio  = in_sz ? (100.0 * out_sz / in_sz) : 0.0;
    double saving = 100.0 - ratio;

    if (op == OP_ENCODE) {
        std::cerr << "\nBytes: " << in_sz  << " (JSON) -> "
                  << out_sz << " (TOON)\n";
        if (saving > 0)
            std::cerr << "Saved " << (in_sz - out_sz) << " bytes ("
                      << static_cast<int>(saving) << "% smaller)\n";
        else
            std::cerr << "TOON is " << static_cast<int>(-saving)
                      << "% larger than JSON\n";
    } else {
        std::cerr << "\nBytes: " << in_sz  << " (TOON) -> "
                  << out_sz << " (JSON)\n";
    }
}

ctoon_delimiter parseDelimiter(const std::string &s) {
    if (s == "\t" || s == "tab")  return CTOON_DELIMITER_TAB;
    if (s == "|"  || s == "pipe") return CTOON_DELIMITER_PIPE;
    return CTOON_DELIMITER_COMMA;
}

} /* namespace */

/* ============================================================
 * main
 * ============================================================ */

int main(int argc, char **argv) {
    const std::string version = CTOON_VERSION_STRING;

    CLI::App app(
        "CToon - TOON <-> JSON converter\n"
        "Version: " + version + "\n\n"
        "TOON is a compact format optimised for LLM token usage.\n"
        "Auto-detects direction from extension (.toon=decode, .json=encode)\n"
        "or content when reading from stdin."
    );
    app.set_help_flag("-h,--help", "Print this help message and exit");

    std::string inputPath, outputPath, delimStr = ",";
    int         indent       = 2;
    bool        forceEncode  = false;
    bool        forceDecode  = false;
    bool        noStrict     = false;
    bool        lengthMarker = false;
    bool        statsFlag    = false;
    bool        showVersion  = false;

    app.add_option("input",           inputPath,   "Input file path (omit or - for stdin)")
       ->expected(0, 1);
    app.add_option("-o,--output",     outputPath,  "Output file path (omit for stdout)");
    app.add_flag  ("-e,--encode",     forceEncode, "Force encode mode (JSON -> TOON)");
    app.add_flag  ("-d,--decode",     forceDecode, "Force decode mode (TOON -> JSON)");
    app.add_option("--delimiter",     delimStr,
                   "Array delimiter: , (comma, default), \\t (tab), | (pipe)")
       ->check([](const std::string &s) -> std::string {
           if (s == "," || s == "\t" || s == "tab" || s == "|" || s == "pipe")
               return std::string();
           return "Delimiter must be one of: ,  \\t  tab  |  pipe";
       });
    app.add_option("-i,--indent",     indent,
                   "Indentation in spaces (default: 2, 0 = compact)")
       ->check(CLI::NonNegativeNumber);
    app.add_flag  ("--length-marker", lengthMarker,
                   "Prefix array lengths with '#': items[#3]: ...");
    app.add_flag  ("--no-strict",     noStrict,    "Disable strict validation when decoding");
    app.add_flag  ("--stats",         statsFlag,   "Print byte statistics to stderr");
    app.add_flag  ("--version",       showVersion, "Show version and exit");

    if (argc == 1) {
        std::cout << app.help()
                  << "\nExamples:\n"
                  << "  ctoon in.toon                    # TOON -> JSON (stdout)\n"
                  << "  ctoon in.toon -o out.json        # TOON -> JSON (file)\n"
                  << "  ctoon in.json -o out.toon        # JSON -> TOON\n"
                  << "  ctoon in.json --delimiter '\\t'   # tab-delimited arrays\n"
                  << "  ctoon in.json --length-marker    # items[#3]: ...\n"
                  << "  ctoon in.json --stats            # show byte savings\n"
                  << "  cat data.toon | ctoon -d -       # stdin decode\n";
        return 0;
    }

    try { app.parse(argc, argv); }
    catch (const CLI::CallForHelp &) { std::cout << app.help(); return 0; }
    catch (const CLI::ParseError  &e) {
        std::cerr << "Error: " << e.what() << "\nUse --help for usage.\n";
        return 1;
    }

    if (showVersion) { std::cout << "ctoon " << version << "\n"; return 0; }

    if (forceEncode && forceDecode) {
        std::cerr << "Error: --encode and --decode are mutually exclusive.\n";
        return 1;
    }

    /* ---- Read input ---- */
    std::string input;
    bool fromStdin = inputPath.empty() || inputPath == "-";
    try {
        if (fromStdin) {
            std::ostringstream buf;
            buf << std::cin.rdbuf();
            input = buf.str();
        } else {
            if (!fileExists(inputPath)) {
                std::cerr << "Error: File not found: " << inputPath << "\n";
                return 1;
            }
            input = readFile(inputPath);
        }
    } catch (const std::exception &e) {
        std::cerr << "Error reading input: " << e.what() << "\n";
        return 1;
    }

    if (input.empty()) { std::cerr << "Error: Input is empty.\n"; return 1; }

    Operation op = detectOp(inputPath, input, forceEncode, forceDecode);

    ctoon_write_options enc_opts;
    memset(&enc_opts, 0, sizeof(enc_opts));
    enc_opts.flag      = lengthMarker ? CTOON_WRITE_LENGTH_MARKER : CTOON_WRITE_NOFLAG;
    enc_opts.delimiter = parseDelimiter(delimStr);
    enc_opts.indent    = indent > 0 ? indent : 2;

    /* ---- Convert ---- */
    try {
        std::string output;
        if (op == OP_ENCODE)
            output = jsonToToon(input, enc_opts);
        else
            output = toonToJson(input, indent, !noStrict);

        if (!outputPath.empty()) {
            writeFile(outputPath, output);
            std::string src = fromStdin ? "<stdin>" : inputPath;
            std::cout << (op == OP_ENCODE ? "JSON->TOON" : "TOON->JSON")
                      << ": " << src << " -> " << outputPath << "\n";
        } else {
            std::cout << output;
            if (output.empty() || output.back() != '\n') std::cout << '\n';
        }

        if (statsFlag) printStats(input, output, op);

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}