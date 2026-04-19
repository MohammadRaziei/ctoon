/*==============================================================================
 |  ctoon.hpp  –  CToon C++ Binding
 |  Copyright (c) 2026 CToon Project, MIT License
 |
 |  Shadows ctoon.h completely. After including this header you never need
 |  to call any ctoon_* C function directly; everything is exposed through
 |  the ctoon:: namespace.
 *============================================================================*/

#ifndef CTOON_HPP
#define CTOON_HPP

/* Include the C API internally – callers of this header do NOT need to */
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
 * Exceptions
 * ============================================================ */

/** Thrown when a TOON string cannot be parsed. */
class ParseError : public std::runtime_error {
public:
    explicit ParseError(std::string_view msg, std::size_t pos = 0)
        : std::runtime_error(std::string(msg)), pos_(pos) {}
    std::size_t position() const noexcept { return pos_; }
private:
    std::size_t pos_;
};

/** Thrown when a Value is accessed as the wrong type. */
class TypeError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};


/* ============================================================
 * Read / write flags  (mirrors ctoon_read_flag / ctoon_write_flag)
 * ============================================================ */

enum class ReadFlag : ctoon_read_flag {
    None     = CTOON_READ_NOFLAG,
    NoStrict = CTOON_READ_NO_STRICT,
};

enum class WriteFlag : ctoon_write_flag {
    None         = CTOON_WRITE_NOFLAG,
    LengthMarker = CTOON_WRITE_LENGTH_MARKER,
};

enum class Delimiter {
    Comma = CTOON_DELIMITER_COMMA,
    Tab   = CTOON_DELIMITER_TAB,
    Pipe  = CTOON_DELIMITER_PIPE,
};

/** Options for encoding – mirrors ctoon_write_options. */
struct EncodeOptions {
    WriteFlag flag      = WriteFlag::None;
    Delimiter delimiter = Delimiter::Comma;
    int       indent    = 2;

    ctoon_write_options to_c() const noexcept {
        return { static_cast<ctoon_write_flag>(flag),
                 static_cast<ctoon_delimiter>(delimiter),
                 indent };
    }
};

/** Options for decoding – mirrors ctoon_read_options. */
struct DecodeOptions {
    ReadFlag flag   = ReadFlag::None;
    int      indent = 2;

    ctoon_read_options to_c() const noexcept {
        return { static_cast<ctoon_read_flag>(flag), indent };
    }
};


/* ============================================================
 * Forward declarations
 * ============================================================ */

class Document;
class Value;
class ArrayIterator;


/* ============================================================
 * Value  –  non-owning view into a Document's arena
 * ============================================================ */

class Value {
public:
    Value() noexcept : v_(nullptr) {}
    explicit Value(ctoon_val *raw) noexcept : v_(raw) {}

    explicit operator bool() const noexcept { return v_ != nullptr; }
    ctoon_val *raw() const noexcept { return v_; }

    /* --- type queries --------------------------------------------------- */
    bool isNull  () const noexcept { return v_ && ctoon_is_null  (v_); }
    bool isTrue  () const noexcept { return v_ && ctoon_is_true  (v_); }
    bool isFalse () const noexcept { return v_ && ctoon_is_false (v_); }
    bool isBool  () const noexcept { return v_ && ctoon_is_bool  (v_); }
    bool isNumber() const noexcept { return v_ && ctoon_is_num   (v_); }
    bool isUint  () const noexcept { return v_ && ctoon_is_uint  (v_); }
    bool isSint  () const noexcept { return v_ && ctoon_is_sint  (v_); }
    bool isInt   () const noexcept { return v_ && ctoon_is_int   (v_); }
    bool isReal  () const noexcept { return v_ && ctoon_is_real  (v_); }
    bool isString() const noexcept { return v_ && ctoon_is_str   (v_); }
    bool isArray () const noexcept { return v_ && ctoon_is_arr   (v_); }
    bool isObject() const noexcept { return v_ && ctoon_is_obj   (v_); }

    /* --- scalar getters ------------------------------------------------- */
    bool           asBool  () const { check(isBool(),   "boolean"); return ctoon_get_bool(v_); }
    std::uint64_t  asUint  () const { check(isNumber(), "number");  return ctoon_get_uint(v_); }
    std::int64_t   asSint  () const { check(isNumber(), "number");  return ctoon_get_sint(v_); }
    int            asInt   () const { check(isNumber(), "number");  return ctoon_get_int (v_); }
    double         asDouble() const { check(isNumber(), "number");  return ctoon_get_real(v_); }

    std::string asString() const {
        check(isString(), "string");
        const char *p = ctoon_get_str(v_);
        return { p ? p : "", ctoon_get_len(v_) };
    }
    std::string_view asStringView() const noexcept {
        if (!isString()) return {};
        const char *p = ctoon_get_str(v_);
        return { p ? p : "", ctoon_get_len(v_) };
    }

    /* --- size (array or object) ---------------------------------------- */
    std::size_t size() const noexcept {
        if (isArray ()) return ctoon_arr_size(v_);
        if (isObject()) return ctoon_obj_size(v_);
        return 0;
    }

    /* --- array access --------------------------------------------------- */
    Value operator[](std::size_t idx) const noexcept {
        return isArray() ? Value(ctoon_arr_get(v_, idx)) : Value{};
    }
    Value at(std::size_t idx) const {
        check(isArray(), "array");
        if (idx >= size()) throw std::out_of_range("ctoon: array index out of range");
        return Value(ctoon_arr_get(v_, idx));
    }
    std::vector<Value> items() const {
        std::vector<Value> r;
        if (!isArray()) return r;
        r.reserve(size());
        for (std::size_t i = 0; i < size(); i++) r.emplace_back(ctoon_arr_get(v_, i));
        return r;
    }

    /* --- object access -------------------------------------------------- */
    Value operator[](const std::string &key) const noexcept {
        return isObject() ? Value(ctoon_obj_get(v_, key.c_str())) : Value{};
    }
    bool has(const std::string &key) const noexcept {
        return isObject() && ctoon_obj_get(v_, key.c_str()) != nullptr;
    }
    std::optional<Value> get(const std::string &key) const {
        check(isObject(), "object");
        ctoon_val *r = ctoon_obj_get(v_, key.c_str());
        return r ? std::optional<Value>(Value(r)) : std::nullopt;
    }
    std::vector<std::string> keys() const {
        std::vector<std::string> r;
        if (!isObject()) return r;
        std::size_t n = size();
        r.reserve(n);
        for (std::size_t i = 0; i < n; i++) {
            ctoon_val *k = ctoon_obj_get_key_at(v_, i);
            if (k) r.emplace_back(ctoon_get_str(k), ctoon_get_len(k));
        }
        return r;
    }

    /* --- range-for (array) --------------------------------------------- */
    ArrayIterator begin() const noexcept;
    ArrayIterator end  () const noexcept;

    /* --- equality ------------------------------------------------------- */
    bool operator==(std::string_view s) const noexcept {
        return isString() && asStringView() == s;
    }
    bool operator!=(std::string_view s) const noexcept { return !(*this == s); }

private:
    ctoon_val *v_;

    void check(bool ok, const char *expected) const {
        if (!ok) throw TypeError(std::string("ctoon: value is not ") + expected);
    }
};


/* ============================================================
 * ArrayIterator
 * ============================================================ */

class ArrayIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = Value;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;
    using reference         = Value;

    ArrayIterator(ctoon_val *arr, std::size_t idx) noexcept : arr_(arr), idx_(idx) {}

    Value operator* () const noexcept { return Value(ctoon_arr_get(arr_, idx_)); }
    ArrayIterator &operator++() noexcept { ++idx_; return *this; }
    ArrayIterator  operator++(int) noexcept { auto t = *this; ++*this; return t; }
    bool operator==(const ArrayIterator &o) const noexcept {
        return arr_ == o.arr_ && idx_ == o.idx_;
    }
    bool operator!=(const ArrayIterator &o) const noexcept { return !(*this == o); }

private:
    ctoon_val  *arr_;
    std::size_t idx_;
};

inline ArrayIterator Value::begin() const noexcept {
    return { isArray() ? v_ : nullptr, 0 };
}
inline ArrayIterator Value::end() const noexcept {
    return { isArray() ? v_ : nullptr, size() };
}


/* ============================================================
 * Document  –  owns all TOON values (RAII)
 * ============================================================ */

class Document {
public:
    Document() noexcept = default;

    /* --- parse from string --------------------------------------------- */
    explicit Document(std::string_view data) { load(data.data(), data.size()); }
    Document(const char *data, std::size_t len) { load(data, len); }

    /* --- parse from string with options --------------------------------- */
    Document(std::string_view data, const DecodeOptions &opts) {
        ctoon_read_options c = opts.to_c();
        ctoon_err err = {};
        ctoon_doc *doc = ctoon_read_opts(data.data(), data.size(), &c, &err);
        if (!doc) throw ParseError(err.msg ? err.msg : "parse failed", err.pos);
        doc_.reset(doc, ctoon_doc_free);
    }

    Document(const Document &)            = delete;
    Document &operator=(const Document &) = delete;
    Document(Document &&)                 = default;
    Document &operator=(Document &&)      = default;
    ~Document()                           = default;

    /* --- factories ------------------------------------------------------ */
    static Document fromFile(const std::string &path) {
        Document d;
        ctoon_doc *doc = ctoon_read_file(path.c_str());
        if (!doc) throw ParseError("cannot read file: " + path);
        d.doc_.reset(doc, ctoon_doc_free);
        return d;
    }
    static Document fromFile(const std::string &path, const DecodeOptions &opts) {
        Document d;
        ctoon_read_options c = opts.to_c();
        ctoon_err err = {};
        ctoon_doc *doc = ctoon_read_file_opts(path.c_str(), &c, &err);
        if (!doc) throw ParseError(err.msg ? err.msg : "cannot read file", err.pos);
        d.doc_.reset(doc, ctoon_doc_free);
        return d;
    }
    /** Create an empty document for programmatic construction. */
    static Document create(std::size_t maxMemory = 0) {
        Document d;
        ctoon_doc *doc = ctoon_doc_new(maxMemory);
        if (!doc) throw std::bad_alloc();
        d.doc_.reset(doc, ctoon_doc_free);
        return d;
    }

    /* --- state ---------------------------------------------------------- */
    explicit operator bool() const noexcept { return doc_ != nullptr; }
    Value root() const noexcept {
        return doc_ ? Value(ctoon_doc_get_root(doc_.get())) : Value{};
    }
    std::size_t valueCount() const noexcept {
        return doc_ ? ctoon_doc_get_val_count(doc_.get()) : 0;
    }

    /* --- serialise ------------------------------------------------------ */
    /** Encode to TOON string. */
    std::string encode(const EncodeOptions &opts = {}) const {
        if (!doc_) return {};
        ctoon_write_options c = opts.to_c();
        ctoon_err err = {};
        std::size_t len = 0;
        char *raw = ctoon_encode_opts(doc_.get(), &c, &len, &err);
        if (!raw) throw std::runtime_error(err.msg ? err.msg : "encode failed");
        std::string r(raw, len);
        free(raw);
        return r;
    }

    bool writeFile(const std::string &path, const EncodeOptions &opts = {}) const {
        if (!doc_) return false;
        ctoon_write_options c = opts.to_c();
        ctoon_err err = {};
        return ctoon_encode_file_opts(doc_.get(), path.c_str(), &c, &err);
    }

    /* --- value construction -------------------------------------------- */
    Value newNull  ()                      { return Value(ctoon_new_null (doc_.get())); }
    Value newBool  (bool v)                { return Value(ctoon_new_bool (doc_.get(), v)); }
    Value newUint  (std::uint64_t v)       { return Value(ctoon_new_uint (doc_.get(), v)); }
    Value newSint  (std::int64_t  v)       { return Value(ctoon_new_sint (doc_.get(), v)); }
    Value newDouble(double v)              { return Value(ctoon_new_real (doc_.get(), v)); }
    Value newString(std::string_view s)    { return Value(ctoon_new_strn(doc_.get(), s.data(), s.size())); }
    Value newArray ()                      { return Value(ctoon_new_arr  (doc_.get())); }
    Value newObject()                      { return Value(ctoon_new_obj  (doc_.get())); }

    bool arrayAppend(Value &arr, Value &val) {
        return ctoon_arr_append(doc_.get(), arr.raw(), val.raw());
    }
    bool objectSet(Value &obj, std::string_view key, Value &val) {
        return ctoon_obj_setn(doc_.get(), obj.raw(), key.data(), key.size(), val.raw());
    }
    void setRoot(Value &root) {
        if (doc_) ctoon_doc_set_root(doc_.get(), root.raw());
    }

    /* --- raw C handle --------------------------------------------------- */
    ctoon_doc *handle() const noexcept { return doc_.get(); }

private:
    std::shared_ptr<ctoon_doc> doc_;

    void load(const char *data, std::size_t len) {
        ctoon_doc *doc = ctoon_decode(data, len);
        if (!doc) throw ParseError("TOON parse failed");
        doc_.reset(doc, ctoon_doc_free);
    }
};


/* ============================================================
 * Free functions  (shadow ctoon_decode / ctoon_encode)
 * ============================================================ */

/** Parse a TOON string. Throws ParseError on failure. */
inline Document decode(std::string_view data) {
    return Document(data);
}
inline Document decode(std::string_view data, const DecodeOptions &opts) {
    return Document(data, opts);
}
inline Document decodeFile(const std::string &path) {
    return Document::fromFile(path);
}
inline Document decodeFile(const std::string &path, const DecodeOptions &opts) {
    return Document::fromFile(path, opts);
}

/** Encode a document to TOON. Equivalent to doc.encode(opts). */
inline std::string encode(const Document &doc, const EncodeOptions &opts = {}) {
    return doc.encode(opts);
}

} // namespace ctoon

#endif /* CTOON_HPP */
