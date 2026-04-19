/*==============================================================================
 |  CToon C++ Binding
 |  Copyright (c) 2026 CToon Project, MIT License
 |
 |  Provides a modern C++17 RAII wrapper around the C ctoon API.
 *============================================================================*/

#ifndef CTOON_HPP
#define CTOON_HPP

#include "ctoon.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ctoon {

/* ============================================================
 * Forward declarations
 * ============================================================ */

class Document;
class Value;
class ArrayIterator;
class ObjectIterator;

/* ============================================================
 * Error type
 * ============================================================ */

/** Exception thrown on parse failures. */
class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string &msg, std::size_t pos = 0)
        : std::runtime_error(msg), pos_(pos) {}

    /** Byte offset in the source where the error occurred. */
    std::size_t position() const noexcept { return pos_; }

private:
    std::size_t pos_;
};

/** Exception thrown when a value is accessed as the wrong type. */
class TypeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/* ============================================================
 * Value – a non-owning view into a Document's arena
 * ============================================================ */

/**
 * @brief A non-owning, cheaply-copyable reference to a TOON value.
 *
 * Value objects are invalidated when the parent Document is destroyed.
 * They must NOT be stored beyond the Document's lifetime.
 */
class Value {
public:
    Value() noexcept : val_(nullptr) {}

    explicit Value(::ctoon_val *raw) noexcept : val_(raw) {}

    /** @return true if this Value is non-null (i.e. points to a real node). */
    explicit operator bool() const noexcept { return val_ != nullptr; }

    /** @return The underlying C handle (may be nullptr). */
    ::ctoon_val *raw() const noexcept { return val_; }

    /* --- type queries --------------------------------------------------- */

    bool isNull  () const noexcept { return val_ && ::ctoon_is_null  (val_); }
    bool isTrue  () const noexcept { return val_ && ::ctoon_is_true  (val_); }
    bool isFalse () const noexcept { return val_ && ::ctoon_is_false (val_); }
    bool isBool  () const noexcept { return val_ && ::ctoon_is_bool  (val_); }
    bool isNumber() const noexcept { return val_ && ::ctoon_is_num   (val_); }
    bool isUint  () const noexcept { return val_ && ::ctoon_is_uint  (val_); }
    bool isSint  () const noexcept { return val_ && ::ctoon_is_sint  (val_); }
    bool isInt   () const noexcept { return val_ && ::ctoon_is_int   (val_); }
    bool isReal  () const noexcept { return val_ && ::ctoon_is_real  (val_); }
    bool isString() const noexcept { return val_ && ::ctoon_is_str   (val_); }
    bool isArray () const noexcept { return val_ && ::ctoon_is_arr   (val_); }
    bool isObject() const noexcept { return val_ && ::ctoon_is_obj   (val_); }

    /* --- value getters -------------------------------------------------- */

    bool asBool() const {
        if (!isBool()) throw TypeError("Value is not a boolean");
        return ::ctoon_get_bool(val_);
    }

    std::uint64_t asUint() const {
        if (!isNumber()) throw TypeError("Value is not a number");
        return ::ctoon_get_uint(val_);
    }

    std::int64_t asSint() const {
        if (!isNumber()) throw TypeError("Value is not a number");
        return ::ctoon_get_sint(val_);
    }

    int asInt() const {
        if (!isNumber()) throw TypeError("Value is not a number");
        return ::ctoon_get_int(val_);
    }

    double asDouble() const {
        if (!isNumber()) throw TypeError("Value is not a number");
        return ::ctoon_get_real(val_);
    }

    std::string asString() const {
        if (!isString()) throw TypeError("Value is not a string");
        const char *p = ::ctoon_get_str(val_);
        std::size_t n = ::ctoon_get_len(val_);
        return std::string(p ? p : "", n);
    }

    std::string_view asStringView() const {
        if (!isString()) throw TypeError("Value is not a string");
        const char *p = ::ctoon_get_str(val_);
        std::size_t n = ::ctoon_get_len(val_);
        return std::string_view(p ? p : "", n);
    }

    /* --- array access --------------------------------------------------- */

    std::size_t size() const noexcept {
        if (!val_) return 0;
        if (isArray ()) return ::ctoon_arr_size(val_);
        if (isObject()) return ::ctoon_obj_size(val_);
        return 0;
    }

    /** Element access by index.  Returns empty Value on out-of-range. */
    Value operator[](std::size_t idx) const noexcept {
        if (!isArray()) return {};
        return Value(::ctoon_arr_get(val_, idx));
    }

    /** Element access by index; throws std::out_of_range if invalid. */
    Value at(std::size_t idx) const {
        if (!isArray()) throw TypeError("Value is not an array");
        if (idx >= size()) throw std::out_of_range("Array index out of range");
        return Value(::ctoon_arr_get(val_, idx));
    }

    /** Collect all array elements into a vector. */
    std::vector<Value> items() const {
        std::vector<Value> result;
        if (!isArray()) return result;
        std::size_t n = size();
        result.reserve(n);
        for (std::size_t i = 0; i < n; i++)
            result.emplace_back(::ctoon_arr_get(val_, i));
        return result;
    }

    /* --- object access -------------------------------------------------- */

    /** Lookup by key string (any char-convertible type). */
    Value operator[](const std::string &key) const noexcept {
        if (!isObject()) return {};
        return Value(::ctoon_obj_get(val_, key.c_str()));
    }

    /** @return false if not an object or key absent; does NOT throw. */
    bool has(const std::string &key) const noexcept {
        if (!isObject()) return false;
        return ::ctoon_obj_get(val_, key.c_str()) != nullptr;
    }

    /** Lookup by key; throws TypeError if not an object, returns nullopt if missing. */
    std::optional<Value> get(const std::string &key) const {
        if (!isObject()) throw TypeError("Value is not an object");
        ::ctoon_val *v = ::ctoon_obj_get(val_, key.c_str());
        if (!v) return std::nullopt;
        return Value(v);
    }

    /** Collect all object keys as strings. */
    std::vector<std::string> keys() const;

    /** Range-based for support for arrays. */
    ArrayIterator begin() const noexcept;
    ArrayIterator end  () const noexcept;

    /* --- equality ------------------------------------------------------- */

    bool operator==(std::string_view s) const noexcept {
        return isString() && asStringView() == s;
    }
    bool operator!=(std::string_view s) const noexcept { return !(*this == s); }

private:
    ::ctoon_val *val_;
};

/* ============================================================
 * ArrayIterator
 * ============================================================ */

class ArrayIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = Value;
    using difference_type   = std::ptrdiff_t;
    using pointer           = Value *;
    using reference         = Value;

    ArrayIterator(::ctoon_val *arr, std::size_t idx) noexcept
        : arr_(arr), idx_(idx) {}

    Value operator*() const noexcept {
        return Value(::ctoon_arr_get(arr_, idx_));
    }

    ArrayIterator &operator++() noexcept { ++idx_; return *this; }
    ArrayIterator  operator++(int) noexcept {
        ArrayIterator tmp = *this; ++*this; return tmp;
    }

    bool operator==(const ArrayIterator &o) const noexcept {
        return arr_ == o.arr_ && idx_ == o.idx_;
    }
    bool operator!=(const ArrayIterator &o) const noexcept {
        return !(*this == o);
    }

private:
    ::ctoon_val *arr_;
    std::size_t  idx_;
};

inline ArrayIterator Value::begin() const noexcept {
    return ArrayIterator(isArray() ? val_ : nullptr, 0);
}
inline ArrayIterator Value::end() const noexcept {
    return ArrayIterator(isArray() ? val_ : nullptr, size());
}

/* ============================================================
 * ObjectIterator
 * ============================================================ */

class ObjectEntry {
public:
    ObjectEntry(std::string_view key, Value value) noexcept
        : key_(key), value_(std::move(value)) {}

    std::string_view key()   const noexcept { return key_;   }
    const Value     &value() const noexcept { return value_; }

private:
    std::string_view key_;
    Value            value_;
};

inline std::vector<std::string> Value::keys() const {
    std::vector<std::string> result;
    if (!isObject()) return result;
    std::size_t n = ::ctoon_obj_size(val_);
    result.reserve(n);
    for (std::size_t i = 0; i < n; i++) {
        ::ctoon_val *kv = ::ctoon_obj_get_key_at(val_, i);
        if (kv) {
            const char *ks = ::ctoon_get_str(kv);
            if (ks) result.emplace_back(ks, ::ctoon_get_len(kv));
        }
    }
    return result;
}

/* ============================================================
 * Document – owns all TOON values
 * ============================================================ */

/**
 * @brief Owns a parsed TOON document and all values within it.
 *
 * Values returned by this document are valid only as long as the Document
 * object itself is alive.
 */
class Document {
public:
    Document() noexcept = default;

    /** Parse TOON from a string.  Throws ParseError on failure. */
    explicit Document(std::string_view data) {
        load(data.data(), data.size());
    }

    /** Parse TOON from raw bytes.  Throws ParseError on failure. */
    Document(const char *data, std::size_t len) {
        load(data, len);
    }

    Document(const Document &)             = delete;
    Document &operator=(const Document &)  = delete;
    Document(Document &&)                  = default;
    Document &operator=(Document &&)       = default;

    ~Document() = default;

    /* --- factory methods ----------------------------------------------- */

    /** Parse from file; throws ParseError on failure. */
    static Document fromFile(const std::string &path) {
        Document d;
        ::ctoon_doc *raw = ::ctoon_read_file(path.c_str());
        if (!raw) throw ParseError("Failed to read TOON file: " + path);
        d.doc_.reset(raw, ::ctoon_doc_free);
        return d;
    }

    /** Create an empty document for building values programmatically. */
    static Document create(std::size_t maxMemory = 0) {
        Document d;
        d.doc_.reset(::ctoon_doc_new(maxMemory), ::ctoon_doc_free);
        if (!d.doc_) throw std::bad_alloc();
        return d;
    }

    /* --- state queries -------------------------------------------------- */

    explicit operator bool() const noexcept { return doc_ != nullptr; }

    /** @return Root value; invalid Value if document is empty. */
    Value root() const noexcept {
        if (!doc_) return {};
        return Value(::ctoon_doc_get_root(doc_.get()));
    }

    std::size_t valueCount() const noexcept {
        return doc_ ? ::ctoon_doc_get_val_count(doc_.get()) : 0;
    }

    /** @return Last parse error message, or empty string. */
    std::string lastError() const {
        if (!doc_) return {};
        const char *e = ::ctoon_doc_get_error(doc_.get());
        return e ? std::string(e) : std::string{};
    }

    std::size_t lastErrorPos() const noexcept {
        return doc_ ? ::ctoon_doc_get_error_pos(doc_.get()) : 0;
    }

    /* --- value construction -------------------------------------------- */

    Value newNull()                         { return Value(::ctoon_new_null  (doc_.get())); }
    Value newBool  (bool v)                 { return Value(::ctoon_new_bool  (doc_.get(), v)); }
    Value newUint  (std::uint64_t v)        { return Value(::ctoon_new_uint  (doc_.get(), v)); }
    Value newSint  (std::int64_t  v)        { return Value(::ctoon_new_sint  (doc_.get(), v)); }
    Value newDouble(double v)               { return Value(::ctoon_new_real  (doc_.get(), v)); }
    Value newString(std::string_view s)     {
        return Value(::ctoon_new_strn(doc_.get(), s.data(), s.size()));
    }
    Value newArray ()                       { return Value(::ctoon_new_arr (doc_.get())); }
    Value newObject()                       { return Value(::ctoon_new_obj(doc_.get())); }

    bool arrayAppend(Value &arr, Value &val) {
        return ::ctoon_arr_append(doc_.get(), arr.raw(), val.raw());
    }

    bool objectSet(Value &obj, std::string_view key, Value &val) {
        return ::ctoon_obj_setn(doc_.get(), obj.raw(),
                                    key.data(), key.size(), val.raw());
    }

    void setRoot(Value &root) {
        if (doc_) ::ctoon_doc_set_root(doc_.get(), root.raw());
    }

    /* --- serialisation -------------------------------------------------- */

    /** Serialise to TOON string.  Returns empty string on error. */
    std::string toToon() const {
        if (!doc_) return {};
        std::size_t len = 0;
        char *raw = ::ctoon_write(doc_.get(), &len);
        if (!raw) return {};
        std::string result(raw, len);
        free(raw);
        return result;
    }

    /** Write TOON to file; returns true on success. */
    bool toFile(const std::string &path) const {
        if (!doc_) return false;
        return ::ctoon_write_file(doc_.get(), path.c_str());
    }

    /** Expose raw handle for interop with C code. */
    ::ctoon_doc *handle() const noexcept { return doc_.get(); }

private:
    std::shared_ptr<::ctoon_doc> doc_;

    void load(const char *data, std::size_t len) {
        ::ctoon_doc *raw = ::ctoon_read(data, len);
        if (!raw)
            throw ParseError("Failed to parse TOON data");
        doc_.reset(raw, ::ctoon_doc_free);
    }
};

/* ============================================================
 * Convenience free functions
 * ============================================================ */

/** Parse TOON from string; throws ParseError on failure. */
inline Document parse(std::string_view data) {
    return Document(data);
}

/** Parse TOON from file; throws ParseError on failure. */
inline Document parseFile(const std::string &path) {
    return Document::fromFile(path);
}

} // namespace ctoon

#endif /* CTOON_HPP */
