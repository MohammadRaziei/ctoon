# CToon C++ API {#mainpage}

**CToon** is a high-performance, zero-dependency C99 library for reading and
writing the **TOON** serialization format, with built-in **JSON** interop.
This reference documents the header-only C++11 RAII wrapper declared in
`ctoon.hpp`, built on top of the C core.

## Installation

```cmake
# CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
  ctoon
  GIT_REPOSITORY https://github.com/MohammadRaziei/ctoon.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(ctoon)

# Link the C++ target
target_link_libraries(my_app PRIVATE ctoon::ctoonpp)
```

Requires CMake 3.19+ and a C++11-compatible compiler. All RAII lifetime
management is included automatically — no manual free calls needed.

## Quick example

```cpp
#include "ctoon.hpp"
#include <iostream>

int main() {
    // Parse a TOON document
    auto doc  = ctoon::document::parse("name: Alice\nage: 30");
    auto root = doc.root();
    std::cout << root["name"].get_str().str() << "\n";  // Alice
    std::cout << root["age"].get_uint()        << "\n";  // 30

    // Serialise to TOON and JSON
    std::cout << doc.write().c_str()    << "\n";
    std::cout << doc.to_json(2).c_str() << "\n";

    // Build a document programmatically
    auto mdoc = ctoon::make_document();
    auto obj  = mdoc.make_obj();
    mdoc.set_root(obj);
    obj.obj_put(mdoc.make_str("city"), mdoc.make_str("Tehran"));
    obj.obj_put(mdoc.make_str("pop"),  mdoc.make_uint(9000000));
    std::cout << mdoc.to_json(0).c_str() << "\n";

    // Parse from JSON
    auto jdoc = ctoon::document::from_json(R"({"x":1,"y":2})");
    std::cout << jdoc.root()["x"].get_uint() << "\n";  // 1
}
```

See the `ctoon` namespace reference for the full API — `ctoon::document`,
`ctoon::value`, `ctoon::make_document`, and the `EncodeOptions` /
`DecodeOptions` structs.
