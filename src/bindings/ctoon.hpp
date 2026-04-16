#ifndef CTOON_HPP
#define CTOON_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include "../../include/ctoon/ctoon.h"

namespace ctoon {

class Value;

class Document {
private:
    std::shared_ptr<::ctoon_doc> doc_;
    
public:
    Document() : doc_(nullptr) {}
    
    Document(const std::string& data, size_t max_memory = 0) {
        doc_.reset(::ctoon_read_toon(data.c_str(), data.size(), max_memory),
                   ::ctoon_doc_free);
        if (!doc_) {
            throw std::runtime_error("Failed to parse TOON data");
        }
    }
    
    Document(const char* data, size_t len, size_t max_memory = 0) {
        doc_.reset(::ctoon_read_toon(data, len, max_memory),
                   ::ctoon_doc_free);
        if (!doc_) {
            throw std::runtime_error("Failed to parse TOON data");
        }
    }
    
    static Document fromFile(const std::string& path, size_t max_memory = 0) {
        Document doc;
        doc.doc_.reset(::ctoon_read_toon_file(path.c_str(), max_memory),
                       ::ctoon_doc_free);
        if (!doc.doc_) {
            throw std::runtime_error("Failed to parse TOON file: " + path);
        }
        return doc;
    }
    
    Value root() const;
    
    bool hasError() const {
        return ::ctoon_get_error(doc_.get()) != nullptr;
    }
    
    std::string error() const {
        const char* err = ::ctoon_get_error(doc_.get());
        return err ? std::string(err) : "";
    }
    
    size_t errorPos() const {
        return ::ctoon_get_error_pos(doc_.get());
    }
    
    operator bool() const {
        return doc_ != nullptr;
    }
};

class Value {
private:
    ::ctoon_val* val_;
    
public:
    Value() : val_(nullptr) {}
    Value(::ctoon_val* val) : val_(val) {}
    
    bool isNull() const { return val_ && ::ctoon_is_null(val_); }
    bool isBool() const { return val_ && ::ctoon_is_bool(val_); }
    bool isTrue() const { return val_ && ::ctoon_is_true(val_); }
    bool isFalse() const { return val_ && ::ctoon_is_false(val_); }
    bool isNumber() const { return val_ && ::ctoon_is_number(val_); }
    bool isString() const { return val_ && ::ctoon_is_string(val_); }
    bool isArray() const { return val_ && ::ctoon_is_array(val_); }
    bool isObject() const { return val_ && ::ctoon_is_object(val_); }
    
    bool asBool() const {
        if (!isBool()) throw std::runtime_error("Value is not a boolean");
        return isTrue();
    }
    
    int asInt() const {
        if (!isNumber()) throw std::runtime_error("Value is not a number");
        return ::ctoon_get_int(val_);
    }
    
    double asDouble() const {
        if (!isNumber()) throw std::runtime_error("Value is not a number");
        return ::ctoon_get_double(val_);
    }
    
    std::string asString() const {
        if (!isString()) throw std::runtime_error("Value is not a string");
        const char* str = ::ctoon_get_str(val_);
        return str ? std::string(str) : "";
    }
    
    size_t size() const {
        if (isArray()) {
            return ::ctoon_arr_size(val_);
        } else if (isObject()) {
            return ::ctoon_obj_size(val_);
        }
        return 0;
    }
    
    Value operator[](size_t index) const {
        if (!isArray()) throw std::runtime_error("Value is not an array");
        return Value(::ctoon_arr_get(val_, index));
    }
    
    Value operator[](const std::string& key) const {
        if (!isObject()) throw std::runtime_error("Value is not an object");
        return Value(::ctoon_obj_get(val_, key.c_str()));
    }
    
    bool has(const std::string& key) const {
        if (!isObject()) return false;
        return ::ctoon_obj_get(val_, key.c_str()) != nullptr;
    }
    
    std::vector<std::string> keys() const {
        std::vector<std::string> result;
        if (!isObject()) return result;
        
        size_t sz = size();
        for (size_t i = 0; i < sz; i++) {
            // Note: This is a simplified implementation
            // A real implementation would need to iterate through keys
        }
        return result;
    }
    
    std::vector<Value> values() const {
        std::vector<Value> result;
        if (!isArray()) return result;
        
        size_t sz = size();
        for (size_t i = 0; i < sz; i++) {
            result.push_back(operator[](i));
        }
        return result;
    }
    
    operator bool() const {
        return val_ != nullptr;
    }
};

inline Value Document::root() const {
    return Value(::ctoon_doc_root(doc_.get()));
}

// Helper functions
inline Document parse(const std::string& data) {
    return Document(data);
}

inline Document parseFile(const std::string& path) {
    return Document::fromFile(path);
}

} // namespace ctoon

#endif // CTOON_HPP