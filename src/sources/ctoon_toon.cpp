#include "ctoon.h"
#include "utils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ctoon {

namespace {

constexpr char COLON = ':';
constexpr char SPACE = ' ';
constexpr char OPEN_BRACKET = '[';
constexpr char CLOSE_BRACKET = ']';
constexpr char OPEN_BRACE = '{';
constexpr char CLOSE_BRACE = '}';
constexpr char DOUBLE_QUOTE = '"';
constexpr char BACKSLASH = '\\';
constexpr char NEWLINE = '\n';

constexpr const char* NULL_LITERAL = "null";
constexpr const char* TRUE_LITERAL = "true";
constexpr const char* FALSE_LITERAL = "false";

std::string encodePrimitive(const Primitive& primitive, Delimiter delimiter) {
    // Use Primitive::asString() which already uses yyjson for number serialization
    std::string result = primitive.asString();
    
    // For strings, we still need to handle TOON-specific quoting
    if (primitive.isString()) {
        bool needsQuoting = result.empty() || result.front() == SPACE || result.back() == SPACE ||
                             result == TRUE_LITERAL || result == FALSE_LITERAL || result == NULL_LITERAL;

        const char activeDelimiter = static_cast<char>(delimiter);
        if (!needsQuoting) {
            needsQuoting = result.find(activeDelimiter) != std::string::npos ||
                           result.find(COLON) != std::string::npos ||
                           result.find(DOUBLE_QUOTE) != std::string::npos ||
                           result.find(BACKSLASH) != std::string::npos;
        }

        if (!needsQuoting) {
            return result;
        }

        std::string escaped;
        escaped += DOUBLE_QUOTE;
        for (char c : result) {
            if (c == DOUBLE_QUOTE || c == BACKSLASH) {
                escaped += BACKSLASH;
            }
            escaped += c;
        }
        escaped += DOUBLE_QUOTE;
        return escaped;
    }
    
    return result;
}

std::string encodeAndJoinPrimitives(const std::vector<Primitive>& primitives, Delimiter delimiter) {
    std::vector<std::string> encoded;
    encoded.reserve(primitives.size());
    for (const auto& primitive : primitives) {
        encoded.push_back(encodePrimitive(primitive, delimiter));
    }

    std::ostringstream oss;
    const char delimChar = static_cast<char>(delimiter);
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (i > 0) {
            oss << delimChar;
        }
        oss << encoded[i];
    }
    return oss.str();
}

bool isArrayOfPrimitives(const Array& array) {
    return std::all_of(array.begin(), array.end(), [](const Value& value) {
        return value.isPrimitive();
    });
}

bool isArrayOfObjects(const Array& array) {
    return std::all_of(array.begin(), array.end(), [](const Value& value) {
        return value.isObject();
    });
}

bool collectUniformObjectFields(const Array& array, std::vector<std::string>& fields) {
    if (array.empty()) {
        return true;
    }

    const Object& firstObj = array.front().asObject();
    fields.reserve(firstObj.size());
    for (const auto& [field, _] : firstObj) {
        fields.push_back(field);
    }

    for (size_t i = 1; i < array.size(); ++i) {
        const Object& obj = array[i].asObject();
        if (obj.size() != fields.size()) {
            return false;
        }

        for (const auto& field : fields) {
            if (obj.find(field) == obj.end()) {
                return false;
            }
        }
    }

    return true;
}

std::string encodeValue(const std::string& key, const Value& value, const EncodeOptions& options, int depth);

std::string encodeNonUniformArrayOfObjects(const std::string& key,
                                           const Array& array,
                                           const EncodeOptions& options,
                                           int depth) {
    std::ostringstream oss;
    oss << key << OPEN_BRACKET << array.size() << CLOSE_BRACKET << COLON << NEWLINE;

    const std::string itemIndent((depth + 1) * options.indent, SPACE);
    const std::string fieldIndent = itemIndent + std::string(options.indent, SPACE);

    for (size_t index = 0; index < array.size(); ++index) {
        const auto& item = array[index];
        oss << itemIndent << "-";

        if (item.isObject()) {
            const Object& obj = item.asObject();
            bool firstField = true;
            for (const auto& [field, fieldValue] : obj) {
                if (firstField) {
                    if (fieldValue.isPrimitive()) {
                        oss << SPACE << field << COLON << SPACE
                            << encodePrimitive(fieldValue.asPrimitive(), options.delimiter);
                    } else {
                        oss << NEWLINE;
                        oss << fieldIndent
                            << encodeValue(field, fieldValue, options, depth + 1);
                    }
                    firstField = false;
                    continue;
                }

                oss << NEWLINE;
                oss << fieldIndent
                    << encodeValue(field, fieldValue, options, depth + 1);
            }

            if (firstField) {
                oss << NEWLINE;
            }
        }

        if (index + 1 < array.size()) {
            oss << NEWLINE;
        }
    }

    return oss.str();
}

std::string encodeArrayOfPrimitives(const std::string& key, const Array& array, const EncodeOptions& options) {
    std::ostringstream oss;
    oss << key << OPEN_BRACKET << array.size() << CLOSE_BRACKET << COLON;

    std::vector<Primitive> primitives;
    primitives.reserve(array.size());
    for (const auto& value : array) {
        primitives.push_back(value.asPrimitive());
    }

    oss << SPACE << encodeAndJoinPrimitives(primitives, options.delimiter);
    return oss.str();
}

std::string encodeArrayOfObjects(const std::string& key, const Array& array, const EncodeOptions& options, int depth);
std::string encodeObject(const Object& obj, const EncodeOptions& options, int depth);

std::string encodeValue(const std::string& key, const Value& value, const EncodeOptions& options, int depth) {
    if (value.isPrimitive()) {
        return key + COLON + SPACE + encodePrimitive(value.asPrimitive(), options.delimiter);
    }

    if (value.isArray()) {
        const Array& array = value.asArray();
        if (array.empty()) {
            return key + "[0]{}:";
        }

        if (isArrayOfPrimitives(array)) {
            return encodeArrayOfPrimitives(key, array, options);
        }

        if (isArrayOfObjects(array)) {
            return encodeArrayOfObjects(key, array, options, depth);
        }

        std::ostringstream oss;
        oss << key << OPEN_BRACKET << array.size() << CLOSE_BRACKET << COLON << NEWLINE;
        for (const auto& item : array) {
            oss << std::string((depth + 1) * options.indent, SPACE)
                << encodeValue("", item, options, depth + 1) << NEWLINE;
        }
        return oss.str();
    }

    if (value.isObject()) {
        const Object& obj = value.asObject();
        if (obj.empty()) {
            return key + COLON;
        }

        std::ostringstream oss;
        oss << key << COLON << NEWLINE;
        oss << encodeObject(obj, options, depth + 1);
        return oss.str();
    }

    return key + COLON + SPACE + NULL_LITERAL;
}

std::string encodeArrayOfObjects(const std::string& key, const Array& array, const EncodeOptions& options, int depth) {
    if (array.empty()) {
        return key + "[0]{}:\n";
    }

    std::vector<std::string> fields;
    if (!collectUniformObjectFields(array, fields)) {
        return encodeNonUniformArrayOfObjects(key, array, options, depth);
    }

    std::ostringstream oss;
    oss << key << OPEN_BRACKET << array.size() << CLOSE_BRACKET << OPEN_BRACE;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            oss << static_cast<char>(options.delimiter);
        }
        oss << fields[i];
    }
    oss << CLOSE_BRACE << COLON << NEWLINE;

    bool first = true;
    for (const auto& item : array) {
        if (!item.isObject()) {
            continue;
        }

        const Object& obj = item.asObject();
        std::vector<Primitive> values;
        values.reserve(fields.size());
        for (const auto& field : fields) {
            const auto it = obj.find(field);
            if (it != obj.end() && it->second.isPrimitive()) {
                values.push_back(it->second.asPrimitive());
            } else {
                values.push_back(nullptr);
            }
        }
        if (!first) {
            oss << NEWLINE;
        }
        first = false;
        // Ensure all array items are indented with proper depth
        oss << std::string((depth + 1) * options.indent, SPACE) << encodeAndJoinPrimitives(values, options.delimiter);
    }

    return oss.str();
}

std::string encodeObject(const Object& obj, const EncodeOptions& options, int depth) {
    std::ostringstream oss;
    const std::string indent(depth * options.indent, SPACE);

    // The ordered_map preserves insertion order, so we can just iterate normally
    bool first = true;
    for (const auto& [key, value] : obj) {
        if (!first) {
            oss << NEWLINE;
        }
        oss << indent << encodeValue(key, value, options, depth);
        first = false;
    }

    return oss.str();
}

} // namespace

std::string encodeInternal(const Value& value, const EncodeOptions& options) {
    if (value.isPrimitive()) {
        return encodePrimitive(value.asPrimitive(), options.delimiter);
    }

    if (value.isArray()) {
        return encodeValue("", value, options, 0);
    }

    if (value.isObject()) {
        return encodeObject(value.asObject(), options, 0);
    }

    return NULL_LITERAL;
}

Value decode(const std::string& input, bool strict) {
    if (input.empty()) {
        return Object{};
    }

    if (input == TRUE_LITERAL) {
        return Value(true);
    }
    if (input == FALSE_LITERAL) {
        return Value(false);
    }
    if (input == NULL_LITERAL) {
        return Value(nullptr);
    }

    try {
        size_t pos = 0;
        const double num = std::stod(input, &pos);
        if (pos == input.length()) {
            return Value(num);
        }
    } catch (...) {
    }

    return Value(input);
}

void encodeToFile(const Value& value, const std::string& outputFile, const EncodeOptions& options) {
    writeStringToFile(encodeInternal(value, options), outputFile);
}

Value decodeFromFile(const std::string& inputFile, bool strict) {
    return decode(readStringFromFile(inputFile), strict);
}

// Main TOON API implementation
std::string encode(const Value& value, const EncodeOptions& options) {
    return encodeInternal(value, options);
}

Value decode(const std::string& input, const DecodeOptions& options) {
    // TODO: Implement proper TOON decoding with options
    // For now, use the basic decode with strict option
    return decode(input, options.strict);
}

// TOON functions implementation (legacy API)
Value loadToon(const std::string& filename, bool strict) {
    return decodeFromFile(filename, strict);
}

Value loadsToon(const std::string& toonString, bool strict) {
    return decode(toonString, strict);
}

std::string dumpsToon(const Value& value, const EncodeOptions& options) {
    return encode(value, options);
}

void dumpToon(const Value& value, const std::string& filename, const EncodeOptions& options) {
    encodeToFile(value, filename, options);
}

} // namespace ctoon
