#pragma once

#include <string>
#include <variant>
#include <vector>
#include <memory>
#include <optional>

#include "ordered_map.h"

namespace ctoon {

// Format types enum
enum class Type {
    JSON,
    TOON,
    YAML,
    UNKOWN
};

Type stringToType(const std::string & name);

// Forward declarations
struct Value;

// TOON value types
using Object = tsl::ordered_map<std::string, Value>;
using Array = std::vector<Value>;

struct Primitive: std::variant<std::string, double, int64_t, bool, std::nullptr_t> {
    using Base = std::variant<std::string, double, int64_t, bool, std::nullptr_t>;
    
    // Inherit constructors
    using Base::Base;
    
    // Type checking methods
    bool isString() const;
    bool isDouble() const;
    bool isInt() const;
    bool isBool() const;
    bool isNull() const;
    bool isNumber() const;
    
    // Getter methods with error checking
    const std::string& getString() const;
    double getDouble() const;
    int64_t getInt() const;
    bool getBool() const;
    std::nullptr_t getNull() const;
    double getNumber() const;
    
    // Conversion to string
    std::string asString() const;
};

struct Value {
    std::variant<Primitive, Object, Array> value;
    
    // Constructors
    Value() : value(nullptr) {}
    Value(const Primitive& p) : value(p) {}
    Value(const Object& o) : value(o) {}
    Value(const Array& a) : value(a) {}
    
    // Type checking
    bool isPrimitive() const { return std::holds_alternative<Primitive>(value); }
    bool isObject() const { return std::holds_alternative<Object>(value); }
    bool isArray() const { return std::holds_alternative<Array>(value); }
    
    // Getters
    const Primitive& asPrimitive() const { return std::get<Primitive>(value); }
    const Object& asObject() const { return std::get<Object>(value); }
    const Array& asArray() const { return std::get<Array>(value); }
    
    Primitive& asPrimitive() { return std::get<Primitive>(value); }
    Object& asObject() { return std::get<Object>(value); }
    Array& asArray() { return std::get<Array>(value); }
};

// Delimiter types
enum class Delimiter {
    Comma = ',',
    Tab = '\t',
    Pipe = '|'
};

struct EncoderOptions {
    int indent = 2;
    Delimiter delimiter = Delimiter::Comma;
    bool lengthMarker = false;

    EncoderOptions() = default;
    EncoderOptions(int indent) : indent(std::max(0, indent)) {}
};

// Utility functions
bool isPrimitive(const Value& value);
bool isObject(const Value& value);
bool isArray(const Value& value);

// Serialization functions (load/loads/dumps/dump)
// JSON functions
Value loadJson(const std::string& filename);
Value loadsJson(const std::string& jsonString);
std::string dumpsJson(const Value& value, int indent = 2);
void dumpJson(const Value& value, const std::string& filename, int indent = 2);

// TOON functions
Value loadToon(const std::string& filename, bool strict = true);
Value loadsToon(const std::string& toonString, bool strict = true);
std::string dumpsToon(const Value& value, const EncoderOptions& options = {});
void dumpToon(const Value& value, const std::string& filename, const EncoderOptions& options = {});

// YAML functions
Value loadYaml(const std::string& filename);
Value loadsYaml(const std::string& yamlString);
std::string dumpsYaml(const Value& value, int indent = 2);
void dumpYaml(const Value& value, const std::string& filename, int indent = 2);

// Generic file format functions (auto-detect format from file extension)
Value load(const std::string& filename);
void dump(const Value& value, const std::string& filename);

// Format-specific functions with explicit format type
Value loads(const std::string& content, Type format);
std::string dumps(const Value& value, Type format, int indent = 2);

} // namespace ctoon
