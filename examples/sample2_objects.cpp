#include "ctoon.h"
#include <iostream>

int main() {
    std::cout << "Testing Nested Objects" << std::endl;
    std::cout << "======================" << std::endl;

    // Create a nested object structure
    ctoon::Object address;
    address["street"] = std::make_shared<ctoon::Value>("123 Main St");
    address["city"] = std::make_shared<ctoon::Value>("Tehran");
    address["country"] = std::make_shared<ctoon::Value>("Iran");

    ctoon::Object person;
    person["name"] = std::make_shared<ctoon::Value>("Mohammad");
    person["age"] = std::make_shared<ctoon::Value>(30.0);
    person["address"] = std::make_shared<ctoon::Value>(address);

    ctoon::Array hobbies;
    hobbies.push_back(std::make_shared<ctoon::Value>("programming"));
    hobbies.push_back(std::make_shared<ctoon::Value>("reading"));
    hobbies.push_back(std::make_shared<ctoon::Value>("hiking"));
    person["hobbies"] = std::make_shared<ctoon::Value>(hobbies);

    std::cout << "Nested Object:" << std::endl;
    std::cout << ctoon::dumpsToon(ctoon::Value(person)) << std::endl;

    return 0;
}
