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

CToon is a high-performance C++ serialization library that provides fast, bidirectional conversion between JSON and TOON formats. It features a modern API, configurable output formatting, and Python bindings through nanobind.

## ✨ Features

### Core Library
- **JSON & TOON support** – Full bidirectional serialization for both formats
- **Modern C++17 API** – Type-safe interface with RAII semantics
- **Flexible formatting** – Configurable indentation, delimiters, and encoding options
- **CLI tool** – Command-line utility for batch conversion and processing
- **Cross-platform** – Works on Windows, Linux, and macOS
- **Python bindings** – Full Python API via nanobind integration

### Command-Line Interface
- **Format conversion** – Convert between JSON and TOON
- **Batch processing** – Process multiple files at once
- **Configurable output** – Control indentation and delimiter choices

## 🚀 Quick Start

### C++ Usage

```cpp
#include "ctoon.h"

int main() {
    // Create data
    ctoon::Object user;
    user["name"] = ctoon::Value("Ali");
    user["age"] = ctoon::Value(30);
    user["active"] = ctoon::Value(true);

    ctoon::Array tags;
    tags.push_back(ctoon::Value("developer"));
    tags.push_back(ctoon::Value("C++"));
    user["tags"] = ctoon::Value(tags);

    // Encode to JSON
    std::string json = ctoon::dumpsJson(ctoon::Value(user), 2);
    std::cout << json << std::endl;

    // Encode to TOON
    ctoon::EncodeOptions options;
    options.indent = 2;
    std::string toon = ctoon::encode(ctoon::Value(user), options);
    std::cout << toon << std::endl;

    // Decode from JSON
    std::string input = R"({"name": "Ali", "age": 30})";
    ctoon::Value parsed = ctoon::loadsJson(input);
    
    return 0;
}
```

### Command-Line Usage

```bash
# Convert JSON to TOON
ctoon input.json -o output.toon

# Convert TOON to JSON
ctoon input.toon -t json -o output.json

# Convert with 4-space indentation
ctoon input.toon -t json -i 4

# Convert with pipe delimiter
ctoon input.json --delimiter "|" -o output.toon
```

### Python Usage

```python
from ctoon import loads_json, dumps_json

# Load JSON
data = loads_json('{"name": "Ali", "age": 30}')

# Dump to JSON
json_str = dumps_json(data, indent=2)
```

## 📦 Installation

### Building from Source

```bash
git clone https://github.com/mohammadraziei/ctoon.git
cd ctoon
mkdir build && cd build
cmake .. -DCTOON_BUILD_EXAMPLES=ON -DCTOON_BUILD_TESTS=ON
make -j4
```

### With Python Bindings

```bash
pip install .
```

Or with scikit-build:

```bash
pip install scikit-build-core nanobind
pip install . -v
```

## 🔧 Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `CTOON_BUILD_EXAMPLES` | Build example programs | `ON` |
| `CTOON_BUILD_TESTS` | Build unit tests | `ON` |
| `CTOON_BUILD_PYTHON` | Build Python bindings | `ON` |
| `CTOON_BUILD_DOCS` | Build documentation with Doxygen | `OFF` |

## 📚 API Overview

### Core Types

#### `Value`
- `isPrimitive()` / `isObject()` / `isArray()` – Type checking
- `asPrimitive()` / `asObject()` / `asArray()` – Type-specific access

#### `Primitive`
- `isString()` / `isInt()` / `isDouble()` / `isBool()` / `isNull()` – Type checking
- `getString()` / `getInt()` / `getDouble()` / `getBool()` – Type-safe getters
- `asString()` – Convert to string representation

### JSON Functions
- `loadsJson()` – Parse JSON from string
- `dumpsJson()` – Serialize to JSON string
- `loadJson()` – Load JSON from file
- `dumpJson()` – Save to JSON file

### TOON Functions
- `encode()` – Encode value to TOON string
- `decode()` – Decode TOON string to value
- `encodeToFile()` – Encode and save to file
- `decodeFromFile()` – Load and decode from file

### Version Information
- `ctoon::Version::major()` – Major version number
- `ctoon::Version::minor()` – Minor version number
- `ctoon::Version::patch()` – Patch version number
- `ctoon::Version::string()` – Version string (e.g., "0.1.0")
- `ctoon::Version::isAtLeast(major, minor, patch)` – Version comparison

## 🧪 Testing

Run the test suite:

```bash
cd build && ctest --output-on-failure
```

Or run tests directly:

```bash
cd build && ./tests/cpp/test_ctoon_cpp
```

## 🎯 Use Cases

1. **Configuration files** – Store app settings in TOON format
2. **Data exchange** – Fast JSON/TOON serialization for APIs
3. **Data processing** – Efficient parsing of structured data
4. **CLI tools** – Quick format conversion for data pipelines
5. **Python integration** – Use C++ performance from Python code

## 📖 Documentation

- **API Reference**: Built with Doxygen (enable with `-DCTOON_BUILD_DOCS=ON`)
- **Examples**: See `examples/` directory
- **Test coverage**: `cmake --build build --target ctoon_coverage`

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## 📄 License

CToon is distributed under the **MIT License**. See [LICENSE](LICENSE) for details.

## 📞 Support

- **GitHub Issues**: https://github.com/mohammadraziei/ctoon/issues
- **Documentation**: https://mohammadraziei.github.io/ctoon

---

<div align="center">
<em>CToon – Fast, flexible serialization for JSON and TOON formats</em>
</div>
