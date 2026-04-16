#include "ctoon.h"
#include "CLI11.hpp"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <cstring>

namespace fs = std::filesystem;

namespace {
const std::string ctoonVersion = CTOON_VERSION_STRING;

// Helper to read entire file
std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Helper to write file
void writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot write to file: " + path);
    }
    file << content;
}

// TODO: Implement proper JSON to TOON conversion
// Currently, the ctoon library only has functions to READ TOON, not CREATE TOON from JSON
// For now, we'll implement a minimal CLI that can only decode TOON to JSON
// To add JSON→TOON support, we need to:
// 1. Add creation functions to ctoon library (ctoon_new_*, ctoon_array_append, ctoon_object_set)
// 2. Or implement JSON→TOON conversion directly

// TODO: Fix object iteration in ctoon library
// The ctoon_obj_iter_* functions have bugs:
// - ctoon_obj_iter_key returns (const char*)key which is wrong (should return string)
// - Object keys are not properly accessible
// This prevents proper JSON output for objects

// Simple TOON to JSON conversion (basic implementation)
std::string ctoon_to_json_string(ctoon_val* val, int indent = 0) {
    if (!val) return "null";
    
    if (ctoon_is_null(val)) {
        return "null";
    } else if (ctoon_is_true(val)) {
        return "true";
    } else if (ctoon_is_false(val)) {
        return "false";
    } else if (ctoon_is_num(val)) {
        double num = ctoon_get_real(val);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.15g", num);
        return buf;
    } else if (ctoon_is_str(val)) {
        const char* str = ctoon_get_str(val);
        // Simple JSON string escaping
        std::string result = "\"";
        if (str) {
            for (const char* p = str; *p; p++) {
                if (*p == '"' || *p == '\\') {
                    result += '\\';
                }
                result += *p;
            }
        }
        result += "\"";
        return result;
    } else if (ctoon_is_arr(val)) {
        std::string result = "[";
        size_t size = ctoon_arr_size(val);
        for (size_t i = 0; i < size; i++) {
            if (i > 0) result += ",";
            ctoon_val* item = ctoon_arr_get(val, i);
            result += ctoon_to_json_string(item, indent + 2);
        }
        result += "]";
        return result;
    } else if (ctoon_is_obj(val)) {
        // NOTE: Object iteration is broken in ctoon library
        // The ctoon_obj_iter_* functions don't work properly
        // For now, return empty object
        return "{}";
    }
    
    return "null";
}

// Detect operation based on file extension
enum class Operation { ENCODE, DECODE, AUTO };

Operation detectOperation(const std::string& inputPath, bool encodeFlag, bool decodeFlag) {
    if (encodeFlag) return Operation::ENCODE;
    if (decodeFlag) return Operation::DECODE;
    
    if (inputPath.empty() || inputPath == "-") {
        // Default to decode for stdin (since encode is not implemented)
        return Operation::DECODE;
    }
    
    fs::path path(inputPath);
    std::string ext = path.extension().string();
    
    if (ext == ".json") {
        return Operation::ENCODE;
    } else if (ext == ".toon") {
        return Operation::DECODE;
    }
    
    // Default to decode (since encode is not implemented)
    return Operation::DECODE;
}

void printHelpMessage(const CLI::App &app) {
    std::cout << app.help() << std::endl;
    std::cout << "\nExamples:\n";
    std::cout << "  ctoon input.toon -o output.json          # Convert TOON to JSON (DECODE)\n";
    std::cout << "  ctoon input.toon                         # Convert TOON to JSON (stdout)\n";
    std::cout << "  cat data.toon | ctoon                    # Convert TOON from stdin\n";
    std::cout << "  ctoon --version                          # Show version\n";
    std::cout << "\nNOTE: JSON to TOON conversion (ENCODE) is not yet implemented.\n";
    std::cout << "      The ctoon library needs creation functions added first.\n";
}

} // namespace

int main(int argc, char **argv) {
    CLI::App app{"Ctoon - Command Line Interface for TOON format\n"
                 "Version: " + ctoonVersion + "\n"
                 "\n"
                 "Convert TOON to JSON format. TOON is a compact format\n"
                 "designed for efficient token usage with LLMs.\n"
                 "\n"
                 "NOTE: Currently only supports TOON → JSON (decode).\n"
                 "      JSON → TOON (encode) is not yet implemented."};
    
    std::string inputPath;
    std::string outputPath;
    int indent = 2;
    bool showVersion = false;
    bool encodeFlag = false;
    bool decodeFlag = false;
    bool statsFlag = false;
    
    app.set_help_flag("-h,--help", "Show this help message and exit");
    app.add_option("input", inputPath, "Input file path (use - or omit for stdin)")
        ->expected(0, 1);
    app.add_option("-o,--output", outputPath, "Output file path (omit for stdout)");
    app.add_option("-i,--indent", indent, "Indentation for JSON output (default: 2)")
        ->check(CLI::NonNegativeNumber);
    app.add_flag("-e,--encode", encodeFlag, "Force encode mode (JSON to TOON) - NOT IMPLEMENTED");
    app.add_flag("-d,--decode", decodeFlag, "Force decode mode (TOON to JSON)");
    app.add_flag("--stats", statsFlag, "Show token statistics (encode only) - NOT IMPLEMENTED");
    app.add_flag("--version", showVersion, "Show version information and exit");
    
    if (argc == 1) {
        printHelpMessage(app);
        return 0;
    }
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp &help) {
        printHelpMessage(app);
        return 0;
    } catch (const CLI::ParseError &error) {
        std::cerr << "Error: " << error.what() << std::endl;
        std::cerr << "Use --help or -h for more information" << std::endl;
        return 1;
    }
    
    if (showVersion) {
        std::cout << "ctoon " << ctoonVersion << std::endl;
        return 0;
    }
    
    if (encodeFlag) {
        std::cerr << "Error: JSON to TOON conversion (encode) is not yet implemented.\n";
        std::cerr << "       The ctoon library needs creation functions added first.\n";
        return 1;
    }
    
    if (encodeFlag && decodeFlag) {
        std::cerr << "Error: Cannot specify both --encode and --decode" << std::endl;
        return 1;
    }
    
    if (indent < 0) {
        std::cerr << "Error: Indent level must be non-negative" << std::endl;
        return 1;
    }
    
    if (statsFlag) {
        std::cerr << "Warning: --stats flag is not yet implemented\n";
    }
    
    // Read input
    std::string inputData;
    if (inputPath.empty() || inputPath == "-") {
        // Read from stdin
        std::stringstream buffer;
        buffer << std::cin.rdbuf();
        inputData = buffer.str();
    } else {
        // Read from file
        if (!fs::exists(inputPath)) {
            std::cerr << "Error: Input file not found: " << inputPath << std::endl;
            return 1;
        }
        inputData = readFile(inputPath);
    }
    
    if (inputData.empty()) {
        std::cerr << "Error: Input is empty" << std::endl;
        return 1;
    }
    
    // Detect operation
    Operation op = detectOperation(inputPath, encodeFlag, decodeFlag);
    
    try {
        if (op == Operation::ENCODE) {
            // JSON to TOON - NOT IMPLEMENTED
            std::cerr << "Error: JSON to TOON conversion (encode) is not yet implemented.\n";
            std::cerr << "       The ctoon library needs creation functions added first.\n";
            return 1;
            
        } else {
            // TOON to JSON
            ctoon_doc* toon_doc = ctoon_read_toon(inputData.c_str(), inputData.size());
            if (!toon_doc) {
                std::cerr << "Error: Failed to parse TOON input" << std::endl;
                return 1;
            }
            
            ctoon_val* toon_root = ctoon_doc_get_root(toon_doc);
            if (!toon_root) {
                ctoon_doc_free(toon_doc);
                std::cerr << "Error: TOON document is empty" << std::endl;
                return 1;
            }
            
            // Convert TOON to JSON string
            std::string json_output = ctoon_to_json_string(toon_root, indent);
            ctoon_doc_free(toon_doc);
            
            // Output result
            if (!outputPath.empty()) {
                writeFile(outputPath, json_output);
                std::cout << "Decoded " << (inputPath.empty() || inputPath == "-" ? "stdin" : inputPath)
                          << " → " << outputPath << std::endl;
            } else {
                std::cout << json_output;
                if (json_output.back() != '\n') {
                    std::cout << std::endl;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}