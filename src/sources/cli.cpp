#include "ctoon.h"
#include "CLI11.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace fs = std::filesystem;

namespace {
const std::string ctoonVersion = MACRO_STRINGIFY(CTOON_VERSION);



std::string availableFormats() {
    return "json, toon";
}

void printHelp(const CLI::App &app) {
    std::cout << app.help() << std::endl;
}

} // namespace

int main(int argc, char **argv) {
    CLI::App app{"Ctoon - A modern C++ serialization library and CLI tool\n"
                 "Version: " + ctoonVersion + "\n"
                 "\n"
                 "Ctoon provides fast and flexible serialization between JSON and Toon formats. "
                 "It can convert between different serialization formats and manipulate structured data.\n"
                 "\n"
                 "Examples:\n"
                 "$  ctoon input.json -o output.toon          # Convert JSON to TOON\n"
                 "$  ctoon input.toon -t json                 # Convert TOON to JSON (stdout)\n"
                 "$  ctoon input.toon -o output.json -i 4     # Convert Toon to JSON with 4-space indent"};

    std::string inputPath;
    std::string outputPath;
    std::string outputType;
    int indent = 2;
    bool showVersion = false;

    app.set_help_flag("-h,--help", "Show this help message and exit");
    app.add_option("input", inputPath, "Path to the input document (required)");
    app.add_option("-o,--output", outputPath, "Path to the output document (if omitted, prints to stdout)");
    app.add_option("-t,--type", outputType, "Output format: " + availableFormats() + " (default: toon)");
    app.add_option("-i,--indent", indent, "Indent level for structured output (default: 2)");
    app.add_flag("--version", showVersion, "Show version information and exit");

    if (argc == 1) {
        printHelp(app);
        return 0;
    }

    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp &help) {
        printHelp(app);
        return 0;
    } catch (const CLI::ParseError &error) {
        std::cerr << error.what() << std::endl;
        std::cerr << "Use --help or -h for more information" << std::endl;
        return 1;
    }

    if (showVersion) {
        std::cout << "ctoon " << ctoonVersion << std::endl;
        return 0;
    }

    if (inputPath.empty()) {
        printHelp(app);
        return 0;
    }

    if (indent < 0) {
        std::cerr << "Indent level must be non-negative" << std::endl;
        return 1;
    }

    if (!fs::exists(inputPath)) {
        std::cerr << "Input file not found: " << inputPath << std::endl;
        return 1;
    }

    try {
        // Load the file using auto-detection
        ctoon::Value value = ctoon::load(inputPath);

        if (!outputPath.empty()) {
            // Dump to file using auto-detection
            ctoon::dump(value, outputPath);
        } else {
            // Determine output format for stdout
            ctoon::Type type = ctoon::Type::TOON; // default
            if (!outputType.empty()) {
                ctoon::Type requested = ctoon::stringToType(outputType);
                if (requested == ctoon::Type::UNKNOWN) {
                    std::cerr << "Unknown output type: " << outputType << std::endl;
                    std::cerr << "Supported formats: " << availableFormats() << std::endl;
                    return 1;
                }
                type = requested;
            }
            // Dump to stdout with specified format and indent
            std::cout << ctoon::dumps(value, type, indent) << std::endl;
        }
    } catch (const std::exception &error) {
        std::cerr << "Failed to process: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
