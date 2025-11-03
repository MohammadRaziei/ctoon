#include "ctoon.h"
#include <iostream>

int main() {
    std::cout << "Testing Serialization Formats" << std::endl;
    std::cout << "=============================" << std::endl;

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

    std::cout << "-------------------" << std::endl;
    std::string jsonStr = ctoon::dumpsJson(value);
    std::cout << "JSON string: " << jsonStr << std::endl;
    std::cout << "-------------------" << std::endl;
    std::string toonStr = ctoon::dumpsToon(value);
    std::cout << "TOON string:" << std::endl;
    std::cout << toonStr << std::endl;

    try {
        // Save to JSON file
        ctoon::dumpJson(value, "test_output.json");
        std::cout << "Saved to test_output.json" << std::endl;

        // Save to TOON file
        ctoon::dumpToon(value, "test_output.toon");
        std::cout << "Saved to test_output.toon" << std::endl;

        // Load from JSON file
        ctoon::Value loadedJson = ctoon::loadJson("test_output.json");
        std::cout << "Loaded from JSON file" << std::endl;

        // Load from TOON file
        ctoon::Value loadedToon = ctoon::loadToon("test_output.toon");
        std::cout << "Loaded from TOON file" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
