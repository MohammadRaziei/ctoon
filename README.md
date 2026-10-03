# <a href="https://mohammadraziei.github.io/ctoon/"><img src="https://raw.githubusercontent.com/MohammadRaziei/ctoon/refs/heads/master/docs/images/ctoon-sq.svg" width="25" alt="CToon Logo"> CToon </a>

<div align="center">
<a href="https://mohammadraziei.github.io/ctoon/"><img src="https://raw.githubusercontent.com/MohammadRaziei/ctoon/refs/heads/master/docs/images/ctoon-sq-ctoon.svg" width="360" alt="CToon Long Logo"></a>
</div>

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![CMake 3.19+](https://img.shields.io/badge/CMake-3.19+-blue.svg)](https://cmake.org/)<!-- BEGIN CTOON_SPEC_BADGE -->
[![TOON spec v4.1.2](https://img.shields.io/badge/TOON%20spec-v4.1.2-blue.svg)](https://github.com/toon-format/spec/tree/v4.1.2)<!-- END CTOON_SPEC_BADGE -->

[![C](https://img.shields.io/badge/C-99-blue.svg)](https://en.cppreference.com/w/c)
[![C++11](https://img.shields.io/badge/C++-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![Go 1.21+](https://img.shields.io/badge/Go-1.21+-blue.svg)](https://go.dev/)
[![Julia 1.7+](https://img.shields.io/badge/Julia-1.7+-blue.svg)](https://julialang.org/)
[![MATLAB R2014b+](https://img.shields.io/badge/MATLAB-R2014b+-blue.svg)](https://www.mathworks.com/products/matlab.html)
[![Python 3.9+](https://img.shields.io/badge/Python-3.9+-blue.svg)](https://www.python.org/)
[![Rust 1.70+](https://img.shields.io/badge/Rust-1.70+-blue.svg)](https://www.rust-lang.org/)
[![Zig 0.16.0+](https://img.shields.io/badge/Zig-0.16.0+-blue.svg)](https://ziglang.org/)

**[Documentation](https://mohammadraziei.github.io/ctoon)**

</div>

The fastest implementation of the [TOON format](https://github.com/toon-format/toon) — a compact, human-readable serialisation format designed to minimise LLM token usage. Achieves 30-60% token reduction versus JSON while remaining fully readable and structured.

CToon is built on a high-performance C core and exposes the same logic through idiomatic bindings for C++, Go, Julia, MATLAB, Python, Rust, and Zig, plus a standalone CLI. The name reflects its foundation: **C** + **TOON**.

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

Pick your language below — each section is install, a short example, then a
link to that language's full reference for everything else (flags, options,
error handling, ...). Order matches the tabs on the
[documentation site](https://mohammadraziei.github.io/ctoon).

### CLI

```bash
pip install ctoon        # ships a pre-built `ctoon` binary — no compiler needed
```

```bash
ctoon input.json                 # JSON  -> TOON (auto-detected from extension)
ctoon input.toon -o output.json  # TOON  -> JSON
cat data.json | ctoon -e -       # stdin -> TOON
```

Building from source with CMake instead is also supported (`sudo cmake
--install build` after building — see [Building & Testing](#building--testing-from-source)).
→ [Full CLI reference](https://mohammadraziei.github.io/ctoon/cli/index.html)

### C

Requires CMake 3.19+ and a C99 compiler.

```cmake
# CMakeLists.txt
include(FetchContent)
FetchContent_Declare(ctoon
  GIT_REPOSITORY https://github.com/MohammadRaziei/ctoon.git  GIT_TAG main)
FetchContent_MakeAvailable(ctoon)
target_link_libraries(my_app PRIVATE ctoon::ctoon)
```

```c
#include "ctoon.h"

ctoon_doc *doc = ctoon_read("name: Alice\nage: 30", 20, 0);
ctoon_val *root = ctoon_doc_get_root(doc);
printf("%s\n", ctoon_get_str(ctoon_obj_get(root, "name")));  /* Alice */

size_t len;
char *toon = ctoon_write(doc, &len);   /* caller must free() */
free(toon);
ctoon_doc_free(doc);
```

→ [Full C API reference](https://mohammadraziei.github.io/ctoon/c/html/index.html) (also summarised [below](#c-api-reference))

### C++

Requires CMake 3.19+ and a C++11 compiler. Header-only on top of the C core.

```cmake
# same FetchContent block as C, then:
target_link_libraries(my_app PRIVATE ctoon::ctoonpp)
```

```cpp
#include "ctoon.hpp"

auto doc  = ctoon::document::parse("name: Alice\nage: 30");
auto root = doc.root();
std::cout << root["name"].get_str().str() << "\n";  // Alice
std::cout << doc.to_json(2).c_str() << "\n";         // pretty JSON
```

→ [Full C++ API reference](https://mohammadraziei.github.io/ctoon/cpp/html/index.html)

### Go

Requires Go 1.21+ with CGo enabled; a C compiler must be on the build host.

```bash
go get github.com/mohammadraziei/ctoon
```

```go
import ctoon "github.com/mohammadraziei/ctoon"

toon, _ := ctoon.Dumps(map[string]interface{}{"name": "Alice", "age": int64(30)})
val, _  := ctoon.Loads(toon)
```

→ [Full Go reference](https://mohammadraziei.github.io/ctoon/go/index.html)

### Julia

Requires Julia 1.7+ and a C compiler. Not yet in Julia's General registry.

```julia
import Pkg
Pkg.add(url="https://github.com/mohammadraziei/ctoon.git", subdir="src/bindings/julia")
```

```julia
using CToon

data = CToon.parse("""{"name": "Alice", "age": 30}""")
println(data["name"])   # Alice
```

→ [Full Julia reference](https://mohammadraziei.github.io/ctoon/julia/index.html)

### MATLAB

Requires MATLAB R2014b+ and a C compiler configured for MEX (`mex -setup C`).

```matlab
cd src/bindings/matlab
ctoon_install    % compiles the MEX gateway, adds it to your MATLAB path
```

```matlab
s = ctoon.encode(struct('name', 'Alice', 'age', uint64(30)));
v = ctoon.decode(s);
v.name    % 'Alice'
```

→ [Full MATLAB reference](https://mohammadraziei.github.io/ctoon/matlab/index.html) (type mapping, `ctoon.read`/`ctoon.write`, Python-style aliases, ...)

### Python

Requires Python 3.9+. Pre-built wheels for Linux, macOS and Windows — no compiler needed.

```bash
pip install ctoon
```

```python
import ctoon

toon = ctoon.dumps({"name": "Alice", "age": 30})
data = ctoon.loads(toon)
```

→ [Full Python reference](https://mohammadraziei.github.io/ctoon/python/html/index.html)

### Rust

Requires Rust 1.70+ and a C compiler on the build host.

```toml
[dependencies]
ctoon = { git = "https://github.com/mohammadraziei/ctoon.git" }
```

```rust
let val = ctoon::loads("name: Alice\nage: 30")?;
println!("{}", val["name"].as_str().unwrap()); // Alice
let toon = ctoon::dumps(&val)?;
```

→ [Full Rust reference](https://mohammadraziei.github.io/ctoon/rust/doc/ctoon/index.html)

### Zig

Requires Zig 0.16.0+.

```bash
zig fetch --save git+https://github.com/mohammadraziei/ctoon.git
```

```zig
const ctoon = @import("ctoon");

var val = try ctoon.loads(gpa, "name: Alice\nage: 30");
defer val.deinit(gpa);
const toon = try ctoon.dumps(gpa, val);
```

→ [Full Zig reference](https://mohammadraziei.github.io/ctoon/zig/index.html)

---

## TOON Spec Version

The [toon-format/spec](https://github.com/toon-format/spec) release a build targets is exposed in every language (spec §13: implementations SHOULD declare it). It is resolved at CMake configure time and recorded in `supported_spec.conf`; the bindings read it from the C core, so it can't drift. Each language gives you the **version** (`4.1`), the **tag** (`v4.1.2`) and the **release date**:

| Language | Version | Tag | Date |
|---|---|---|---|
| C | `CTOON_SPEC_VERSION_STRING` | `CTOON_SPEC_TAG_STRING` | `CTOON_SPEC_DATE_STRING` |
| C++ | `ctoon::spec::version::string()` | `ctoon::spec::tag::string()` | `ctoon::spec::date::string()` |
| Go | `ctoon.SpecVersion()` | `ctoon.SpecTag()` | `ctoon.SpecDate()` |
| Julia | `CToon.spec_version()` | `CToon.spec_tag()` | `CToon.spec_date()` |
| MATLAB | `info.Spec.Version` | `info.Spec.Tag` | `info.Spec.Date` (`[~, info] = ctoon.version()`) |
| Python | `ctoon.__toon_spec__` | `ctoon.__toon_spec_tag__` | `ctoon.__toon_spec_date__` |
| Rust | `ctoon::spec_version()` | `ctoon::spec_tag()` | `ctoon::spec_date()` |
| Zig | `ctoon.specVersion()` | `ctoon.specTag()` (`major`/`minor`/`patch`) | `ctoon.specDate()` |

In C and C++ the tag is also available as numbers (`CTOON_SPEC_TAG_MAJOR/MINOR/PATCH`, `CTOON_SPEC_TAG_HEX`; `ctoon::spec::tag::major()` etc.).

---

## Building & Testing From Source

Each binding can be built, run and tested entirely with its own native
tooling — `cargo test` for Rust, `go test` for Go, `pytest` for Python, `zig
build test` for Zig, `Pkg.test()` for Julia, MATLAB's `buildtool test` — pick
whichever's already open and go. But the project **as a whole** — building
every language together, running the full cross-language test matrix,
generating coverage, and building the documentation site — is driven by
**CMake**, the same commands CI uses. This section covers that path; see
[CONTRIBUTING.md](CONTRIBUTING.md) for the complete contributor workflow.

### Configure

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

By default this configures only C, C++ and the CLI. Each other language is
opt-in via its own flag — and the flag changes what a *missing* toolchain
does: silently skipped when the flag is OFF (the default, so the command
above works on a machine with only some toolchains installed), a hard
configure error when you've explicitly turned it ON:

```bash
# e.g. working on the Rust binding
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCTOON_BUILD_RUST=ON

# everything, like CI
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCTOON_BUILD_PYTHON=ON -DCTOON_BUILD_GO=ON -DCTOON_BUILD_RUST=ON \
      -DCTOON_BUILD_ZIG=ON -DCTOON_BUILD_JULIA=ON
```

#### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `CTOON_BUILD_TESTS` | ON (top-level) | Build and register tests. |
| `CTOON_BUILD_EXAMPLES` | ON (top-level) | Build the examples. |
| `CTOON_BUILD_DOCS` | OFF | Build documentation. |
| `CTOON_BUILD_PYTHON` | OFF | Build the Python extension (nanobind). |
| `CTOON_BUILD_GO` | OFF | Require the Go toolchain (error instead of skip if missing). |
| `CTOON_BUILD_RUST` | OFF | Require the Rust toolchain (error instead of skip if missing). |
| `CTOON_BUILD_ZIG` | OFF | Require the Zig toolchain (error instead of skip if missing). |
| `CTOON_BUILD_JULIA` | OFF | Require the Julia toolchain (error instead of skip if missing). |
| `CTOON_BUILD_MATLAB` | OFF | Require the MATLAB toolchain (error instead of skip if missing). |
| `CTOON_BUILD_ALL_LANGS` | OFF | Require every self-building language's toolchain at once. |

JSON support is **on by default**. Define `CTOON_DISABLE_JSON` to turn it off — no external JSON library is required either way.

### Build

```bash
cmake --build build -j
```

### Test

```bash
ctest --test-dir build --output-on-failure --extra-verbose
# equivalently:
cmake --build build --target ctoon_test
```

Per-language targets, for iterating on one binding at a time:
`ctoon_test_c`, `ctoon_test_cpp`, `ctoon_test_go`, `ctoon_test_julia`,
`ctoon_test_matlab`, `ctoon_test_python`, `ctoon_test_rust`, `ctoon_test_zig`
— each also runnable as a single `ctest` case, e.g. `ctest --test-dir build -R rust`.

### Coverage

```bash
cmake --build build --target ctoon_coverage
```

Per-language: `ctoon_coverage_c`, `ctoon_coverage_cpp`, `ctoon_coverage_go`,
`ctoon_coverage_julia`, `ctoon_coverage_matlab`, `ctoon_coverage_python`,
`ctoon_coverage_rust`, plus `ctoon_coverage_total` (merged C/C++/Python/Go/Rust/Julia
lcov report). Output lands in `build/coverage/`, with `build/coverage/index.html`
as the dashboard — published at
[mohammadraziei.github.io/ctoon/coverage](https://mohammadraziei.github.io/ctoon/coverage/index.html).
(Zig has no coverage target yet — see the comment in `tests/zig/CMakeLists.txt`.)

### Docs

```bash
cmake -B build -DCTOON_BUILD_DOCS=ON   # plus any CTOON_BUILD_<LANG>=ON you need
cmake --build build --target ctoon_docs
```

Per-language: `ctoon_docs_c`, `ctoon_docs_cpp`, `ctoon_docs_julia`,
`ctoon_docs_matlab`, `ctoon_docs_python`, `ctoon_docs_rust`, plus
`ctoon_docs_index` for the landing page (CLI and Zig are assembled straight
from Markdown, no external doc tool needed). Output lands in `build/docs/out/`.

---

## C API Reference

### Parsing

```c
/* from memory */
ctoon_doc *ctoon_read(const char *toon, size_t len, ctoon_read_flag flags);
ctoon_doc *ctoon_read_opts(char *toon, size_t len, ctoon_read_flag flags,
                            const ctoon_alc *alc, ctoon_read_err *err);
ctoon_doc *ctoon_read_opts_indent(char *toon, size_t len, ctoon_read_flag flags,
                                   int indent_size, const ctoon_alc *alc,
                                   ctoon_read_err *err);
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

## License

MIT — see [LICENSE](LICENSE).

## Related

- [toon-format/toon](https://github.com/toon-format/toon) — the TOON format specification
