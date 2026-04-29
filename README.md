# CToon

<div align="center">
<img src="docs/images/ctoon-sq-ctoon.svg" width="256" alt="CToon Logo">
</div>

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.cppreference.com/w/c/11)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.19+-blue.svg)](https://cmake.org/)
[![Python 3.8+](https://img.shields.io/badge/Python-3.8+-blue.svg)](https://www.python.org/)

</div>

A C implementation of the [TOON format](https://github.com/toon-format/spec) — a compact, human-readable serialization format optimised for LLM token usage. Achieves 30–60% token reduction versus JSON while maintaining full readability and structure.

## Quick Start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
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
| `CTOON_WITHOUT_JSON` | OFF | Disable built-in JSON reader/writer |

JSON support is **on by default** (`CTOON_ENABLE_JSON=1`). The CLI uses it directly — no external JSON library is required.

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

/* ---- Parse TOON ---- */
ctoon_doc *doc = ctoon_read("name: Alice\nage: 30", 20, 0);
ctoon_val *root = ctoon_doc_get_root(doc);

/* ---- Access ---- */
ctoon_val *name = ctoon_obj_get(root, "name");   /* O(k) linear scan */
ctoon_val *v0   = ctoon_arr_get(arr, 0);          /* O(1) */

/* ---- Type queries ---- */
ctoon_is_null(v)   ctoon_is_bool(v)   ctoon_is_num(v)
ctoon_is_str(v)    ctoon_is_arr(v)    ctoon_is_obj(v)
ctoon_is_true(v)   ctoon_is_false(v)
ctoon_is_uint(v)   ctoon_is_sint(v)   ctoon_is_real(v)

/* ---- Getters ---- */
ctoon_get_str(v)   ctoon_get_uint(v)   ctoon_get_sint(v)
ctoon_get_real(v)  ctoon_get_int(v)    ctoon_get_bool(v)
ctoon_get_len(v)

/* ---- Iterate object ---- */
ctoon_obj_iter iter = ctoon_obj_iter_with(root);
ctoon_val *key;
while ((key = ctoon_obj_iter_next(&iter)) != NULL) {
    ctoon_val *val = ctoon_obj_iter_get_val(key);
    /* ctoon_get_str(key), ctoon_get_type(val), … */
}

/* ---- Iterate array ---- */
ctoon_arr_iter it = ctoon_arr_iter_with(arr);
ctoon_val *item;
while ((item = ctoon_arr_iter_next(&it)) != NULL) { /* … */ }

/* ---- Encode to TOON ---- */
size_t len;
char *toon = ctoon_write(doc, &len);   /* caller must free() */
free(toon);

/* ---- Encode with options ---- */
ctoon_write_options opts = {
    .flag      = CTOON_WRITE_LENGTH_MARKER,
    .delimiter = CTOON_DELIMITER_PIPE,
    .indent    = 2,
};
ctoon_write_err err = {};
char *out = ctoon_write_opts(doc, &opts, NULL, &len, &err);

/* ---- JSON input (requires CTOON_ENABLE_JSON=1) ---- */
ctoon_doc *jdoc = ctoon_read_json(json_str, json_len, 0, NULL, NULL);

/* ---- JSON output ---- */
/* From immutable doc (O(n), zero copy): */
char *json = ctoon_doc_to_json(doc, 2, CTOON_WRITE_NOFLAG, NULL, &len, NULL);
/* From mutable doc (O(n), one imut_copy pass): */
char *json2 = ctoon_mut_doc_to_json(mdoc, 2, CTOON_WRITE_NOFLAG, NULL, &len, NULL);
free(json); free(json2);

/* ---- Build mutable document ---- */
ctoon_mut_doc *mdoc = ctoon_mut_doc_new(NULL);
ctoon_mut_val *obj  = ctoon_mut_obj(mdoc);
ctoon_mut_obj_add_str(mdoc, obj, "name", "Alice");
ctoon_mut_obj_add_uint(mdoc, obj, "age",  30);
ctoon_mut_doc_set_root(mdoc, obj);

/* ---- Convert between immutable and mutable ---- */
ctoon_mut_doc *m = ctoon_doc_mut_copy(doc, NULL);   /* imut → mut  (deep copy) */
ctoon_doc     *i = ctoon_mut_doc_imut_copy(m, NULL); /* mut  → imut (deep copy) */

/* ---- Cleanup ---- */
ctoon_doc_free(doc);
ctoon_mut_doc_free(mdoc);
```

## C++ API

```cpp
#include "ctoon.hpp"

// Parse TOON
ctoon::document doc = ctoon::document::parse("name: Alice\nage: 30");
ctoon::value root = doc.root();

// Access
ctoon::string_view name = root["name"].get_str();
uint64_t  age    = root["age"].get_uint();
bool      active = root["active"].get_bool();

// Iterate array
for (std::size_t i = 0; i < root["tags"].arr_size(); i++) {
    ctoon::value item = root["tags"][i];
}

// Iterate object
ctoon::value obj = root["person"];
for (std::size_t i = 0; i < obj.obj_size(); i++) {
    ctoon::value k = obj.obj_key_at(i);
    ctoon::value v = obj.obj_val_at(i);
}

// Optional get
if (auto v = root.get("optional_field")) {
    std::cout << v->get_str() << "\n";
}

// Encode to TOON
ctoon::write_result toon = doc.encode();
std::string toon_str = toon.str();

// With options
ctoon::write_options opts;
opts.set_delimiter(ctoon::delimiter::PIPE);
opts.set_flag(ctoon::write_flag::LENGTH_MARKER);
ctoon::write_result toon2 = doc.encode(opts);

// JSON input/output (requires CTOON_ENABLE_JSON=1)
ctoon::document jdoc = ctoon::document::parse_json("{\"x\":1}");
ctoon::document jdoc2 = ctoon::document::parse_json(str);       // std::string
ctoon::document jdoc3 = ctoon::document::parse_json(sv);        // string_view
ctoon::document jdoc4 = ctoon::document::parse_json_file("f.json");

ctoon::write_result json  = doc.to_json(2);   // immutable doc → JSON
ctoon::write_result json2 = mdoc.to_json(2);  // mutable   doc → JSON

// Free functions
ctoon::write_result r  = ctoon::to_json(doc,  2);
ctoon::write_result r2 = ctoon::to_json(mdoc, 2);

// Build mutable
ctoon::mut_document mdoc = ctoon::mut_document::create();
ctoon::mut_value obj2 = mdoc.make_obj();
mdoc.set_root(obj2);
obj2.obj_put(mdoc.make_str("name"), mdoc.make_str("Alice"));
obj2.obj_put(mdoc.make_str("age"),  mdoc.make_uint(30));

// Convert immutable ↔ mutable
ctoon::mut_document m = doc.mut_copy();
ctoon::document     i = mdoc.imut_copy();

// File I/O
ctoon::document doc3 = ctoon::document::parse_file("data.toon");
doc3.write_file("out.toon");
```

## Complexity

| Operation | Time | Notes |
|-----------|------|-------|
| `ctoon_read` / `ctoon_read_json` | O(n) | n = input bytes |
| `ctoon_write` / `ctoon_doc_to_json` | O(n) | n = total nodes, zero copy for imut |
| `ctoon_mut_doc_to_json` | O(n) | one imut_copy pass then serialize |
| `ctoon_arr_get(arr, i)` | **O(1)** | direct index |
| `ctoon_obj_get(obj, key)` | O(k) | linear scan; k = key count |
| `ctoon_obj_iter_next` | O(1) per step | |
| `ctoon_doc_mut_copy` | O(n) | deep copy |
| `ctoon_mut_doc_imut_copy` | O(n) | deep copy |
| `ctoon_doc_free` | O(chunks) ≈ O(1) | arena freed in one pass |

**Why no O(1) mut→imut cast?** Mutable documents use a circular doubly-linked list; immutable docs use a flat contiguous arena with `uni.ofs` offsets. The layouts are fundamentally different, so a zero-copy view is not possible — a full O(n) walk is required.

## Thread Safety

| | Safe? |
|-|-------|
| Multiple threads reading different documents | ✅ Yes |
| Multiple threads reading the same document | ✅ Yes |
| `ctoon_arr_get` / `ctoon_arr_iter_next` | ✅ Yes |
| `ctoon_obj_iter_get/next` | ⚠️ Thread-local (one iterator per thread) |
| Writing/building a document from multiple threads | ❌ No (arena not thread-safe) |

## License

MIT