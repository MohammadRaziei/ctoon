#include "ctoon.h"
#include <iostream>

int main() {
    std::cout << "Testing Primitive Types" << std::endl;
    std::cout << "=======================" << std::endl;

    // Test all primitive types
    ctoon::Value str("Hello Ctoon");
    ctoon::Value num(42.5);
    ctoon::Value boolean(true);
    ctoon::Value nullValue(nullptr);

    std::cout << "String: " << ctoon::dumpsToon(str) << std::endl;
    std::cout << "Number: " << ctoon::dumpsToon(num) << std::endl;
    std::cout << "Boolean: " << ctoon::dumpsToon(boolean) << std::endl;
    std::cout << "Null: " << ctoon::dumpsToon(nullValue) << std::endl;

    return 0;
}
