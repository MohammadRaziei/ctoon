/**
 * @file ctoon.hpp
 * @brief C++11 binding for the CToon library.
 *
 * Include-only wrapper around ctoon.h.
 * Everything lives in namespace @c ctoon.
 * Naming follows the STL snake_case convention.
 *
 * @par Class hierarchy
 * | Class             | Owns memory | Underlying type     |
 * |-------------------|-------------|---------------------|
 * | ctoon::value      | no          | ctoon_val*          |
 * | ctoon::mut_value  | no          | ctoon_mut_val*      |
 * | ctoon::document   | yes         | ctoon_doc*          |
 * | ctoon::mut_document | yes       | ctoon_mut_doc*      |
 *
 * @par Exception hierarchy
 * @code
 * std::exception
 * └── ctoon::error             (base)
 *     ├── ctoon::parse_error   (TOON parse failures)
 *     └── ctoon::write_error   (serialisation failures)
 * @endcode
 *
 * @author Mohammad Raziei
 * @copyright MIT License, 2026
 */

#pragma once

#include "ctoon.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

/* -----------------------------------------------------------------------
 * Portability helpers
 * -------------------------------------------------------------------- */

/// @cond INTERNAL
#if __cplusplus >= 201103L
#   define CTOON_NOEXCEPT noexcept
#else
#   define CTOON_NOEXCEPT throw()
#endif

#if __cplusplus >= 201703L
#   include <string_view>
    namespace ctoon { using string_view = std::string_view; }
#else
    namespace ctoon {
    /**
     * @brief Minimal string_view substitute for C++11/14.
     *
     * Provides the data/size/str interface used by ctoon::value and friends.
     */
    class string_view {
    public:
        string_view() CTOON_NOEXCEPT : d_(""), n_(0) {}
        string_view(const char *p, std::size_t n) CTOON_NOEXCEPT
            : d_(p ? p : ""), n_(p ? n : 0) {}
        string_view(const char *p) CTOON_NOEXCEPT
            : d_(p ? p : ""), n_(p ? std::strlen(p) : 0) {}
        string_view(const std::string &s) CTOON_NOEXCEPT
            : d_(s.data()), n_(s.size()) {}

        const char *data()  const CTOON_NOEXCEPT { return d_; }
        std::size_t size()  const CTOON_NOEXCEPT { return n_; }
        bool        empty() const CTOON_NOEXCEPT { return n_ == 0; }

        /// Copy to a std::string.
        std::string str()   const { return std::string(d_, n_); }

        bool operator==(const string_view &o) const CTOON_NOEXCEPT {
            return n_ == o.n_ && std::memcmp(d_, o.d_, n_) == 0;
        }
        bool operator!=(const string_view &o) const CTOON_NOEXCEPT {
            return !(*this == o);
        }
    private:
        const char *d_;
        std::size_t n_;
    };
    } // namespace ctoon
#endif
/// @endcond

namespace ctoon {

/* -----------------------------------------------------------------------
 * Version
 * -------------------------------------------------------------------- */

/// @name Version constants
/// @{
static const unsigned int    VERSION_MAJOR  = CTOON_VERSION_MAJOR; ///< Major version number.
static const unsigned int    VERSION_MINOR  = CTOON_VERSION_MINOR; ///< Minor version number.
static const unsigned int    VERSION_PATCH  = CTOON_VERSION_PATCH; ///< Patch version number.
static const char * const    VERSION_STRING = CTOON_VERSION_STRING; ///< @c "major.minor.patch".
/// @}

/* -----------------------------------------------------------------------
 * Exception hierarchy
 * -------------------------------------------------------------------- */

/**
 * @brief Base exception for all CToon C++ API errors.
 *
 * Derives from std::exception.  Catch this to handle any ctoon failure
 * without caring about the specific subtype.
 *
 * @code
 * try {
 *     auto doc = ctoon::parse(toon_text);
 * } catch (const ctoon::error &e) {
 *     std::cerr << e.what() << "\n";
 * }
 * @endcode
 */
class error : public std::exception {
public:
    /// Construct with an explanatory message.
    explicit error(const std::string &msg) CTOON_NOEXCEPT : msg_(msg) {}

    /// Null-terminated description of the error.
    virtual const char *what() const CTOON_NOEXCEPT { return msg_.c_str(); }

    virtual ~error() CTOON_NOEXCEPT {}

protected:
    std::string msg_;
};

/**
 * @brief Thrown when a TOON document fails to parse.
 *
 * @par Member fields
 * - @c code – numeric error code (@c ctoon_read_code)
 * - @c pos  – byte offset in the source where parsing stopped
 */
class parse_error : public error {
public:
    ctoon_read_code code; ///< Numeric error code.
    std::size_t     pos;  ///< Byte position in the input.

    /// Construct from the C read-error struct.
    explicit parse_error(const ctoon_read_err &e)
        : error(build_msg(e))
        , code(e.code)
        , pos(e.pos)
    {}

    virtual ~parse_error() CTOON_NOEXCEPT {}

private:
    static std::string build_msg(const ctoon_read_err &e) {
        std::string m(e.msg ? e.msg : "parse error");
        char buf[32];
        std::sprintf(buf, " (pos %zu)", e.pos);
        return m + buf;
    }
};

/**
 * @brief Thrown when a document or value fails to serialise.
 *
 * @par Member fields
 * - @c code – numeric error code (@c ctoon_write_code)
 */
class write_error : public error {
public:
    ctoon_write_code code; ///< Numeric error code.

    /// Construct from the C write-error struct.
    explicit write_error(const ctoon_write_err &e)
        : error(e.msg ? e.msg : "write error")
        , code(e.code)
    {}

    virtual ~write_error() CTOON_NOEXCEPT {}
};

/* -----------------------------------------------------------------------
 * write_result  –  RAII owner for heap strings from ctoon_write_*
 * -------------------------------------------------------------------- */

/**
 * @brief RAII wrapper for heap strings returned by ctoon write functions.
 *
 * Non-copyable; move-only semantics (C++11 move emulated with non-const ref).
 * The underlying pointer is released with @c std::free() on destruction.
 */
class write_result {
public:
    /// Default: empty result.
    write_result() CTOON_NOEXCEPT : ptr_(NULL), len_(0) {}
    /// Take ownership of @p p (must be heap-allocated, @p n bytes, no NUL counted).
    write_result(char *p, std::size_t n) CTOON_NOEXCEPT : ptr_(p), len_(n) {}
    ~write_result() { std::free(ptr_); }

    /// Move constructor (C++11 style via non-const ref).
    write_result(write_result &&o) CTOON_NOEXCEPT
        : ptr_(o.ptr_), len_(o.len_) { o.ptr_ = NULL; o.len_ = 0; }

    /// Move-assign.
    write_result &operator=(write_result &&o) CTOON_NOEXCEPT {
        if (this != &o) {
            std::free(ptr_);
            ptr_ = o.ptr_; len_ = o.len_;
            o.ptr_ = NULL; o.len_ = 0;
        }
        return *this;
    }

    const char  *c_str() const CTOON_NOEXCEPT { return ptr_ ? ptr_ : ""; } ///< Null-terminated pointer.
    std::size_t  size()  const CTOON_NOEXCEPT { return len_;  } ///< Byte length (excl. NUL).
    bool         empty() const CTOON_NOEXCEPT { return len_ == 0; }
    string_view  view()  const CTOON_NOEXCEPT { return string_view(ptr_ ? ptr_ : "", len_); } ///< Non-owning view.
    std::string  str()   const { return std::string(ptr_ ? ptr_ : "", len_); } ///< Copy to std::string.

    char        *ptr_;
    std::size_t  len_;
};

/* -----------------------------------------------------------------------
 * write_options
 * -------------------------------------------------------------------- */

/**
 * @brief Options controlling TOON serialisation.
 *
 * Wraps @c ctoon_write_options. Default values match the library defaults:
 * 2-space indent, comma delimiter, no special flags.
 */
struct write_options {
    ctoon_write_flag flag;      ///< @c CTOON_WRITE_* flags.
    ctoon_delimiter  delimiter; ///< Array value delimiter.
    int              indent;    ///< Spaces per indent level (0 = compact).

    /// Construct with library defaults.
    write_options() CTOON_NOEXCEPT
        : flag(CTOON_WRITE_NOFLAG)
        , delimiter(CTOON_DELIMITER_COMMA)
        , indent(2)
    {}

    /// Construct with explicit values.
    write_options(ctoon_write_flag f, ctoon_delimiter d, int i) CTOON_NOEXCEPT
        : flag(f), delimiter(d), indent(i) {}

    /// Convert to the underlying C struct.
    ctoon_write_options to_c() const CTOON_NOEXCEPT {
        ctoon_write_options o;
        o.flag      = flag;
        o.delimiter = delimiter;
        o.indent    = indent;
        return o;
    }
};

/* -----------------------------------------------------------------------
 * Forward declarations
 * -------------------------------------------------------------------- */

class value;
class mut_value;
class document;
class mut_document;

/* -----------------------------------------------------------------------
 * value  –  non-owning immutable view over ctoon_val*
 * -------------------------------------------------------------------- */

/**
 * @brief Non-owning view over an immutable @c ctoon_val node.
 *
 * Instances are cheap to copy. They are valid only while the owning
 * @c document is alive.
 */
class value {
public:
    value() CTOON_NOEXCEPT : v_(NULL) {} ///< Invalid (null) view.
    explicit value(ctoon_val *v) CTOON_NOEXCEPT : v_(v) {} ///< Wrap a C pointer.

    ctoon_val *raw()  const CTOON_NOEXCEPT { return v_; }   ///< Underlying pointer.
    bool       valid() const CTOON_NOEXCEPT { return v_ != NULL; } ///< Is this view valid?
    operator   bool()  const CTOON_NOEXCEPT { return valid(); }

    /* --- type predicates ----------------------------------------------- */
    /// @name Type predicates
    /// Return @c false when the view is invalid.
    /// @{
    bool is_null()  const CTOON_NOEXCEPT { return v_ && ctoon_is_null(v_);  }
    bool is_true()  const CTOON_NOEXCEPT { return v_ && ctoon_is_true(v_);  }
    bool is_false() const CTOON_NOEXCEPT { return v_ && ctoon_is_false(v_); }
    bool is_bool()  const CTOON_NOEXCEPT { return v_ && ctoon_is_bool(v_);  }
    bool is_uint()  const CTOON_NOEXCEPT { return v_ && ctoon_is_uint(v_);  }
    bool is_sint()  const CTOON_NOEXCEPT { return v_ && ctoon_is_sint(v_);  }
    bool is_int()   const CTOON_NOEXCEPT { return v_ && ctoon_is_int(v_);   }
    bool is_real()  const CTOON_NOEXCEPT { return v_ && ctoon_is_real(v_);  }
    bool is_num()   const CTOON_NOEXCEPT { return v_ && ctoon_is_num(v_);   }
    bool is_str()   const CTOON_NOEXCEPT { return v_ && ctoon_is_str(v_);   }
    bool is_arr()   const CTOON_NOEXCEPT { return v_ && ctoon_is_arr(v_);   }
    bool is_obj()   const CTOON_NOEXCEPT { return v_ && ctoon_is_obj(v_);   }
    /// @}

    /* --- scalar getters ------------------------------------------------- */
    /// @name Scalar getters
    /// Return a zero/empty value when the view is invalid or the type mismatches.
    /// @{
    bool              get_bool() const CTOON_NOEXCEPT { return  v_ && ctoon_get_bool(v_);    }
    std::uint64_t     get_uint() const CTOON_NOEXCEPT { return  v_ ? ctoon_get_uint(v_) : 0; }
    std::int64_t      get_sint() const CTOON_NOEXCEPT { return  v_ ? ctoon_get_sint(v_) : 0; }
    double            get_real() const CTOON_NOEXCEPT { return  v_ ? ctoon_get_real(v_) : 0.0; }
    double            get_num()  const CTOON_NOEXCEPT { return  v_ ? ctoon_get_num(v_)  : 0.0; }
    std::size_t       get_len()  const CTOON_NOEXCEPT { return  v_ ? ctoon_get_len(v_)  : 0;   }
    /// Non-owning view of the string value; empty when not a string.
    string_view       get_str()  const CTOON_NOEXCEPT {
        if (!v_) return string_view();
        const char *s = ctoon_get_str(v_);
        return s ? string_view(s, ctoon_get_len(v_)) : string_view();
    }
    /// @}

    /* --- array access --------------------------------------------------- */
    /// @name Array access
    /// @{
    std::size_t arr_size()            const CTOON_NOEXCEPT { return v_ ? ctoon_arr_size(v_) : 0; }
    value arr_get(std::size_t i)      const CTOON_NOEXCEPT { return value(v_ ? ctoon_arr_get(v_, i) : NULL); }
    value arr_first()                 const CTOON_NOEXCEPT { return value(v_ ? ctoon_arr_get_first(v_) : NULL); }
    value arr_last()                  const CTOON_NOEXCEPT { return value(v_ ? ctoon_arr_get_last(v_)  : NULL); }
    value operator[](std::size_t i)   const CTOON_NOEXCEPT { return arr_get(i); }
    /// @}

    /* --- object access -------------------------------------------------- */
    /// @name Object access
    /// @{
    std::size_t obj_size()                    const CTOON_NOEXCEPT { return v_ ? ctoon_obj_size(v_) : 0; }
    value obj_get(const char *key)            const CTOON_NOEXCEPT { return value(v_ ? ctoon_obj_get(v_, key) : NULL); }
    value obj_get(const string_view &key)     const CTOON_NOEXCEPT {
        return value(v_ ? ctoon_obj_getn(v_, key.data(), key.size()) : NULL);
    }
    value operator[](const char *key)         const CTOON_NOEXCEPT { return obj_get(key); }
    value operator[](const string_view &key)  const CTOON_NOEXCEPT { return obj_get(key); }
    /// @}

    /* --- equality ------------------------------------------------------- */
    /// @name Equality
    /// @{
    bool equals(const value &rhs)          const CTOON_NOEXCEPT { return v_ && rhs.v_ && ctoon_equals(v_, rhs.v_); }
    bool equals(const char *s)             const CTOON_NOEXCEPT { return v_ && ctoon_equals_str(v_, s); }
    bool equals(const string_view &s)      const CTOON_NOEXCEPT { return v_ && ctoon_equals_strn(v_, s.data(), s.size()); }
    /// @}

    /* --- TOON pointer path ---------------------------------------------- */
    /**
     * @brief Resolve a TOON path expression, e.g. @c "/users/0/name".
     * @return Matching value, or invalid view when not found.
     */
    value get_pointer(const string_view &ptr) const CTOON_NOEXCEPT {
        return value(v_ ? ctoon_ptr_getn(v_, ptr.data(), ptr.size()) : NULL);
    }

    /* --- serialisation -------------------------------------------------- */
    /**
     * @brief Serialise this value to a TOON string.
     * @param opts Write options (default: 2-space indent, comma delimiter).
     * @throws ctoon::write_error on failure.
     */
    write_result write(const write_options &opts = write_options()) const {
        ctoon_write_options co = opts.to_c();
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_val_write_opts(v_, &co, NULL, &len, &err);
        if (!raw) throw write_error(err);
        return write_result(raw, len);
    }

    /* --- iterators ------------------------------------------------------ */
    /// @cond INTERNAL
    class arr_iterator {
    public:
        explicit arr_iterator(ctoon_val *arr, bool end_sentinel = false)
            : cur_(NULL)
        {
            if (!arr || end_sentinel) { std::memset(&iter_, 0, sizeof(iter_)); return; }
            ctoon_arr_iter_init(arr, &iter_);
            cur_ = ctoon_arr_iter_next(&iter_);
        }
        value          operator*()  const CTOON_NOEXCEPT { return value(cur_); }
        arr_iterator  &operator++() CTOON_NOEXCEPT { cur_ = ctoon_arr_iter_next(&iter_); return *this; }
        bool           operator!=(const arr_iterator &o) const CTOON_NOEXCEPT { return cur_ != o.cur_; }
    private:
        ctoon_arr_iter iter_;
        ctoon_val     *cur_;
    };
    /// @endcond

    /// Range for range-based @c for over array elements.
    struct arr_range {
        ctoon_val   *arr;
        arr_iterator begin() const { return arr_iterator(arr); }
        arr_iterator end()   const { return arr_iterator(arr, true); }
    };

    /**
     * @brief Range over array elements.
     * @code
     * for (ctoon::value item : val.arr_items()) { ... }
     * @endcode
     */
    arr_range arr_items() const CTOON_NOEXCEPT { arr_range r; r.arr = v_; return r; }

    /// @cond INTERNAL
    struct kv_pair {
        ctoon_val *key_raw; ctoon_val *val_raw;
        value key() const { return value(key_raw); }
        value val() const { return value(val_raw); }
    };

    class obj_iterator {
    public:
        explicit obj_iterator(ctoon_val *obj, bool end_sentinel = false)
            : key_(NULL)
        {
            if (!obj || end_sentinel) { std::memset(&iter_, 0, sizeof(iter_)); return; }
            ctoon_obj_iter_init(obj, &iter_);
            key_ = ctoon_obj_iter_next(&iter_);
        }
        kv_pair operator*() const CTOON_NOEXCEPT {
            kv_pair kv; kv.key_raw = key_; kv.val_raw = ctoon_obj_iter_get_val(key_);
            return kv;
        }
        obj_iterator  &operator++() CTOON_NOEXCEPT { key_ = ctoon_obj_iter_next(&iter_); return *this; }
        bool           operator!=(const obj_iterator &o) const CTOON_NOEXCEPT { return key_ != o.key_; }
    private:
        ctoon_obj_iter iter_;
        ctoon_val     *key_;
    };
    /// @endcond

    /// Range for range-based @c for over object fields.
    struct obj_range {
        ctoon_val   *obj;
        obj_iterator begin() const { return obj_iterator(obj); }
        obj_iterator end()   const { return obj_iterator(obj, true); }
    };

    /**
     * @brief Range over object key-value pairs.
     * @code
     * for (ctoon::value::kv_pair kv : val.obj_items()) {
     *     std::cout << kv.key.get_str().str() << "\n";
     * }
     * @endcode
     */
    obj_range obj_items() const CTOON_NOEXCEPT { obj_range r; r.obj = v_; return r; }

protected:
    ctoon_val *v_; ///< Underlying pointer (not owned).
};

/* -----------------------------------------------------------------------
 * mut_value  –  non-owning mutable view over ctoon_mut_val*
 * -------------------------------------------------------------------- */

/**
 * @brief Non-owning view over a mutable @c ctoon_mut_val node.
 *
 * Mirrors @c value but operates on the mutable document tree.
 */
class mut_value {
public:
    mut_value() CTOON_NOEXCEPT : v_(NULL) {}
    explicit mut_value(ctoon_mut_val *v) CTOON_NOEXCEPT : v_(v) {}

    ctoon_mut_val *raw()   const CTOON_NOEXCEPT { return v_; }
    bool           valid() const CTOON_NOEXCEPT { return v_ != NULL; }
    operator       bool()  const CTOON_NOEXCEPT { return valid(); }

    /* --- type predicates --- */
    /// @name Type predicates
    /// @{
    bool is_null()  const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_null(v_);  }
    bool is_true()  const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_true(v_);  }
    bool is_false() const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_false(v_); }
    bool is_bool()  const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_bool(v_);  }
    bool is_uint()  const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_uint(v_);  }
    bool is_sint()  const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_sint(v_);  }
    bool is_int()   const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_int(v_);   }
    bool is_real()  const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_real(v_);  }
    bool is_num()   const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_num(v_);   }
    bool is_str()   const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_str(v_);   }
    bool is_arr()   const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_arr(v_);   }
    bool is_obj()   const CTOON_NOEXCEPT { return v_ && ctoon_mut_is_obj(v_);   }
    /// @}

    /* --- scalar getters --- */
    /// @name Scalar getters
    /// @{
    bool          get_bool() const CTOON_NOEXCEPT { return  v_ && ctoon_mut_get_bool(v_);    }
    std::uint64_t get_uint() const CTOON_NOEXCEPT { return  v_ ? ctoon_mut_get_uint(v_) : 0; }
    std::int64_t  get_sint() const CTOON_NOEXCEPT { return  v_ ? ctoon_mut_get_sint(v_) : 0; }
    double        get_real() const CTOON_NOEXCEPT { return  v_ ? ctoon_mut_get_real(v_) : 0.0; }
    double        get_num()  const CTOON_NOEXCEPT { return  v_ ? ctoon_mut_get_num(v_)  : 0.0; }
    std::size_t   get_len()  const CTOON_NOEXCEPT { return  v_ ? ctoon_mut_get_len(v_)  : 0;   }
    string_view   get_str()  const CTOON_NOEXCEPT {
        if (!v_) return string_view();
        const char *s = ctoon_mut_get_str(v_);
        return s ? string_view(s, ctoon_mut_get_len(v_)) : string_view();
    }
    /// @}

    /* --- array access --- */
    /// @name Array access
    /// @{
    std::size_t arr_size()              const CTOON_NOEXCEPT { return v_ ? ctoon_mut_arr_size(v_) : 0; }
    mut_value arr_get(std::size_t i)    const CTOON_NOEXCEPT { return mut_value(v_ ? ctoon_mut_arr_get(v_, i) : NULL); }
    mut_value arr_first()               const CTOON_NOEXCEPT { return mut_value(v_ ? ctoon_mut_arr_get_first(v_) : NULL); }
    mut_value arr_last()                const CTOON_NOEXCEPT { return mut_value(v_ ? ctoon_mut_arr_get_last(v_)  : NULL); }
    mut_value operator[](std::size_t i) const CTOON_NOEXCEPT { return arr_get(i); }
    /// @}

    /* --- object access --- */
    /// @name Object access
    /// @{
    std::size_t obj_size()                         const CTOON_NOEXCEPT { return v_ ? ctoon_mut_obj_size(v_) : 0; }
    mut_value obj_get(const char *key)             const CTOON_NOEXCEPT { return mut_value(v_ ? ctoon_mut_obj_get(v_, key) : NULL); }
    mut_value obj_get(const string_view &key)      const CTOON_NOEXCEPT {
        return mut_value(v_ ? ctoon_mut_obj_getn(v_, key.data(), key.size()) : NULL);
    }
    mut_value operator[](const char *key)          const CTOON_NOEXCEPT { return obj_get(key); }
    mut_value operator[](const string_view &key)   const CTOON_NOEXCEPT { return obj_get(key); }
    /// @}

    /* --- equality --- */
    bool equals(const mut_value &rhs)     const CTOON_NOEXCEPT { return v_ && rhs.v_ && ctoon_mut_equals(v_, rhs.v_); }
    bool equals(const char *s)            const CTOON_NOEXCEPT { return v_ && ctoon_mut_equals_str(v_, s); }
    bool equals(const string_view &s)     const CTOON_NOEXCEPT { return v_ && ctoon_mut_equals_strn(v_, s.data(), s.size()); }

    /* --- TOON pointer --- */
    mut_value get_pointer(const string_view &ptr) const CTOON_NOEXCEPT {
        return mut_value(v_ ? ctoon_mut_ptr_getn(v_, ptr.data(), ptr.size()) : NULL);
    }

    /* --- array mutation --- */
    /// @name Array mutation
    /// @{
    bool arr_append(mut_value item)  const CTOON_NOEXCEPT { return v_ && item.v_ && ctoon_mut_arr_append(v_, item.v_); }
    bool arr_prepend(mut_value item) const CTOON_NOEXCEPT { return v_ && item.v_ && ctoon_mut_arr_prepend(v_, item.v_); }
    bool arr_clear()                 const CTOON_NOEXCEPT { return v_ && ctoon_mut_arr_clear(v_); }
    mut_value arr_remove(std::size_t i) const CTOON_NOEXCEPT {
        return mut_value(v_ ? ctoon_mut_arr_remove(v_, i) : NULL);
    }
    /// @}

    /* --- object mutation --- */
    /// @name Object mutation
    /// @{
    /**
     * @brief Append a key-value pair (does not replace duplicate keys).
     * @return @c true on success.
     */
    bool obj_add(mut_value key, mut_value val) const CTOON_NOEXCEPT {
        return v_ && key.v_ && val.v_ && ctoon_mut_obj_add(v_, key.v_, val.v_);
    }
    /**
     * @brief Append or replace a key-value pair.
     * @return @c true on success.
     */
    bool obj_put(mut_value key, mut_value val) const CTOON_NOEXCEPT {
        return v_ && key.v_ && val.v_ && ctoon_mut_obj_put(v_, key.v_, val.v_);
    }
    bool obj_clear() const CTOON_NOEXCEPT { return v_ && ctoon_mut_obj_clear(v_); }
    mut_value obj_remove(const string_view &key) const CTOON_NOEXCEPT {
        return mut_value(v_ ? ctoon_mut_obj_remove_keyn(v_, key.data(), key.size()) : NULL);
    }
    /// @}

    /* --- iterators --- */
    /// @cond INTERNAL
    class arr_iterator {
    public:
        explicit arr_iterator(ctoon_mut_val *arr, bool end_sentinel = false)
            : cur_(NULL)
        {
            if (!arr || end_sentinel) { std::memset(&iter_, 0, sizeof(iter_)); return; }
            ctoon_mut_arr_iter_init(arr, &iter_);
            cur_ = ctoon_mut_arr_iter_next(&iter_);
        }
        mut_value      operator*()  const CTOON_NOEXCEPT { return mut_value(cur_); }
        arr_iterator  &operator++() CTOON_NOEXCEPT { cur_ = ctoon_mut_arr_iter_next(&iter_); return *this; }
        bool           operator!=(const arr_iterator &o) const CTOON_NOEXCEPT { return cur_ != o.cur_; }
    private:
        ctoon_mut_arr_iter iter_;
        ctoon_mut_val     *cur_;
    };
    /// @endcond

    struct arr_range {
        ctoon_mut_val *arr;
        arr_iterator begin() const { return arr_iterator(arr); }
        arr_iterator end()   const { return arr_iterator(arr, true); }
    };
    arr_range arr_items() const CTOON_NOEXCEPT { arr_range r; r.arr = v_; return r; }

    /// @cond INTERNAL
    struct kv_pair {
        ctoon_mut_val *key_raw; ctoon_mut_val *val_raw;
        mut_value key() const { return mut_value(key_raw); }
        mut_value val() const { return mut_value(val_raw); }
    };

    class obj_iterator {
    public:
        explicit obj_iterator(ctoon_mut_val *obj, bool end_sentinel = false)
            : key_(NULL)
        {
            if (!obj || end_sentinel) { std::memset(&iter_, 0, sizeof(iter_)); return; }
            ctoon_mut_obj_iter_init(obj, &iter_);
            key_ = ctoon_mut_obj_iter_next(&iter_);
        }
        kv_pair operator*() const CTOON_NOEXCEPT {
            kv_pair kv; kv.key_raw = key_; kv.val_raw = ctoon_mut_obj_iter_get_val(key_);
            return kv;
        }
        obj_iterator  &operator++() CTOON_NOEXCEPT { key_ = ctoon_mut_obj_iter_next(&iter_); return *this; }
        bool           operator!=(const obj_iterator &o) const CTOON_NOEXCEPT { return key_ != o.key_; }
    private:
        ctoon_mut_obj_iter iter_;
        ctoon_mut_val     *key_;
    };
    /// @endcond

    struct obj_range {
        ctoon_mut_val *obj;
        obj_iterator begin() const { return obj_iterator(obj); }
        obj_iterator end()   const { return obj_iterator(obj, true); }
    };
    obj_range obj_items() const CTOON_NOEXCEPT { obj_range r; r.obj = v_; return r; }

protected:
    ctoon_mut_val *v_; ///< Underlying pointer (not owned).
};

/* -----------------------------------------------------------------------
 * document  –  RAII owner of ctoon_doc*
 * -------------------------------------------------------------------- */

/**
 * @brief RAII owner of an immutable (parsed) TOON document.
 *
 * @par Example
 * @code
 * ctoon::document doc = ctoon::parse("name: Alice\nage: 30\n");
 * std::cout << doc.root()["name"].get_str().str() << "\n"; // Alice
 * std::cout << doc.root()["age"].get_uint()        << "\n"; // 30
 * // Serialise back to TOON
 * std::cout << doc.write().c_str() << "\n";
 * // Serialise to JSON
 * std::cout << doc.to_json(2).c_str() << "\n";
 * @endcode
 */
class document {
public:
    document() CTOON_NOEXCEPT : doc_(NULL) {} ///< Invalid (empty) document.
    explicit document(ctoon_doc *doc) CTOON_NOEXCEPT : doc_(doc) {} ///< Take ownership.
    ~document() { ctoon_doc_free(doc_); }

    /// Move constructor.
    document(document &&o) CTOON_NOEXCEPT : doc_(o.doc_) { o.doc_ = NULL; }
    /// Move-assign.
    document &operator=(document &&o) CTOON_NOEXCEPT {
        if (this != &o) { ctoon_doc_free(doc_); doc_ = o.doc_; o.doc_ = NULL; }
        return *this;
    }

    /* --- parse factories ------------------------------------------------ */
    /**
     * @brief Parse TOON bytes.
     * @param toon   Input pointer (need not be null-terminated).
     * @param len    Byte count.
     * @param flags  @c CTOON_READ_* flags.
     * @throws ctoon::parse_error on failure.
     */
    static document parse(const char *toon, std::size_t len,
                           ctoon_read_flag flags = CTOON_READ_NOFLAG) {
        ctoon_read_err err; std::memset(&err, 0, sizeof(err));
        ctoon_doc *d = ctoon_read_opts(const_cast<char *>(toon),
                                        len, flags, NULL, &err);
        if (!d) throw parse_error(err);
        return document(d);
    }

    /// @overload Null-terminated TOON string.
    static document parse(const char *toon,
                           ctoon_read_flag flags = CTOON_READ_NOFLAG) {
        return parse(toon, std::strlen(toon), flags);
    }

    /// @overload std::string.
    static document parse(const std::string &toon,
                           ctoon_read_flag flags = CTOON_READ_NOFLAG) {
        return parse(toon.data(), toon.size(), flags);
    }

    /**
     * @brief Parse a TOON file from disk.
     * @param path  Null-terminated file path.
     * @param flags @c CTOON_READ_* flags.
     * @throws ctoon::parse_error on failure.
     */
    static document parse_file(const char *path,
                                ctoon_read_flag flags = CTOON_READ_NOFLAG) {
        ctoon_read_err err; std::memset(&err, 0, sizeof(err));
        ctoon_doc *d = ctoon_read_file(path, flags, NULL, &err);
        if (!d) throw parse_error(err);
        return document(d);
    }

    /* --- access --------------------------------------------------------- */
    bool       valid()       const CTOON_NOEXCEPT { return doc_ != NULL; }
    operator   bool()        const CTOON_NOEXCEPT { return valid(); }

    value root()             const CTOON_NOEXCEPT { return value(ctoon_doc_get_root(doc_)); } ///< Root value.
    value operator*()        const CTOON_NOEXCEPT { return root(); }

    ctoon_doc  *raw()        const CTOON_NOEXCEPT { return doc_; }
    std::size_t read_size()  const CTOON_NOEXCEPT { return ctoon_doc_get_read_size(doc_); } ///< Bytes consumed during parse.
    std::size_t val_count()  const CTOON_NOEXCEPT { return ctoon_doc_get_val_count(doc_); } ///< Number of values in the pool.

    value get_pointer(const string_view &ptr) const CTOON_NOEXCEPT {
        return value(ctoon_doc_ptr_getn(doc_, ptr.data(), ptr.size()));
    }

    /* --- TOON serialisation --------------------------------------------- */
    /**
     * @brief Serialise to TOON.
     * @param opts Write options (default: 2-space indent, comma delimiter).
     * @throws ctoon::write_error on failure.
     */
    write_result write(const write_options &opts = write_options()) const {
        ctoon_write_options co = opts.to_c();
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_write_opts(doc_, &co, NULL, &len, &err);
        if (!raw) throw write_error(err);
        return write_result(raw, len);
    }

    /**
     * @brief Write TOON to a file.
     * @throws ctoon::write_error on failure.
     */
    bool write_file(const char *path,
                    const write_options &opts = write_options()) const {
        ctoon_write_options co = opts.to_c();
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        if (!ctoon_write_file(path, doc_, &co, NULL, &err)) throw write_error(err);
        return true;
    }

    /* --- JSON serialisation --------------------------------------------- */
    /**
     * @brief Serialise to JSON.
     * @param indent  Spaces per indent level. 0 = compact (no whitespace).
     * @param flags   Subset of @c CTOON_WRITE_* flags accepted by the JSON
     *                writer (e.g. @c CTOON_WRITE_ESCAPE_UNICODE).
     * @throws ctoon::write_error on failure.
     */
    write_result to_json(int indent = 2,
                         ctoon_write_flag flags = CTOON_WRITE_NOFLAG) const {
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_doc_to_json(doc_, indent, flags, NULL, &len, &err);
        if (!raw) throw write_error(err);
        return write_result(raw, len);
    }

    /* --- convert to mutable --------------------------------------------- */
    /// Create a deep mutable copy.
    /// @throws std::bad_alloc on allocation failure.
    mut_document mut_copy() const;   /* defined after mut_document */

    ctoon_doc *release() CTOON_NOEXCEPT { ctoon_doc *d = doc_; doc_ = NULL; return d; }

    ctoon_doc *doc_;
};

/* -----------------------------------------------------------------------
 * mut_document  –  RAII owner of ctoon_mut_doc*
 * -------------------------------------------------------------------- */

/**
 * @brief RAII owner of a mutable TOON document.
 *
 * Create with @c mut_document::create() or @c ctoon::make_document().
 *
 * @par Example
 * @code
 * ctoon::mut_document doc = ctoon::make_document();
 * ctoon::mut_value root = doc.make_obj();
 * doc.set_root(root);
 * root.obj_add(doc.make_str("x"), doc.make_uint(42));
 * std::cout << doc.write().c_str()    << "\n"; // x: 42
 * std::cout << doc.to_json(0).c_str() << "\n"; // {"x":42}
 * @endcode
 */
class mut_document {
public:
    mut_document() CTOON_NOEXCEPT : doc_(NULL) {}
    explicit mut_document(ctoon_mut_doc *doc) CTOON_NOEXCEPT : doc_(doc) {}
    ~mut_document() { ctoon_mut_doc_free(doc_); }

    mut_document(mut_document &&o) CTOON_NOEXCEPT : doc_(o.doc_) { o.doc_ = NULL; }
    mut_document &operator=(mut_document &&o) CTOON_NOEXCEPT {
        if (this != &o) { ctoon_mut_doc_free(doc_); doc_ = o.doc_; o.doc_ = NULL; }
        return *this;
    }

    /**
     * @brief Create an empty mutable document.
     * @throws std::bad_alloc on allocation failure.
     */
    static mut_document create() {
        ctoon_mut_doc *d = ctoon_mut_doc_new(NULL);
        if (!d) throw std::bad_alloc();
        return mut_document(d);
    }

    /* --- access --------------------------------------------------------- */
    bool    valid()    const CTOON_NOEXCEPT { return doc_ != NULL; }
    operator bool()   const CTOON_NOEXCEPT { return valid(); }

    mut_value root()   const CTOON_NOEXCEPT { return mut_value(ctoon_mut_doc_get_root(doc_)); }
    mut_value operator*() const CTOON_NOEXCEPT { return root(); }

    /// Set the root value.
    void set_root(mut_value v) CTOON_NOEXCEPT { ctoon_mut_doc_set_root(doc_, v.raw()); }

    ctoon_mut_doc *raw() const CTOON_NOEXCEPT { return doc_; }

    mut_value get_pointer(const string_view &ptr) const CTOON_NOEXCEPT {
        return mut_value(ctoon_mut_doc_ptr_getn(doc_, ptr.data(), ptr.size()));
    }

    /* --- value factories ------------------------------------------------ */
    /// @name Value factories
    /// All @c make_* calls allocate a value owned by this document.
    /// @{
    mut_value make_null()                  CTOON_NOEXCEPT { return mut_value(ctoon_mut_null(doc_)); }
    mut_value make_true()                  CTOON_NOEXCEPT { return mut_value(ctoon_mut_true(doc_)); }
    mut_value make_false()                 CTOON_NOEXCEPT { return mut_value(ctoon_mut_false(doc_)); }
    mut_value make_bool(bool v)            CTOON_NOEXCEPT { return mut_value(ctoon_mut_bool(doc_, v)); }
    mut_value make_uint(std::uint64_t v)   CTOON_NOEXCEPT { return mut_value(ctoon_mut_uint(doc_, v)); }
    mut_value make_sint(std::int64_t  v)   CTOON_NOEXCEPT { return mut_value(ctoon_mut_sint(doc_, v)); }
    mut_value make_real(double v)          CTOON_NOEXCEPT { return mut_value(ctoon_mut_real(doc_, v)); }
    mut_value make_arr()                   CTOON_NOEXCEPT { return mut_value(ctoon_mut_arr(doc_)); }
    mut_value make_obj()                   CTOON_NOEXCEPT { return mut_value(ctoon_mut_obj(doc_)); }
    mut_value make_str(const char *s)      CTOON_NOEXCEPT { return mut_value(ctoon_mut_strcpy(doc_, s)); }
    mut_value make_str(const std::string &s) CTOON_NOEXCEPT {
        return mut_value(ctoon_mut_strncpy(doc_, s.data(), s.size()));
    }
    mut_value make_str(const string_view &s) CTOON_NOEXCEPT {
        return mut_value(ctoon_mut_strncpy(doc_, s.data(), s.size()));
    }
    /// @}

    /* --- TOON serialisation --------------------------------------------- */
    /**
     * @brief Serialise to TOON.
     * @throws ctoon::write_error on failure.
     */
    write_result write(const write_options &opts = write_options()) const {
        ctoon_write_options co = opts.to_c();
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_mut_write_opts(doc_, &co, NULL, &len, &err);
        if (!raw) throw write_error(err);
        return write_result(raw, len);
    }

    /**
     * @brief Write TOON to a file.
     * @throws ctoon::write_error on failure.
     */
    bool write_file(const char *path,
                    const write_options &opts = write_options()) const {
        ctoon_write_options co = opts.to_c();
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        if (!ctoon_mut_write_file(path, doc_, &co, NULL, &err)) throw write_error(err);
        return true;
    }

    /* --- JSON serialisation --------------------------------------------- */
    /**
     * @brief Serialise to JSON.
     * @param indent  Spaces per indent level. 0 = compact.
     * @param flags   @c CTOON_WRITE_* flags subset for JSON.
     * @throws ctoon::write_error on failure.
     */
    write_result to_json(int indent = 2,
                         ctoon_write_flag flags = CTOON_WRITE_NOFLAG) const {
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_mut_doc_to_json(doc_, indent, flags, NULL, &len, &err);
        if (!raw) throw write_error(err);
        return write_result(raw, len);
    }

    ctoon_mut_doc *release() CTOON_NOEXCEPT { ctoon_mut_doc *d = doc_; doc_ = NULL; return d; }

    ctoon_mut_doc *doc_;
};

/* --- deferred: document::mut_copy ---------------------------------------- */

inline mut_document document::mut_copy() const {
    ctoon_mut_doc *d = ctoon_doc_mut_copy(doc_, NULL);
    if (!d) throw std::bad_alloc();
    return mut_document(d);
}

/* -----------------------------------------------------------------------
 * Free-function API
 * -------------------------------------------------------------------- */

/**
 * @brief Parse a null-terminated TOON string.
 * @throws ctoon::parse_error on failure.
 */
inline document parse(const char *toon,
                       ctoon_read_flag flags = CTOON_READ_NOFLAG) {
    return document::parse(toon, flags);
}

/**
 * @brief Parse a TOON string from a std::string.
 * @throws ctoon::parse_error on failure.
 */
inline document parse(const std::string &toon,
                       ctoon_read_flag flags = CTOON_READ_NOFLAG) {
    return document::parse(toon, flags);
}

/**
 * @brief Parse a TOON file from disk.
 * @throws ctoon::parse_error on failure.
 */
inline document parse_file(const char *path,
                             ctoon_read_flag flags = CTOON_READ_NOFLAG) {
    return document::parse_file(path, flags);
}

/// Create a new empty mutable document.
/// @throws std::bad_alloc on allocation failure.
inline mut_document make_document() { return mut_document::create(); }

/// Serialise an immutable document to TOON.  @throws ctoon::write_error.
inline write_result write(const document &d,
                           const write_options &o = write_options()) { return d.write(o); }

/// Serialise a mutable document to TOON.  @throws ctoon::write_error.
inline write_result write(const mut_document &d,
                           const write_options &o = write_options()) { return d.write(o); }

/**
 * @brief Serialise a document to JSON.
 * @param d       Source document.
 * @param indent  Spaces per indent level. 0 = compact.
 * @param flags   @c CTOON_WRITE_* flags.
 * @throws ctoon::write_error on failure.
 */
inline write_result to_json(const document &d,
                              int indent = 2,
                              ctoon_write_flag flags = CTOON_WRITE_NOFLAG) {
    return d.to_json(indent, flags);
}

/// @overload
inline write_result to_json(const mut_document &d,
                              int indent = 2,
                              ctoon_write_flag flags = CTOON_WRITE_NOFLAG) {
    return d.to_json(indent, flags);
}

} // namespace ctoon
