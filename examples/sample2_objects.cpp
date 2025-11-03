#include "ctoon.h"
#include <iostream>

int main() {
    std::cout << "Testing Nested Objects" << std::endl;
    std::cout << "======================" << std::endl;

    // Create a nested object structure
    ctoon::Object address;
    address["street"] = ctoon::Value("123 Main St");
    address["city"] = ctoon::Value("Tehran");
    address["country"] = ctoon::Value("Iran");

    ctoon::Object person;
    person["name"] = ctoon::Value("Mohammad");
    person["age"] = ctoon::Value(30.0);
    person["address"] = ctoon::Value(address);

    ctoon::Array hobbies;
    hobbies.push_back(ctoon::Value("programming"));
    hobbies.push_back(ctoon::Value("reading"));
    hobbies.push_back(ctoon::Value("hiking"));
    person["hobbies"] = ctoon::Value(hobbies);

    std::cout << "Nested Object:" << std::endl;
    std::cout << ctoon::dumpsToon(ctoon::Value(person)) << std::endl;

    return 0;
}
