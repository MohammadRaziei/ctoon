#include <cstdio>
#include "ctoon.hpp"

int main() {
    ctoon::document doc = ctoon::parse("a: 1");
    (void)doc;
    if (ctoon_version() != static_cast<uint32_t>(CTOON_VERSION_HEX)) return 1;
    std::printf("ctoon %s (C++)\n", CTOON_VERSION_STRING);
    return 0;
}
