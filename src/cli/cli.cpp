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
 * TOON → JSON  (ctoon_val* → yyjson_mut_val*, recursive)
 * ============================================================ */

static yyjson_mut_val *toon_val_to_json(yyjson_mut_doc *jdoc, ctoon_val *val) {
    if (!val)                return yyjson_mut_null(jdoc);
    if (ctoon_is_null(val))  return yyjson_mut_null(jdoc);
    if (ctoon_is_true(val))  return yyjson_mut_true(jdoc);
    if (ctoon_is_false(val)) return yyjson_mut_false(jdoc);

    if (ctoon_is_num(val)) {
        switch (ctoon_get_subtype(val)) {
            case CTOON_SUBTYPE_UINT: return yyjson_mut_uint(jdoc, ctoon_get_uint(val));
            case CTOON_SUBTYPE_SINT: return yyjson_mut_sint(jdoc, ctoon_get_sint(val));
            default:                 return yyjson_mut_real(jdoc, ctoon_get_real(val));
        }
    }

    if (ctoon_is_str(val)) {
        const char *s = ctoon_get_str(val);
        return yyjson_mut_strncpy(jdoc, s ? s : "", ctoon_get_len(val));
    }

    if (ctoon_is_arr(val)) {
        yyjson_mut_val *jarr = yyjson_mut_arr(jdoc);
        size_t sz = ctoon_arr_size(val);
        for (size_t i = 0; i < sz; i++)
            yyjson_mut_arr_append(jarr, toon_val_to_json(jdoc, ctoon_arr_get(val, i)));
        return jarr;
    }

    if (ctoon_is_obj(val)) {
        yyjson_mut_val *jobj = yyjson_mut_obj(jdoc);
        size_t sz = ctoon_obj_size(val);
        for (size_t i = 0; i < sz; i++) {
            ctoon_val *kv = ctoon_obj_get_key_at(val, i);
            ctoon_val *vv = ctoon_obj_get_val_at(val, i);
            const char *ks = ctoon_get_str(kv);
            yyjson_mut_val *jk = yyjson_mut_strncpy(jdoc, ks ? ks : "", ctoon_get_len(kv));
            yyjson_mut_obj_add(jobj, jk, toon_val_to_json(jdoc, vv));
        }
        return jobj;
    }

    return yyjson_mut_null(jdoc);
}

std::string toonToJson(const std::string &input, int indent, bool strict) {
    ctoon_read_options opts = {};
    opts.flag   = strict ? CTOON_READ_NOFLAG : CTOON_READ_NO_STRICT;
    opts.indent = indent;

    ctoon_err err = {};
    ctoon_doc *doc = ctoon_decode_opts(input.c_str(), input.size(), &opts, &err);
    if (!doc)
        throw std::runtime_error(std::string("TOON parse error: ") +
                                  (err.msg ? err.msg : "unknown"));

    yyjson_mut_doc *jdoc = yyjson_mut_doc_new(nullptr);
    if (!jdoc) { ctoon_doc_free(doc); throw std::runtime_error("yyjson alloc failed"); }

    yyjson_mut_doc_set_root(jdoc, toon_val_to_json(jdoc, ctoon_doc_get_root(doc)));
    ctoon_doc_free(doc);

    yyjson_write_flag flags = (indent > 0) ? YYJSON_WRITE_PRETTY : YYJSON_WRITE_NOFLAG;
    yyjson_write_err werr;
    char *raw = yyjson_mut_write_opts(jdoc, flags, nullptr, nullptr, &werr);
    yyjson_mut_doc_free(jdoc);

    if (!raw)
        throw std::runtime_error(std::string("JSON write error: ") + werr.msg);

    std::string result(raw);
    free(raw);
    return result;
}

/* ============================================================
 * JSON → TOON  (yyjson_val* → ctoon_val*, recursive)
 * ============================================================ */

static ctoon_val *json_val_to_toon(ctoon_doc *doc, yyjson_val *val);

static ctoon_val *json_arr_to_toon(ctoon_doc *doc, yyjson_val *arr) {
    ctoon_val *out = ctoon_new_arr(doc);
    if (!out) throw std::runtime_error("alloc failed");
    yyjson_arr_iter it;
    yyjson_arr_iter_init(arr, &it);
    yyjson_val *item;
    while ((item = yyjson_arr_iter_next(&it))) {
        if (!ctoon_arr_append(doc, out, json_val_to_toon(doc, item)))
            throw std::runtime_error("array append failed");
    }
    return out;
}

static ctoon_val *json_obj_to_toon(ctoon_doc *doc, yyjson_val *obj) {
    ctoon_val *out = ctoon_new_obj(doc);
    if (!out) throw std::runtime_error("alloc failed");
    yyjson_obj_iter it;
    yyjson_obj_iter_init(obj, &it);
    yyjson_val *key;
    while ((key = yyjson_obj_iter_next(&it))) {
        yyjson_val *val = yyjson_obj_iter_get_val(key);
        if (!ctoon_obj_set(doc, out, yyjson_get_str(key), json_val_to_toon(doc, val)))
            throw std::runtime_error("object set failed");
    }
    return out;
}

static ctoon_val *json_val_to_toon(ctoon_doc *doc, yyjson_val *val) {
    if (!val) return ctoon_new_null(doc);
    switch (yyjson_get_type(val)) {
        case YYJSON_TYPE_NULL: return ctoon_new_null(doc);
        case YYJSON_TYPE_BOOL: return ctoon_new_bool(doc, yyjson_get_bool(val));
        case YYJSON_TYPE_NUM:
            switch (yyjson_get_subtype(val)) {
                case YYJSON_SUBTYPE_UINT: return ctoon_new_uint(doc, yyjson_get_uint(val));
                case YYJSON_SUBTYPE_SINT: return ctoon_new_sint(doc, yyjson_get_sint(val));
                default:                  return ctoon_new_real(doc, yyjson_get_real(val));
            }
        case YYJSON_TYPE_STR: {
            const char *s = yyjson_get_str(val);
            return ctoon_new_strn(doc, s ? s : "", yyjson_get_len(val));
        }
        case YYJSON_TYPE_ARR: return json_arr_to_toon(doc, val);
        case YYJSON_TYPE_OBJ: return json_obj_to_toon(doc, val);
        default:              return ctoon_new_null(doc);
    }
}

std::string jsonToToon(const std::string &input, ctoon_write_options enc_opts) {
    yyjson_read_err rerr;
    yyjson_doc *jdoc = yyjson_read_opts(
        const_cast<char *>(input.data()), input.size(),
        YYJSON_READ_NOFLAG, nullptr, &rerr);
    if (!jdoc)
        throw std::runtime_error(std::string("JSON parse error: ") + rerr.msg
                                  + " at pos " + std::to_string(rerr.pos));

    ctoon_doc *doc = ctoon_doc_new(0);
    if (!doc) { yyjson_doc_free(jdoc); throw std::bad_alloc(); }

    ctoon_doc_set_root(doc, json_val_to_toon(doc, yyjson_doc_get_root(jdoc)));
    yyjson_doc_free(jdoc);

    ctoon_err eerr = {};
    size_t len = 0;
    char *raw = ctoon_encode_opts(doc, &enc_opts, &len, &eerr);
    ctoon_doc_free(doc);

    if (!raw)
        throw std::runtime_error(std::string("TOON encode error: ") +
                                  (eerr.msg ? eerr.msg : "unknown"));

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
        if (ext == ".json")  return Op::ENCODE;
        if (ext == ".toon")  return Op::DECODE;
        /* unknown extension: try content sniffing */
    }

    /* Content sniffing: valid JSON starts with { or [ or digit or " */
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
 * Stats output  (char-based; no tiktoken dependency)
 * ============================================================ */

void printStats(const std::string &input, const std::string &output, Op op) {
    size_t in_sz  = input.size();
    size_t out_sz = output.size();
    double ratio  = in_sz ? (100.0 * out_sz / in_sz) : 0.0;
    double saving = 100.0 - ratio;

    if (op == Op::ENCODE) {
        std::cerr << "\nBytes: " << in_sz << " (JSON) \xe2\x86\x92 "
                  << out_sz << " (TOON)\n";
        if (saving > 0)
            std::cerr << "Saved " << (in_sz - out_sz) << " bytes ("
                      << static_cast<int>(saving) << "% smaller)\n";
        else
            std::cerr << "TOON is " << static_cast<int>(-saving)
                      << "% larger than JSON\n";
    } else {
        std::cerr << "\nBytes: " << in_sz << " (TOON) \xe2\x86\x92 "
                  << out_sz << " (JSON)\n";
    }
}

/* Resolve "--delimiter" CLI string to ctoon_delimiter enum */
ctoon_delimiter parseDelimiter(const std::string &s) {
    if (s == "\t" || s == "tab") return CTOON_DELIMITER_TAB;
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
    int  indent      = 2;
    bool forceEncode = false;
    bool forceDecode = false;
    bool noStrict    = false;
    bool lengthMarker= false;
    bool statsFlag   = false;
    bool showVersion = false;

    app.add_option ("input",         inputPath,  "Input file path (omit or - for stdin)")
       ->expected(0, 1);
    app.add_option ("-o,--output",   outputPath, "Output file path (omit for stdout)");
    app.add_flag   ("-e,--encode",   forceEncode,"Force encode mode (JSON \xe2\x86\x92 TOON)");
    app.add_flag   ("-d,--decode",   forceDecode,"Force decode mode (TOON \xe2\x86\x92 JSON)");
    app.add_option ("--delimiter",   delimStr,
                    "Array delimiter: , (comma, default), \\t (tab), | (pipe)")
       ->check([](const std::string &s) -> std::string {
           if (s == "," || s == "\t" || s == "tab" || s == "|" || s == "pipe")
               return {};
           return "Delimiter must be one of: , \\t tab | pipe";
       });
    app.add_option ("-i,--indent",   indent,
                    "Indentation size in spaces (default: 2, 0 = compact)")
       ->check(CLI::NonNegativeNumber);
    app.add_flag   ("--length-marker", lengthMarker,
                    "Prefix array lengths with '#': items[#3]: ...");
    app.add_flag   ("--no-strict",   noStrict,   "Disable strict validation when decoding");
    app.add_flag   ("--stats",       statsFlag,  "Print byte statistics to stderr");
    app.add_flag   ("--version",     showVersion,"Show version and exit");

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
    catch (const CLI::CallForHelp &)  { std::cout << app.help(); return 0; }
    catch (const CLI::ParseError &e)  {
        std::cerr << "Error: " << e.what() << "\nUse --help for usage.\n";
        return 1;
    }

    if (showVersion) { std::cout << "ctoon " << version << "\n"; return 0; }

    if (forceEncode && forceDecode) {
        std::cerr << "Error: --encode and --decode are mutually exclusive.\n";
        return 1;
    }

    /* Read input */
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

    /* Resolve operation */
    Op op = detectOp(inputPath, input, forceEncode, forceDecode);

    /* Build encode options */
    ctoon_write_options enc_opts = {};
    enc_opts.flag      = lengthMarker ? CTOON_WRITE_LENGTH_MARKER : CTOON_WRITE_NOFLAG;
    enc_opts.delimiter = parseDelimiter(delimStr);
    enc_opts.indent    = indent > 0 ? indent : 2;

    /* Convert */
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
