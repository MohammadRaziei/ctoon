/*==============================================================================
 |  CToon CLI – Convert between TOON and JSON formats
 |  Copyright (c) 2026 CToon Project, MIT License
 *============================================================================*/

#include "ctoon.h"
#include "CLI11.hpp"
#include "json/yyjson.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

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
 * TOON → JSON  (ctoon_mut_val* → yyjson_mut_val*)
 * ============================================================ */

static yyjson_mut_val *toon_val_to_json(yyjson_mut_doc *jdoc,
                                         ctoon_mut_val  *val) {
    if (!val || ctoon_mut_is_null(val))  return yyjson_mut_null(jdoc);
    if (ctoon_mut_is_true(val))          return yyjson_mut_true(jdoc);
    if (ctoon_mut_is_false(val))         return yyjson_mut_false(jdoc);

    if (ctoon_mut_is_num(val)) {
        ctoon_subtype sub = ctoon_mut_get_subtype(val);
        if (sub == CTOON_SUBTYPE_UINT) return yyjson_mut_uint(jdoc, ctoon_mut_get_uint(val));
        if (sub == CTOON_SUBTYPE_SINT) return yyjson_mut_sint(jdoc, ctoon_mut_get_sint(val));
        return yyjson_mut_real(jdoc, ctoon_mut_get_real(val));
    }

    if (ctoon_mut_is_str(val))
        return yyjson_mut_strncpy(jdoc, ctoon_mut_get_str(val),
                                   ctoon_mut_get_len(val));

    if (ctoon_mut_is_arr(val)) {
        yyjson_mut_val *jarr = yyjson_mut_arr(jdoc);
        ctoon_mut_arr_iter it;
        ctoon_mut_arr_iter_init(val, &it);
        ctoon_mut_val *item;
        while ((item = ctoon_mut_arr_iter_next(&it)))
            yyjson_mut_arr_append(jarr, toon_val_to_json(jdoc, item));
        return jarr;
    }

    if (ctoon_mut_is_obj(val)) {
        yyjson_mut_val *jobj = yyjson_mut_obj(jdoc);
        ctoon_mut_obj_iter it;
        ctoon_mut_obj_iter_init(val, &it);
        ctoon_mut_val *key;
        while ((key = ctoon_mut_obj_iter_next(&it))) {
            ctoon_mut_val  *v  = ctoon_mut_obj_iter_get_val(key);
            yyjson_mut_val *jk = yyjson_mut_strncpy(jdoc,
                                                      ctoon_mut_get_str(key),
                                                      ctoon_mut_get_len(key));
            yyjson_mut_obj_add(jobj, jk, toon_val_to_json(jdoc, v));
        }
        return jobj;
    }

    return yyjson_mut_null(jdoc);
}

std::string toonToJson(const std::string &input, int indent, bool strict) {
    ctoon_read_flag flg  = strict ? CTOON_READ_NOFLAG : CTOON_READ_ALLOW_INF_AND_NAN;
    ctoon_read_err  rerr = {};
    ctoon_doc *idoc = ctoon_read_opts(const_cast<char *>(input.data()),
                                       input.size(), flg, nullptr, &rerr);
    if (!idoc)
        throw std::runtime_error(std::string("TOON parse error at pos ")
                                  + std::to_string(rerr.pos) + ": "
                                  + (rerr.msg ? rerr.msg : "unknown"));

    /* Convert immutable doc to mutable so both sides use the same *_mut_ API */
    ctoon_mut_doc *tdoc = ctoon_doc_mut_copy(idoc, nullptr);
    ctoon_doc_free(idoc);
    if (!tdoc) throw std::runtime_error("ctoon_doc_mut_copy failed");

    yyjson_mut_doc *jdoc = yyjson_mut_doc_new(nullptr);
    if (!jdoc) { ctoon_mut_doc_free(tdoc); throw std::runtime_error("yyjson alloc failed"); }

    yyjson_mut_doc_set_root(jdoc, toon_val_to_json(jdoc,
                                       ctoon_mut_doc_get_root(tdoc)));
    ctoon_mut_doc_free(tdoc);

    yyjson_write_flag wflg = (indent > 0) ? YYJSON_WRITE_PRETTY : YYJSON_WRITE_NOFLAG;
    yyjson_write_err  werr  = {};
    char *raw = yyjson_mut_write_opts(jdoc, wflg, nullptr, nullptr, &werr);
    yyjson_mut_doc_free(jdoc);
    if (!raw) throw std::runtime_error(std::string("JSON write error: ") + werr.msg);

    std::string result(raw);
    free(raw);
    return result;
}

/* ============================================================
 * JSON → TOON  (yyjson_mut_val* → ctoon_mut_val*)
 * ============================================================ */

static ctoon_mut_val *json_val_to_toon(ctoon_mut_doc  *tdoc,
                                        yyjson_mut_val *val) {
    if (!val || yyjson_mut_is_null(val))  return ctoon_mut_null(tdoc);
    if (yyjson_mut_is_true(val))          return ctoon_mut_true(tdoc);
    if (yyjson_mut_is_false(val))         return ctoon_mut_false(tdoc);

    if (yyjson_mut_is_num(val)) {
        yyjson_subtype sub = yyjson_mut_get_subtype(val);
        if (sub == YYJSON_SUBTYPE_UINT) return ctoon_mut_uint(tdoc, yyjson_mut_get_uint(val));
        if (sub == YYJSON_SUBTYPE_SINT) return ctoon_mut_sint(tdoc, yyjson_mut_get_sint(val));
        return ctoon_mut_real(tdoc, yyjson_mut_get_real(val));
    }

    if (yyjson_mut_is_str(val))
        return ctoon_mut_strncpy(tdoc, yyjson_mut_get_str(val),
                                  yyjson_mut_get_len(val));

    if (yyjson_mut_is_arr(val)) {
        ctoon_mut_val *arr = ctoon_mut_arr(tdoc);
        yyjson_mut_arr_iter it;
        yyjson_mut_arr_iter_init(val, &it);
        yyjson_mut_val *item;
        while ((item = yyjson_mut_arr_iter_next(&it)))
            ctoon_mut_arr_append(arr, json_val_to_toon(tdoc, item));
        return arr;
    }

    if (yyjson_mut_is_obj(val)) {
        ctoon_mut_val *obj = ctoon_mut_obj(tdoc);
        yyjson_mut_obj_iter it;
        yyjson_mut_obj_iter_init(val, &it);
        yyjson_mut_val *key;
        while ((key = yyjson_mut_obj_iter_next(&it))) {
            yyjson_mut_val *v  = yyjson_mut_obj_iter_get_val(key);
            ctoon_mut_val  *ck = ctoon_mut_strncpy(tdoc,
                                                     yyjson_mut_get_str(key),
                                                     yyjson_mut_get_len(key));
            ctoon_mut_obj_add(obj, ck, json_val_to_toon(tdoc, v));
        }
        return obj;
    }

    return ctoon_mut_null(tdoc);
}

std::string jsonToToon(const std::string &input,
                       const ctoon_write_options &enc_opts) {
    yyjson_read_err rerr = {};
    yyjson_doc *idoc = yyjson_read_opts(
        const_cast<char *>(input.data()), input.size(),
        YYJSON_READ_NOFLAG, nullptr, &rerr);
    if (!idoc)
        throw std::runtime_error(std::string("JSON parse error at pos ")
                                  + std::to_string(rerr.pos) + ": " + rerr.msg);

    /* Convert immutable yyjson doc to mutable so both sides use *_mut_ API */
    yyjson_mut_doc *jdoc = yyjson_doc_mut_copy(idoc, nullptr);
    yyjson_doc_free(idoc);
    if (!jdoc) throw std::runtime_error("yyjson_doc_mut_copy failed");

    ctoon_mut_doc *tdoc = ctoon_mut_doc_new(nullptr);
    if (!tdoc) { yyjson_mut_doc_free(jdoc); throw std::bad_alloc(); }

    ctoon_mut_doc_set_root(tdoc, json_val_to_toon(tdoc,
                                       yyjson_mut_doc_get_root(jdoc)));
    yyjson_mut_doc_free(jdoc);

    ctoon_write_err werr = {};
    size_t len = 0;
    char  *raw = ctoon_mut_write_opts(tdoc, &enc_opts, nullptr, &len, &werr);
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

enum class Op { ENCODE, DECODE };

Op detectOp(const std::string &path, const std::string &content,
             bool forceEncode, bool forceDecode) {
    if (forceEncode) return Op::ENCODE;
    if (forceDecode) return Op::DECODE;

    if (!path.empty() && path != "-") {
        std::string ext = fs::path(path).extension().string();
        if (ext == ".json") return Op::ENCODE;
        if (ext == ".toon") return Op::DECODE;
    }

    /* Content sniffing: JSON starts with { [ " digit or - */
    for (char c : content) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        if (c == '{' || c == '[' || c == '"' ||
            (c >= '0' && c <= '9') || c == '-')
            return Op::ENCODE;
        break;
    }
    return Op::DECODE;
}

/* ============================================================
 * Stats
 * ============================================================ */

void printStats(const std::string &input, const std::string &output, Op op) {
    size_t in_sz  = input.size();
    size_t out_sz = output.size();
    double ratio  = in_sz ? (100.0 * out_sz / in_sz) : 0.0;
    double saving = 100.0 - ratio;

    if (op == Op::ENCODE) {
        std::cerr << "\nBytes: " << in_sz  << " (JSON) \xe2\x86\x92 "
                  << out_sz << " (TOON)\n";
        if (saving > 0)
            std::cerr << "Saved " << (in_sz - out_sz) << " bytes ("
                      << static_cast<int>(saving) << "% smaller)\n";
        else
            std::cerr << "TOON is " << static_cast<int>(-saving)
                      << "% larger than JSON\n";
    } else {
        std::cerr << "\nBytes: " << in_sz  << " (TOON) \xe2\x86\x92 "
                  << out_sz << " (JSON)\n";
    }
}

ctoon_delimiter parseDelimiter(const std::string &s) {
    if (s == "\t" || s == "tab")  return CTOON_DELIMITER_TAB;
    if (s == "|"  || s == "pipe") return CTOON_DELIMITER_PIPE;
    return CTOON_DELIMITER_COMMA;
}

} // namespace

/* ============================================================
 * main
 * ============================================================ */

int main(int argc, char **argv) {
    const std::string version = CTOON_VERSION_STRING;

    CLI::App app{
        "CToon - TOON \xe2\x86\x94 JSON converter\n"
        "Version: " + version + "\n\n"
        "TOON is a compact format optimised for LLM token usage.\n"
        "Auto-detects direction from extension (.toon=decode, .json=encode)\n"
        "or content when reading from stdin."
    };
    app.set_help_flag("-h,--help", "Print this help message and exit");

    std::string inputPath, outputPath, delimStr = ",";
    int  indent       = 2;
    bool forceEncode  = false;
    bool forceDecode  = false;
    bool noStrict     = false;
    bool lengthMarker = false;
    bool statsFlag    = false;
    bool showVersion  = false;

    app.add_option("input",           inputPath,   "Input file path (omit or - for stdin)")
       ->expected(0, 1);
    app.add_option("-o,--output",     outputPath,  "Output file path (omit for stdout)");
    app.add_flag  ("-e,--encode",     forceEncode, "Force encode mode (JSON \xe2\x86\x92 TOON)");
    app.add_flag  ("-d,--decode",     forceDecode, "Force decode mode (TOON \xe2\x86\x92 JSON)");
    app.add_option("--delimiter",     delimStr,
                   "Array delimiter: , (comma, default), \\t (tab), | (pipe)")
       ->check([](const std::string &s) -> std::string {
           if (s == "," || s == "\t" || s == "tab" || s == "|" || s == "pipe")
               return {};
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
                  << "  ctoon in.toon                    # TOON \xe2\x86\x92 JSON (stdout)\n"
                  << "  ctoon in.toon -o out.json        # TOON \xe2\x86\x92 JSON (file)\n"
                  << "  ctoon in.json -o out.toon        # JSON \xe2\x86\x92 TOON\n"
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
            if (!fs::exists(inputPath)) {
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

    Op op = detectOp(inputPath, input, forceEncode, forceDecode);

    ctoon_write_options enc_opts = {};
    enc_opts.flag      = lengthMarker ? CTOON_WRITE_LENGTH_MARKER : CTOON_WRITE_NOFLAG;
    enc_opts.delimiter = parseDelimiter(delimStr);
    enc_opts.indent    = indent > 0 ? indent : 2;

    /* ---- Convert ---- */
    try {
        std::string output;
        if (op == Op::ENCODE)
            output = jsonToToon(input, enc_opts);
        else
            output = toonToJson(input, indent, !noStrict);

        if (!outputPath.empty()) {
            writeFile(outputPath, output);
            std::string src = fromStdin ? "<stdin>" : inputPath;
            std::cout << (op == Op::ENCODE ? "JSON\xe2\x86\x92TOON" : "TOON\xe2\x86\x92JSON")
                      << ": " << src << " \xe2\x86\x92 " << outputPath << "\n";
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
