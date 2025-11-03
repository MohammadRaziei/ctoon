#include "ctoon.h"
#include <iostream>

int main() {
    std::cout << "Testing File I/O Operations" << std::endl;
    std::cout << "===========================" << std::endl;

    // Create test data
    ctoon::Object data;
    data["name"] = ctoon::Value("Test User");
    data["age"] = ctoon::Value(30.0);
    data["active"] = ctoon::Value(true);

    ctoon::Array tags;
    tags.push_back(ctoon::Value("programming"));
    tags.push_back(ctoon::Value("c++"));
    tags.push_back(ctoon::Value("serialization"));
    data["tags"] = ctoon::Value(tags);

    ctoon::Value value(data);

    try {
        // Save to TOON file
        ctoon::dumpToon(value, "sample_output.toon");
        std::cout << "Saved data to sample_output.toon" << std::endl;

        // Load from TOON file
        ctoon::Value loaded = ctoon::loadToon("sample_output.toon");
        std::cout << "Loaded data from sample_output.toon" << std::endl;
        std::cout << "Loaded data:" << std::endl;
        std::cout << ctoon::dumpsToon(loaded) << std::endl;

        // Test JSON file operations
        ctoon::dumpJson(value, "sample_output.json");
        std::cout << "\nSaved data to sample_output.json" << std::endl;

        ctoon::Value jsonLoaded = ctoon::loadJson("sample_output.json");
        std::cout << "Loaded data from sample_output.json" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
