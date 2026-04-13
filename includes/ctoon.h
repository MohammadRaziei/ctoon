#pragma once

#define CTOON_VERSION_MAJOR 0
#define CTOON_VERSION_MINOR 1
#define CTOON_VERSION_PATCH 0

#define CTOON_VERSION_ENCODE(major, minor, patch) (((major) * 10000) + ((minor) * 100) + ((patch) * 1))
#define CTOON_VERSION CTOON_VERSION_ENCODE(CTOON_VERSION_MAJOR, CTOON_VERSION_MINOR, CTOON_VERSION_PATCH)

#define CTOON_VERSION_XSTRINGIZE(major, minor, patch) #major"."#minor"."#patch
#define CTOON_VERSION_STRINGIZE(major, minor, patch) CTOON_VERSION_XSTRINGIZE(major, minor, patch)
#define CTOON_VERSION_STRING CTOON_VERSION_STRINGIZE(CTOON_VERSION_MAJOR, CTOON_VERSION_MINOR, CTOON_VERSION_PATCH)

#include <string>
#include <variant>
#include <vector>
#include <memory>
#include <optional>

#include "ordered_map.h"

namespace ctoon {

// Version class with static methods for version information
class Version {
public:
    // Get major version number
    static constexpr int major() noexcept { return CTOON_VERSION_MAJOR; }
    
    // Get minor version number
    static constexpr int minor() noexcept { return CTOON_VERSION_MINOR; }
    
    // Get patch version number
    static constexpr int patch() noexcept { return CTOON_VERSION_PATCH; }
    
    // Get encoded version number (major * 10000 + minor * 100 + patch)
    static constexpr int encoded() noexcept { return CTOON_VERSION; }
    
    // Get version string (e.g., "0.1.0")
    static std::string string() noexcept { return CTOON_VERSION_STRING; }
    
    // Check if version is at least the specified version
    static constexpr bool isAtLeast(int major, int minor = 0, int patch = 0) noexcept {
        return encoded() >= CTOON_VERSION_ENCODE(major, minor, patch);
    }
    
    // Compare with another version
    static constexpr int compare(int otherMajor, int otherMinor = 0, int otherPatch = 0) noexcept {
        int otherEncoded = CTOON_VERSION_ENCODE(otherMajor, otherMinor, otherPatch);
        return (encoded() > otherEncoded) ? 1 : (encoded() < otherEncoded) ? -1 : 0;
    }
    
    // Check if this version equals another
    static constexpr bool equals(int otherMajor, int otherMinor = 0, int otherPatch = 0) noexcept {
        return encoded() == CTOON_VERSION_ENCODE(otherMajor, otherMinor, otherPatch);
    }
};

// Format types enum
enum class Type {
    JSON,
    TOON,
    UNKNOWN
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

struct EncodeOptions {
    int indent = 2;
    Delimiter delimiter = Delimiter::Comma;
    bool lengthMarker = false;

    EncodeOptions() = default;
    EncodeOptions(int indent) : indent(std::max(0, indent)) {}

    // Builder pattern methods
    EncodeOptions& setIndent(int indent) { this->indent = std::max(0, indent); return *this; }
    EncodeOptions& setDelimiter(Delimiter delimiter) { this->delimiter = delimiter; return *this; }
    EncodeOptions& setLengthMarker(bool lengthMarker) { this->lengthMarker = lengthMarker; return *this; }
};

struct DecodeOptions {
    bool strict = true;
    int indent = 2;

    DecodeOptions() = default;
    DecodeOptions(bool strict) : strict(strict) {}

    // Builder pattern methods
    DecodeOptions& setStrict(bool strict) { this->strict = strict; return *this; }
    DecodeOptions& setIndent(int indent) { this->indent = std::max(0, indent); return *this; }
};

// Utility functions
bool isPrimitive(const Value& value);
bool isObject(const Value& value);
bool isArray(const Value& value);

// JSON functions
Value loadJson(const std::string& filename);
Value loadsJson(const std::string& jsonString);
std::string dumpsJson(const Value& value, int indent = 2);
void dumpJson(const Value& value, const std::string& filename, int indent = 2);

// TOON functions (legacy API - for backward compatibility)
Value loadToon(const std::string& filename, bool strict = true);
Value loadsToon(const std::string& toonString, bool strict = true);
std::string dumpsToon(const Value& value, const EncodeOptions& options = {});
void dumpToon(const Value& value, const std::string& filename, const EncodeOptions& options = {});

// Main TOON API (matching reference implementation)
std::string encode(const Value& value, const EncodeOptions& options = {});
Value decode(const std::string& input, const DecodeOptions& options = {});
void encodeToFile(const Value& value, const std::string& outputFile, const EncodeOptions& options = {});
Value decodeFromFile(const std::string& inputFile, const DecodeOptions& options = {});

// Generic file format functions (auto-detect format from file extension)
Value load(const std::string& filename);
void dump(const Value& value, const std::string& filename);

// Format-specific functions with explicit format type
Value loads(const std::string& content, Type format);
std::string dumps(const Value& value, Type format, int indent = 2);

} // namespace ctoon
