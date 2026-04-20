# CToon

<div align="center">
<img src="docs/images/ctoon-sq-ctoon.svg" width="256" alt="CToon Logo">
</div>

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.19+-blue.svg)](https://cmake.org/)
[![Python 3.8+](https://img.shields.io/badge/Python-3.8+-blue.svg)](https://www.python.org/)

</div>

A C implementation of the [TOON format](https://github.com/toon-format/spec) — a compact, human-readable serialization format optimised for LLM token usage. Achieves 30–60% token reduction versus JSON while maintaining full readability and structure.

## Quick Start

```bash
cmake -B build && cmake --build build
./build/ctoon data.json          # JSON → TOON
./build/ctoon data.toon          # TOON → JSON
cat data.json | ./build/ctoon    # stdin → TOON
```

## Format Overview

TOON encodes structured data efficiently:

```
# JSON (352 bytes)                 # TOON (165 bytes, -53%)
{                                  order:
  "order": {                         id: ORD-12345
    "id": "ORD-12345",               status: completed
    "items": [                       customer:
      {"product":"Book","qty":2},      name: John Doe
      {"product":"Pen","qty":5}        email: john@example.com
    ]                                items[2]{product,qty}:
  }                                    Book,2
}                                      Pen,5
```

## CLI

```bash
# Encode (JSON → TOON)
ctoon input.json
ctoon input.json -o output.toon
ctoon input.json --delimiter ","    # comma (default)
ctoon input.json --delimiter "|"    # pipe
ctoon input.json --delimiter "tab"  # tab
ctoon input.json --length-marker    # items[#3]: ...
ctoon input.json --stats            # show byte savings

# Decode (TOON → JSON)
ctoon input.toon
ctoon input.toon -o output.json
ctoon input.toon --no-strict        # lenient parsing
ctoon input.toon -i 4               # 4-space JSON indent

# Auto-detection from extension; force with flags
ctoon -e input.json   # force encode
ctoon -d input.toon   # force decode

# Stdin
cat data.json | ctoon -e -
cat data.toon | ctoon -d -
```

## C API

```c
#include "ctoon.h"

// --- Parse ---
ctoon_doc *doc = ctoon_decode("name: Alice\nage: 30", 20);
ctoon_val *root = ctoon_doc_get_root(doc);

// --- Access ---
ctoon_val *name = ctoon_obj_get(root, "name");   // O(k)
ctoon_val *v0   = ctoon_arr_get(arr, 0);          // O(1)
ctoon_val *key  = ctoon_obj_get_key_at(obj, i);   // O(1), thread-safe
ctoon_val *val  = ctoon_obj_get_val_at(obj, i);   // O(1), thread-safe

// --- Iterate (full tree: O(n)) ---
for (size_t i = 0; i < ctoon_obj_size(root); i++) {
    ctoon_val *k = ctoon_obj_get_key_at(root, i);
    ctoon_val *v = ctoon_obj_get_val_at(root, i);
    // use ctoon_get_str(k), ctoon_get_type(v), etc.
}

// --- Type queries ---
ctoon_is_null(v)    ctoon_is_bool(v)    ctoon_is_num(v)
ctoon_is_str(v)     ctoon_is_arr(v)     ctoon_is_obj(v)
ctoon_get_subtype(v)  // UINT / SINT / REAL

// --- Getters ---
ctoon_get_str(v)    ctoon_get_uint(v)   ctoon_get_sint(v)
ctoon_get_real(v)   ctoon_get_int(v)    ctoon_get_len(v)

// --- Encode ---
size_t len;
char *toon = ctoon_encode(doc, &len);   // caller must free()
free(toon);

// --- With options ---
ctoon_write_options opts = {
    .flag      = CTOON_WRITE_LENGTH_MARKER,
    .delimiter = CTOON_DELIMITER_PIPE,
    .indent    = 2,
};
ctoon_err err = {};
char *out = ctoon_encode_opts(doc, &opts, &len, &err);

// --- Build values ---
ctoon_doc *doc2 = ctoon_doc_new(0);
ctoon_val *obj  = ctoon_new_obj(doc2);
ctoon_val *name = ctoon_new_str(doc2, "Alice");
ctoon_val *age  = ctoon_new_uint(doc2, 30);
ctoon_obj_set(doc2, obj, "name", name);
ctoon_obj_set(doc2, obj, "age",  age);
ctoon_doc_set_root(doc2, obj);

// --- Cleanup ---
ctoon_doc_free(doc);
```

## C++ API

```cpp
#include "ctoon.hpp"

// Decode
ctoon::Document doc = ctoon::decode("name: Alice\nage: 30");
ctoon::Value root = doc.root();

// Access
std::string name = root["name"].asString();
int age          = root["age"].asInt();
bool active      = root["active"].asBool();

// Iterate array
for (ctoon::Value item : root["tags"]) {
    std::cout << item.asString() << "\n";
}

// Iterate object keys
for (const std::string &key : root.keys()) {
    ctoon::Value v = root[key];
}

// Optional get
if (auto v = root.get("optional_field")) {
    std::cout << v->asString();
}

// Encode
std::string toon = doc.encode();

// With options
ctoon::EncodeOptions opts;
opts.delimiter = ctoon::Delimiter::Pipe;
opts.flag      = ctoon::WriteFlag::LengthMarker;
std::string toon2 = doc.encode(opts);

// Build
ctoon::Document doc2 = ctoon::Document::create();
ctoon::Value obj  = doc2.newObject();
ctoon::Value name = doc2.newString("Alice");
doc2.objectSet(obj, "name", name);
doc2.setRoot(obj);

// File I/O
ctoon::Document doc3 = ctoon::Document::fromFile("data.toon");
doc3.writeFile("out.toon");
```

## Complexity

| Operation | Time | Notes |
|-----------|------|-------|
| `ctoon_decode` / `ctoon_read` | O(n) | n = input bytes |
| `ctoon_encode` / `ctoon_write` | O(n) | n = total nodes |
| `ctoon_arr_get(arr, i)` | **O(1)** | direct index |
| `ctoon_obj_get_key_at(obj, i)` | **O(1)** | direct index, thread-safe |
| `ctoon_obj_get_val_at(obj, i)` | **O(1)** | direct index, thread-safe |
| `ctoon_obj_get(obj, key)` | O(k) | linear scan; k = key count |
| Full tree traversal from root | **O(n)** | optimal; n = total nodes |
| `ctoon_doc_free` | O(chunks) ≈ O(1) | frees arena in one pass |

**Full tree traversal is O(n)** — each of the n nodes is visited exactly once. This is optimal since you must read every node. Stack depth is O(d) where d = nesting depth.

`ctoon_obj_get(key)` is O(k) linear scan (no hash map). For deeply nested objects with many keys, prefer `ctoon_obj_get_key_at` / `ctoon_obj_get_val_at` in indexed loops.

## Thread Safety

| | Safe? |
|-|-------|
| Multiple threads reading different documents | ✅ Yes |
| Multiple threads reading the same document | ✅ Yes |
| `ctoon_obj_get_key_at` / `ctoon_obj_get_val_at` | ✅ Yes |
| `ctoon_arr_get` | ✅ Yes |
| `ctoon_obj_iter_get/next` | ⚠️ Thread-local (one iterator per thread) |
| Writing/building a document from multiple threads | ❌ No (arena not thread-safe) |

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build       # run tests
```

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `CTOON_BUILD_TESTS` | ON | Build C and C++ tests |
| `CTOON_BUILD_PYTHON` | OFF | Build Python bindings |

## License

MIT
