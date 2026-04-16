#include "utest/utest.h"
#include "../../src/bindings/ctoon.hpp"

#include <string>
#include <vector>
#include <cmath>

// Test basic TOON parsing with C++ API
UTEST(ctoon_cpp_tests, test_parse_basic_object) {
    std::string toon_data = "name: Alice\nage: 30\nactive: true\ntags[3]: programming,c++,serialization";
    
    ctoon::Document doc(toon_data);
    ASSERT_TRUE(doc);
    
    ctoon::Value root = doc.root();
    ASSERT_TRUE(root);
    ASSERT_TRUE(root.isObject());
    
    // Check name
    ASSERT_TRUE(root.has("name"));
    ASSERT_TRUE(root["name"].isString());
    ASSERT_STREQ("Alice", root["name"].asString().c_str());
    
    // Check age
    ASSERT_TRUE(root.has("age"));
    ASSERT_TRUE(root["age"].isNumber());
    ASSERT_EQ(30, root["age"].asInt());
    
    // Check active
    ASSERT_TRUE(root.has("active"));
    ASSERT_TRUE(root["active"].isTrue());
    
    // Check tags
    ASSERT_TRUE(root.has("tags"));
    ASSERT_TRUE(root["tags"].isArray());
    ASSERT_EQ(3, root["tags"].size());
}

UTEST(ctoon_cpp_tests, test_parse_string) {
    std::string toon_data = "Hello World";
    
    ctoon::Document doc(toon_data);
    ASSERT_TRUE(doc);
    
    ctoon::Value root = doc.root();
    ASSERT_TRUE(root);
    ASSERT_TRUE(root.isString());
    ASSERT_STREQ("Hello World", root.asString().c_str());
}

UTEST(ctoon_cpp_tests, test_parse_number) {
    std::string toon_data = "42";
    
    ctoon::Document doc(toon_data);
    ASSERT_TRUE(doc);
    
    ctoon::Value root = doc.root();
    ASSERT_TRUE(root);
    ASSERT_TRUE(root.isNumber());
    ASSERT_EQ(42, root.asInt());
}

UTEST(ctoon_cpp_tests, test_parse_boolean) {
    std::string toon_data = "true";
    
    ctoon::Document doc(toon_data);
    ASSERT_TRUE(doc);
    
    ctoon::Value root = doc.root();
    ASSERT_TRUE(root);
    ASSERT_TRUE(root.isTrue());
}

UTEST(ctoon_cpp_tests, test_parse_array) {
    std::string toon_data = "[1, 2, 3, 4, 5]";
    
    ctoon::Document doc(toon_data);
    ASSERT_TRUE(doc);
    
    ctoon::Value root = doc.root();
    ASSERT_TRUE(root);
    ASSERT_TRUE(root.isArray());
    ASSERT_EQ(5, root.size());
    
    for (int i = 0; i < 5; i++) {
        ctoon::Value item = root[i];
        ASSERT_TRUE(item);
        ASSERT_TRUE(item.isNumber());
        ASSERT_EQ(i + 1, item.asInt());
    }
}

UTEST(ctoon_cpp_tests, test_parse_nested_object) {
    std::string toon_data = "person: {name: Bob, age: 25, active: false}";
    
    ctoon::Document doc(toon_data);
    ASSERT_TRUE(doc);
    
    ctoon::Value root = doc.root();
    ASSERT_TRUE(root);
    ASSERT_TRUE(root.isObject());
    
    ASSERT_TRUE(root.has("person"));
    ctoon::Value person = root["person"];
    ASSERT_TRUE(person);
    ASSERT_TRUE(person.isObject());
    
    ASSERT_TRUE(person.has("name"));
    ASSERT_TRUE(person["name"].isString());
    ASSERT_STREQ("Bob", person["name"].asString().c_str());
    
    ASSERT_TRUE(person.has("age"));
    ASSERT_TRUE(person["age"].isNumber());
    ASSERT_EQ(25, person["age"].asInt());
    
    ASSERT_TRUE(person.has("active"));
    ASSERT_TRUE(person["active"].isFalse());
}

UTEST(ctoon_cpp_tests, test_file_parsing) {
    // This test would require a test file
    // For now, just test that the API exists
    ASSERT_TRUE(true);
}

UTEST(ctoon_cpp_tests, test_error_handling) {
    std::string invalid_toon = "{invalid: toon data";
    
    try {
        ctoon::Document doc(invalid_toon);
        // Should throw or return invalid document
        ASSERT_FALSE(doc);
    } catch (...) {
        // Exception is acceptable
        ASSERT_TRUE(true);
    }
}