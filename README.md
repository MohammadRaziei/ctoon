# CTOON - C++ TOON Format Library
<img src="docs/images/ctoon.png" width=200 />

A modern C++ serialization library for the TOON format that provides bidirectional conversion between JSON and TOON formats.

## Features

- **Bidirectional Serialization**: Convert JSON to TOON and vice versa
- **Data Type Support**: Strings, numbers, booleans, null, arrays, and objects
- **Configurable Formatting**: Multiple delimiters (comma, tab, pipe) and indentation sizes
- **File Processing**: Direct reading and writing from/to files
- **CLI Tool**: Command-line utility for file conversion

## Installation & Compilation

```bash
mkdir build && cd build
cmake ..
make -j4
```

## Core API

### Encode Functions (Convert to TOON)

#### `std::string encode(const Value& value, const EncodeOptions& options)`
Convert a value to TOON string.

```cpp
#include "ctoon.h"

ctoon::Object obj;
obj["name"] = ctoon::Value("Ali");
obj["age"] = ctoon::Value(25);

ctoon::EncodeOptions options;
options.indent = 2;
options.delimiter = ctoon::Delimiter::Comma;

std::string toon = ctoon::encode(ctoon::Value(obj), options);
// Output: name: Ali\nage: 25
```

#### `std::string dumpsToon(const Value& value, const EncodeOptions& options)`
Legacy function for compatibility - similar to `encode`.

#### `void encodeToFile(const Value& value, const std::string& filename, const EncodeOptions& options)`
Save value directly to TOON file.

```cpp
ctoon::encodeToFile(value, "output.toon", options);
```

#### `void dumpToon(const Value& value, const std::string& filename, const EncodeOptions& options)`
Legacy function for compatibility - similar to `encodeToFile`.

### Decode Functions (Convert from TOON)

#### `Value decode(const std::string& input, const DecodeOptions& options)`
Convert TOON string to data structure.

```cpp
#include "ctoon.h"

std::string toonText = "name: Ali\nage: 25";

ctoon::DecodeOptions options;
options.strict = true;

ctoon::Value value = ctoon::decode(toonText, options);
const auto& obj = value.asObject();
std::string name = obj.at("name").asPrimitive().getString();
// name = "Ali"
```

#### `Value decode(const std::string& input, bool strict)`
Simplified version of decode function.

#### `Value loadsToon(const std::string& toonString, bool strict)`
Legacy function for compatibility - similar to `decode`.

#### `Value loadToon(const std::string& filename, bool strict)`
Read directly from TOON file and return text content.

```cpp
ctoon::Value toonContent = ctoon::loadToon("input.toon", true);
std::string toonText = toonContent.asPrimitive().getString();
```

#### `Value decodeFromFile(const std::string& filename, bool strict)`
Read from file and parse into data structure.

```cpp
ctoon::Value parsedData = ctoon::decodeFromFile("input.toon", true);
```

## Encode Options

```cpp
struct EncodeOptions {
    int indent = 2;                    // Indentation size
    Delimiter delimiter = Delimiter::Comma;  // Array delimiter
};
```

### Delimiter Types

```cpp
enum class Delimiter {
    Comma = ',',    // Comma separator
    Tab = '\t',     // Tab separator  
    Pipe = '|'      // Pipe separator
};
```

## Decode Options

```cpp
struct DecodeOptions {
    bool strict = true;  // Strict mode for parsing
};
```

## Usage Examples

### Example 1: Basic Conversion

```cpp
#include "ctoon.h"

// Create data
ctoon::Object data;
data["name"] = ctoon::Value("Mohammad");
data["age"] = ctoon::Value(30);
data["active"] = ctoon::Value(true);

ctoon::Array tags;
tags.push_back(ctoon::Value("programming"));
tags.push_back(ctoon::Value("C++"));
tags.push_back(ctoon::Value("serialization"));
data["tags"] = ctoon::Value(tags);

// Convert to TOON
ctoon::EncodeOptions options;
options.indent = 2;
std::string toon = ctoon::encode(ctoon::Value(data), options);

std::cout << "TOON Output:\n" << toon << std::endl;
```

Output:
```
name: Mohammad
age: 30
active: true
tags[3]: programming,C++,serialization
```

### Example 2: Array of Objects

```cpp
ctoon::Object root;
ctoon::Array users;

// First user
ctoon::Object user1;
user1["id"] = ctoon::Value(1);
user1["name"] = ctoon::Value("Sara");
user1["role"] = ctoon::Value("admin");
users.push_back(ctoon::Value(user1));

// Second user  
ctoon::Object user2;
user2["id"] = ctoon::Value(2);
user2["name"] = ctoon::Value("Reza");
user2["role"] = ctoon::Value("user");
users.push_back(ctoon::Value(user2));

root["users"] = ctoon::Value(users);

// Convert with tab delimiter
ctoon::EncodeOptions options;
options.delimiter = ctoon::Delimiter::Tab;
std::string toon = ctoon::encode(ctoon::Value(root), options);
```

Output:
```
users[2]{id	name	role}:
  1	Sara	admin
  2	Reza	user
```

### Example 3: Reading and Parsing

```cpp
// Read from file
ctoon::Value parsed = ctoon::decodeFromFile("data.toon", true);

if (parsed.isObject()) {
    const auto& obj = parsed.asObject();
    
    if (obj.find("users") != obj.end() && obj.at("users").isArray()) {
        const auto& users = obj.at("users").asArray();
        
        for (const auto& user : users) {
            if (user.isObject()) {
                const auto& userObj = user.asObject();
                std::string name = userObj.at("name").asPrimitive().getString();
                int age = userObj.at("age").asPrimitive().getInt();
                std::cout << "User: " << name << ", Age: " << age << std::endl;
            }
        }
    }
}
```

### Example 4: CLI Usage

```bash
# Convert JSON to TOON
./ctoon input.json -o output.toon

# Convert TOON to JSON
./ctoon input.toon -t json -o output.json

# Convert with 4-space indentation
./ctoon input.toon -t json -i 4

# Convert with pipe delimiter
./ctoon input.json --delimiter "|" -o output.toon
```

## TOON Format Structure

### Basic Format

```
key: value
nested key:
  subkey: value
array[3]: value1,value2,value3
object array[2]{field1,field2}:
  value1,value2
  value3,value4
```

### Value Types

- **String**: `name: "Ali"` or `name: Ali` (if quoting not needed)
- **Number**: `age: 25` or `score: 95.5`
- **Boolean**: `active: true` or `active: false`
- **Null**: `value: null`
- **Array**: `tags[3]: programming,C++,serialization`
- **Object**: Nested with indentation

### Array Formats

**Primitive Arrays:**
```
tags[3]: red,blue,green
```

**Object Arrays (Tabular):**
```
users[2]{id,name,role}:
  1,Alice,admin
  2,Bob,user
```

**Object Arrays (Nested):**
```
users[2]:
  id: 1
  name: Alice
  role: admin
  id: 2
  name: Bob  
  role: user
```

## Testing

Run the test suite:

```bash
cd build && ./ctoon_tests
```

Run examples:

```bash
cd build && ./example_basic
cd build && ./example_tabular
```

## Python Integration

The project includes a Python implementation in the `toon-python/` directory with the same API design for cross-language compatibility.

## License

MIT License - See LICENSE file for details.
