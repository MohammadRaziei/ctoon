# <a href="https://mohammadraziei.github.io/ctoon/"><img src="https://raw.githubusercontent.com/MohammadRaziei/ctoon/refs/heads/master/docs/images/ctoon-sq.svg" width="25" alt="CToon Logo"> CToon </a>

<div align="center">
<a href="https://mohammadraziei.github.io/ctoon/"><img src="https://raw.githubusercontent.com/MohammadRaziei/ctoon/refs/heads/master/docs/images/ctoon-sq-ctoon.svg" width="360" alt="CToon Long Logo"></a>
</div>

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C](https://img.shields.io/badge/C-99-blue.svg)](https://en.cppreference.com/w/c)
[![C++11](https://img.shields.io/badge/C++-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Go 1.21+](https://img.shields.io/badge/Go-1.21+-blue.svg)](https://go.dev/)
[![Python 3.9+](https://img.shields.io/badge/Python-3.9+-blue.svg)](https://www.python.org/)
[![MATLAB R2014b+](https://img.shields.io/badge/MATLAB-R2014b+-orange.svg)](https://www.mathworks.com/products/matlab.html)
[![CMake 3.19+](https://img.shields.io/badge/CMake-3.19+-blue.svg)](https://cmake.org/)

**[Documentation](https://mohammadraziei.github.io/ctoon)**

</div>

The fastest implementation of the [TOON format](https://github.com/toon-format/toon) — a compact, human-readable serialisation format designed to minimise LLM token usage. Achieves 30-60% token reduction versus JSON while remaining fully readable and structured.

CToon is built on a high-performance C core and exposes the same logic through idiomatic bindings for C++, Python, Go, and MATLAB. The name reflects its foundation: **C** + **TOON**.

## Format Overview

```
# JSON  (352 bytes)                  # TOON  (165 bytes, -53 %)
{                                    order:
  "order": {                           id: ORD-12345
    "id": "ORD-12345",                 status: completed
    "items": [                         customer:
      {"product":"Book","qty":2},        name: John Doe
      {"product":"Pen","qty":5}          email: john@example.com
    ]                                  items[2]{product,qty}:
  }                                      Book,2
}                                        Pen,5
```

---

## Quick Start

### CLI

```bash
cmake -B build && cmake --build build
./build/ctoon data.json           # JSON  -> TOON
./build/ctoon data.toon           # TOON  -> JSON
cat data.json | ./build/ctoon     # stdin -> TOON
```

### C

```c
#include "ctoon.h"

ctoon_doc *doc = ctoon_read("name: Alice\nage: 30", 20, 0);
ctoon_val *root = ctoon_doc_get_root(doc);

ctoon_val *name = ctoon_obj_get(root, "name");
printf("%s\n", ctoon_get_str(name));     /* Alice */

size_t len;
char *toon = ctoon_write(doc, &len);     /* caller must free() */
free(toon);
ctoon_doc_free(doc);

/* JSON output (requires CTOON_ENABLE_JSON=1, on by default) */
char *json = ctoon_doc_to_json(doc, 2, CTOON_WRITE_NOFLAG, NULL, &len, NULL);
free(json);
```

### C++

```cpp
#include "ctoon.hpp"

/* Parse TOON */
auto doc  = ctoon::document::parse("name: Alice\nage: 30");
auto root = doc.root();
std::cout << root["name"].get_str().str() << "\n";  // Alice
std::cout << root["age"].get_uint()        << "\n";  // 30

/* Serialise */
std::cout << doc.write().c_str()    << "\n";  // TOON
std::cout << doc.to_json(2).c_str() << "\n";  // JSON

/* Build a mutable document */
auto mdoc = ctoon::make_document();
auto obj  = mdoc.make_obj();
mdoc.set_root(obj);
obj.obj_put(mdoc.make_str("city"), mdoc.make_str("Tehran"));
obj.obj_put(mdoc.make_str("pop"),  mdoc.make_uint(9'000'000));
std::cout << mdoc.to_json(0).c_str() << "\n";  // {"city":"Tehran","pop":9000000}
```

### Python

```python
import ctoon

# encode / decode
toon = ctoon.dumps({"name": "Alice", "age": 30})
data = ctoon.loads(toon)

# file I/O
ctoon.dump(data, "out.toon")
data = ctoon.load("out.toon")

# JSON
json_str = ctoon.dumps_json(data, indent=2)
data     = ctoon.loads_json(json_str)
```

### Go

```go
import ctoon "github.com/mohammadraziei/ctoon"

// encode / decode
toon, _ := ctoon.Dumps(map[string]interface{}{"name": "Alice", "age": int64(30)})
val,  _ := ctoon.Loads(toon)

// file I/O
ctoon.EncodeToFile(data, "out.toon", ctoon.DefaultEncodeOptions())
val, _ = ctoon.DecodeFromFile("out.toon", ctoon.DefaultDecodeOptions())
```

### MATLAB

```matlab
% Install once (compiles MEX and adds to MATLAB path permanently)
cd src/bindings/matlab
ctoon_install

% Encode a MATLAB value -> TOON string
s = ctoon_encode(struct('name', 'Alice', 'age', uint64(30), 'active', true));

% Decode TOON string -> MATLAB value
v = ctoon_decode(s);
v.name    % -> 'Alice'
v.age     % -> uint64(30)
v.active  % -> true

% File I/O
ctoon_write(v, 'config.toon');
v = ctoon_read('config.toon');
```

MATLAB type mapping:

| MATLAB | TOON |
|--------|------|
| `[]` | null |
| `logical` | bool |
| `double` scalar | real |
| `int64` scalar | sint |
| `uint64` scalar | uint |
| `char` | str |
| `cell` | array |
| `struct` | object |

Requires MATLAB R2014b+ and a C compiler configured for MEX (`mex -setup C`).

---

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) --target ctoon_test     # Run all tests
cmake --build build -j$(nproc) --target ctoon_coverage # Generate coverage reports
```

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `CTOON_BUILD_TESTS` | ON | Build C and C++ tests |
| `CTOON_BUILD_PYTHON` | OFF | Build Python extension (nanobind) |
| `CTOON_BUILD_MATLAB` | OFF | Build MATLAB MEX binding |
| `CTOON_BUILD_DOCS` | OFF | Build documentation |

JSON support is **on by default** (`CTOON_ENABLE_JSON=1`). No external JSON library required.

### Python package

```bash
pip install ctoon
```

### Go module

```bash
go get github.com/mohammadraziei/ctoon
```

Requires Go 1.21+. Uses CGo to call the C core.

### MATLAB MEX

```bash
# Via CMake
cmake -B build -DCTOON_BUILD_MATLAB=ON -DMatlab_ROOT_DIR=/path/to/matlab
cmake --build build --target ctoon_mex

# Or directly from MATLAB
cd src/bindings/matlab
ctoon_install        % compile + add to path permanently
```

---

## CLI Reference

```bash
# Encode  JSON -> TOON
ctoon input.json
ctoon input.json -o output.toon
ctoon input.json --delimiter pipe      # comma (default), tab, pipe
ctoon input.json --length-marker       # items[#3]: ...
ctoon input.json --stats               # show byte savings

# Decode  TOON -> JSON
ctoon input.toon
ctoon input.toon -o output.json
ctoon input.toon -i 4                  # 4-space JSON indent

# Force format (auto-detected from extension by default)
ctoon -e input.json    # force encode
ctoon -d input.toon    # force decode

# Stdin
cat data.json | ctoon -e -
cat data.toon | ctoon -d -
```

---

## C API Reference

### Parsing

```c
/* from memory */
ctoon_doc *ctoon_read(const char *toon, size_t len, ctoon_read_flag flags);
ctoon_doc *ctoon_read_opts(char *toon, size_t len, ctoon_read_flag flags,
                            const ctoon_alc *alc, ctoon_read_err *err);
/* from file */
ctoon_doc *ctoon_read_file(const char *path, ctoon_read_flag flags,
                            const ctoon_alc *alc, ctoon_read_err *err);
/* from JSON */
ctoon_doc *ctoon_read_json(char *json, size_t len, ctoon_read_flag flags,
                            const ctoon_alc *alc, ctoon_read_err *err);
ctoon_doc *ctoon_read_json_file(const char *path, ctoon_read_flag flags,
                                 const ctoon_alc *alc, ctoon_read_err *err);
```

### Writing

```c
/* TOON output */
char *ctoon_write(const ctoon_doc *doc, size_t *len);
char *ctoon_write_opts(const ctoon_doc *doc, const ctoon_write_options *opts,
                        const ctoon_alc *alc, size_t *len, ctoon_write_err *err);
char *ctoon_mut_write(const ctoon_mut_doc *doc, size_t *len);

/* JSON output */
char *ctoon_doc_to_json(const ctoon_doc *doc, int indent,
                         ctoon_write_flag flags, const ctoon_alc *alc,
                         size_t *len, ctoon_write_err *err);
```

### Access

```c
/* document */
ctoon_val *ctoon_doc_get_root(const ctoon_doc *doc);
void       ctoon_doc_free(ctoon_doc *doc);

/* type checks */
bool ctoon_is_null(ctoon_val *v);   bool ctoon_is_bool(ctoon_val *v);
bool ctoon_is_true(ctoon_val *v);   bool ctoon_is_false(ctoon_val *v);
bool ctoon_is_uint(ctoon_val *v);   bool ctoon_is_sint(ctoon_val *v);
bool ctoon_is_real(ctoon_val *v);   bool ctoon_is_str(ctoon_val *v);
bool ctoon_is_arr(ctoon_val *v);    bool ctoon_is_obj(ctoon_val *v);

/* getters */
const char *ctoon_get_str(ctoon_val *v);
uint64_t    ctoon_get_uint(ctoon_val *v);
int64_t     ctoon_get_sint(ctoon_val *v);
double      ctoon_get_real(ctoon_val *v);
bool        ctoon_get_bool(ctoon_val *v);

/* array */
size_t     ctoon_arr_size(ctoon_val *arr);
ctoon_val *ctoon_arr_get(ctoon_val *arr, size_t idx);   /* O(1) */

/* object */
size_t     ctoon_obj_size(ctoon_val *obj);
ctoon_val *ctoon_obj_get(ctoon_val *obj, const char *key);

/* iteration */
ctoon_obj_iter ctoon_obj_iter_with(ctoon_val *obj);
ctoon_val     *ctoon_obj_iter_next(ctoon_obj_iter *iter);
ctoon_val     *ctoon_obj_iter_get_val(ctoon_val *key);
```

### Mutable documents

```c
ctoon_mut_doc *ctoon_mut_doc_new(const ctoon_alc *alc);
void           ctoon_mut_doc_free(ctoon_mut_doc *doc);
void           ctoon_mut_doc_set_root(ctoon_mut_doc *doc, ctoon_mut_val *root);

ctoon_mut_val *ctoon_mut_null(ctoon_mut_doc *doc);
ctoon_mut_val *ctoon_mut_bool(ctoon_mut_doc *doc, bool val);
ctoon_mut_val *ctoon_mut_uint(ctoon_mut_doc *doc, uint64_t val);
ctoon_mut_val *ctoon_mut_sint(ctoon_mut_doc *doc, int64_t val);
ctoon_mut_val *ctoon_mut_real(ctoon_mut_doc *doc, double val);
ctoon_mut_val *ctoon_mut_str(ctoon_mut_doc *doc, const char *str);
ctoon_mut_val *ctoon_mut_arr(ctoon_mut_doc *doc);
ctoon_mut_val *ctoon_mut_obj(ctoon_mut_doc *doc);

bool ctoon_mut_arr_append(ctoon_mut_val *arr, ctoon_mut_val *item);
bool ctoon_mut_obj_put(ctoon_mut_val *obj, ctoon_mut_val *key, ctoon_mut_val *val);

/* convert between immutable and mutable */
ctoon_mut_doc *ctoon_doc_mut_copy(ctoon_doc *doc, const ctoon_alc *alc);
ctoon_doc     *ctoon_mut_doc_imut_copy(ctoon_mut_doc *doc, const ctoon_alc *alc);
```

---

## Complexity

| Operation | Time | Notes |
|-----------|------|-------|
| `ctoon_read` / `ctoon_read_json` | O(n) | n = input bytes |
| `ctoon_write` / `ctoon_doc_to_json` | O(n) | zero extra copy for immutable doc |
| `ctoon_arr_get(arr, i)` | **O(1)** | direct index into flat arena |
| `ctoon_obj_get(obj, key)` | O(k) | linear scan; k = key count |
| `ctoon_doc_free` | O(chunks) ~= O(1) | arena freed in one shot |

---

## Thread Safety

| Scenario | Safe? |
|----------|-------|
| Multiple threads reading different documents | Yes |
| Multiple threads reading the same document | Yes |
| Building a document from multiple threads | No — arena not thread-safe |

---

## Requirements

| Component | Requirement |
|-----------|-------------|
| C core | C99, no dependencies |
| C++ binding | C++11, header-only |
| Python binding | Python 3.9+, nanobind >= 2.0, CMake 3.19+ |
| Go binding | Go 1.21+, CGo |
| MATLAB binding | MATLAB R2014b+, C compiler for MEX |
| CLI | C++17 |

---

## License

MIT — see [LICENSE](LICENSE).

## Related

- [toon-format/toon](https://github.com/toon-format/toon) — the TOON format specification