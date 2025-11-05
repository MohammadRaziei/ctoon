# Ctoon - C++ Implementation of TOON

<div align="center">
  <div style="display: flex; align-items: center; justify-content: center; gap: 40px;">
    <div style="flex: 0 0 auto;">
      <img src="docs/images/ctoon.png" alt="Ctoon Logo" width="130" >
    </div>
    <div style="flex: 1;">
      <p><strong>Ctoon</strong> is a C++ and Python implementation of the <a href="https://github.com/toon-format/toon">TOON format</a> (Token-Oriented Object Notation), a serialization library that supports multiple formats like JSON and TOON, and can convert data between formats.</p>
      <p>This project is written using the C++17 standard and is efficient and fast.</p>
    </div>
  </div>
</div>

## 🚀 Features

- **Multiple Format Support**: JSON and TOON
- **Bidirectional Conversion**: Convert data between supported formats
- **High Performance**: Optimized with C++17 for fast performance
- **Powerful CLI**: Command-line tool for file conversion
- **TOON Integration**: Full TOON format support for token efficiency in LLM

## 📦 Installation

### C++

```bash
# Clone the repository
git clone https://github.com/MohammadRaziei/ctoon.git
cd ctoon

# Build the project
mkdir build && cd build
cmake ..
make

# Install
sudo make install
```


## 🎯 Quick Start

### Using C++

```cpp
#include "ctoon.h"
#include <iostream>

int main() {
    // Create sample data
    ctoon::Object data;
    data["name"] = ctoon::Value("Test User");
    data["age"] = ctoon::Value(30.0);
    data["active"] = ctoon::Value(true);
    
    ctoon::Array tags;
    tags.push_back(ctoon::Value("programming"));
    tags.push_back(ctoon::Value("c++"));
    tags.push_back(ctoon::Value("serialization"));
    data["tags"] = ctoon::Value(tags);
    
    ctoon::Value value(data);

    // Serialize to JSON
    std::string jsonStr = ctoon::dumpsJson(value);
    std::cout << "JSON: " << jsonStr << std::endl;
    
    // Serialize to TOON
    std::string toonStr = ctoon::dumpsToon(value);
    std::cout << "TOON:" << std::endl;
    std::cout << toonStr << std::endl;
    
    return 0;
}
```


## 🔧 Command Line Interface (CLI)

Ctoon includes a powerful command-line tool for file conversion:

```bash
# Show help and available options
ctoon --help

# Show the current Ctoon version
ctoon --version

# Convert JSON to TOON and write to a file (format inferred from extension)
ctoon input.json -o output.toon

# Output a conversion directly to the terminal (defaults to TOON)
ctoon input.json

# Select an explicit output format when streaming to stdout
ctoon input.toon -t json

# Control indentation for structured formats
ctoon data.toon -t json -i 4
```

## 📊 TOON Format

![TOON logo with step‑by‑step guide](docs/images/og.png)

**Token-Oriented Object Notation** is a compact, human-readable serialization format designed for passing structured data to Large Language Models with significantly reduced token usage. It's intended for LLM input, not output.

TOON's sweet spot is **uniform arrays of objects** – multiple fields per row, same structure across items. It borrows YAML's indentation-based structure for nested objects and CSV's tabular format for uniform data rows, then optimizes both for token efficiency in LLM contexts. For deeply nested or non-uniform data, JSON may be more efficient.

> [!TIP]
> Think of TOON as a translation layer: use JSON programmatically, convert to TOON for LLM input.

### Why TOON?

AI is becoming cheaper and more accessible, but larger context windows allow for larger data inputs as well. **LLM tokens still cost money** – and standard JSON is verbose and token-expensive:

```json
{
  "users": [
    { "id": 1, "name": "Alice", "role": "admin" },
    { "id": 2, "name": "Bob", "role": "user" }
  ]
}
```

TOON conveys the same information with **fewer tokens**:

```
users[2]{id,name,role}:
  1,Alice,admin
  2,Bob,user
```

### TOON Advantages

- **💸 Token Efficiency**: Typically 30-60% fewer tokens than JSON
- **🤿 LLM Guidance**: Explicit lengths and field lists help models
- **🍱 Minimal Syntax**: Eliminates repetitive punctuation
- **📐 Indentation-based Structure**: Replaces braces with whitespace
- **🧺 Tabular Arrays**: Declare keys once, stream rows


### Format Overview

#### Objects

Simple objects with primitive values:

```
id: 123
name: Ada
active: true
```

Nested objects:

```
user:
  id: 123
  name: Ada
```

#### Arrays

**Primitive Arrays (Inline):**
```
tags[3]: admin,ops,dev
```

**Arrays of Objects (Tabular):**
When all objects share the same primitive fields, TOON uses an efficient **tabular format**:

```
items[2]{sku,qty,price}:
  A1,2,9.99
  B2,1,14.5
```

**Mixed and Non-Uniform Arrays:**
Arrays that don't meet the tabular requirements use list format:

```
items[3]:
  - 1
  - a: 1
  - text
```

### Using TOON in LLM Prompts

TOON works best when you show the format instead of describing it. The structure is self-documenting – models parse it naturally once they see the pattern.

#### Sending TOON to LLMs (Input)

Wrap your encoded data in a fenced code block (label it ````toon` for clarity). The indentation and headers are usually enough – models treat it like familiar YAML or CSV. The explicit length markers (`[N]`) and field headers (`{field1,field2}`) help the model track structure, especially for large tables.

#### Generating TOON from LLMs (Output)

For output, be more explicit. When you want the model to **generate** TOON:

- **Show the expected header** (`users[N]{id,name,role}:`). The model fills rows instead of repeating keys, reducing generation errors.
- **State the rules:** 2-space indent, no trailing spaces, `[N]` matches row count.

Here's a prompt that works for both reading and generating:

````
Data is in TOON format (2-space indent, arrays show length and fields).

```toon
users[3]{id,name,role,lastLogin}:
  1,Alice,admin,2025-01-15T10:30:00Z
  2,Bob,user,2025-01-14T15:22:00Z
  3,Charlie,user,2025-01-13T09:45:00Z
```

Task: Return only users with role "user" as TOON. Use the same header. Set [N] to match the row count. Output only the code block.
````

> [!TIP]
> For large uniform tables, use tab delimiters and tell the model "fields are tab-separated." Tabs often tokenize better than commas and reduce the need for quote-escaping.

## 🧪 Examples

The project includes multiple examples for quick start:

- `examples/example_basic.cpp` - Basic examples
- `examples/sample4_serialization.cpp` - Serialization functions
- `examples/example_tabular.cpp` - Tabular data

## 📚 API

### Main Functions

- `loadJson(filename)` / `loadsJson(string)` - Load JSON
- `dumpJson(value, filename)` / `dumpsJson(value)` - Save JSON
- `loadToon(filename)` / `loadsToon(string)` - Load TOON
- `dumpToon(value, filename)` / `dumpsToon(value)` - Save TOON

### Data Structures

- `ctoon::Value` - Main data type
- `ctoon::Object` - Object (dictionary)
- `ctoon::Array` - Array
- `ctoon::Primitive` - Primitive values (string, number, boolean, null)
- `ctoon::ToonOptions` - Configure TOON serialization (indentation, delimiter, strict mode)

### TOON Configuration

```cpp
ctoon::ToonOptions options;
options.setIndent(4).setDelimiter(ctoon::Delimiter::Pipe);

auto toon = ctoon::dumpsToon(value, options);
```

To confirm that Ctoon's TOON output matches the reference implementation, run the C++ tests with
`npx` available. The `TOON output matches @byjohann/toon when available` test will invoke
`npx @byjohann/toon` to compare the serialized output whenever the package can be downloaded.

## 🧪 Tests

```bash
# Run tests
cd build
make test

# Or directly
./test_ctoon
```

## 🤝 Contribution

Contributions are always welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Open a pull request

## 📄 License

This project is released under the MIT License.

## 👤 Developer

- **Mohammad Raziei** - [GitHub](https://github.com/MohammadRaziei)

## 🙌 Acknowledgments

- [TOON](https://github.com/byjohann/toon) project for inspiration
- Open source community for tools and libraries

---

<p align="center">
  Made with ❤️ in Iran
</p>
