/**
 * @file ctoon.hpp
 * @brief C++11 binding for the CToon library.
 *
 * Include-only wrapper around ctoon.h.
 * Everything lives in namespace @c ctoon.
 * Naming follows the STL snake_case convention.
 *
 * @par Class hierarchy
 * | Class               | Owns memory | Underlying type     |
 * |---------------------|-------------|---------------------|
 * | ctoon::value        | no          | ctoon_val*          |
 * | ctoon::mut_value    | no          | ctoon_mut_val*      |
 * | ctoon::document     | yes         | ctoon_doc*          |
 * | ctoon::mut_document | yes         | ctoon_mut_doc*      |
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

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#   include <string_view>
    namespace ctoon { using string_view = std::string_view; }
#else
    namespace ctoon {
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
        std::string str()   const { return std::string(d_, n_); }
        bool operator==(const string_view &o) const CTOON_NOEXCEPT {
            return n_ == o.n_ && std::memcmp(d_, o.d_, n_) == 0;
        }
        bool operator!=(const string_view &o) const CTOON_NOEXCEPT { return !(*this == o); }
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

class version {
public:
    static unsigned int major()  CTOON_NOEXCEPT { return CTOON_VERSION_MAJOR; }
    static unsigned int minor()  CTOON_NOEXCEPT { return CTOON_VERSION_MINOR; }
    static unsigned int patch()  CTOON_NOEXCEPT { return CTOON_VERSION_PATCH; }
    static unsigned int hex()    CTOON_NOEXCEPT { return CTOON_VERSION_HEX; }
    static string_view  string() CTOON_NOEXCEPT { return CTOON_VERSION_STRING; }
private:
    version();
};

/* -----------------------------------------------------------------------
 * Exception hierarchy
 * -------------------------------------------------------------------- */

class error : public std::exception {
public:
    explicit error(const std::string &msg) CTOON_NOEXCEPT : msg_(msg) {}
    virtual const char *what() const CTOON_NOEXCEPT { return msg_.c_str(); }
    virtual ~error() CTOON_NOEXCEPT {}
protected:
    std::string msg_;
};

class parse_error : public error {
public:
    ctoon_read_code code;
    std::size_t     pos;
    explicit parse_error(const ctoon_read_err &e)
        : error(build_msg(e)), code(e.code), pos(e.pos) {}
    virtual ~parse_error() CTOON_NOEXCEPT {}
private:
    static std::string build_msg(const ctoon_read_err &e) {
        std::string m(e.msg ? e.msg : "parse error");
        char buf[32]; std::sprintf(buf, " (pos %zu)", e.pos);
        return m + buf;
    }
};

class write_error : public error {
public:
    ctoon_write_code code;
    explicit write_error(const ctoon_write_err &e)
        : error(e.msg ? e.msg : "write error"), code(e.code) {}
    virtual ~write_error() CTOON_NOEXCEPT {}
};

/* -----------------------------------------------------------------------
 * write_result
 * -------------------------------------------------------------------- */

class write_result {
public:
    write_result() CTOON_NOEXCEPT : ptr_(NULL), len_(0) {}
    write_result(char *p, std::size_t n) CTOON_NOEXCEPT : ptr_(p), len_(n) {}
    ~write_result() { std::free(ptr_); }
    write_result(write_result &&o) CTOON_NOEXCEPT
        : ptr_(o.ptr_), len_(o.len_) { o.ptr_ = NULL; o.len_ = 0; }
    write_result &operator=(write_result &&o) CTOON_NOEXCEPT {
        if (this != &o) { std::free(ptr_); ptr_ = o.ptr_; len_ = o.len_; o.ptr_ = NULL; o.len_ = 0; }
        return *this;
    }
    const char  *c_str() const CTOON_NOEXCEPT { return ptr_ ? ptr_ : ""; }
    std::size_t  size()  const CTOON_NOEXCEPT { return len_; }
    bool         empty() const CTOON_NOEXCEPT { return len_ == 0; }
    string_view  view()  const CTOON_NOEXCEPT { return string_view(ptr_ ? ptr_ : "", len_); }
    std::string  str()   const { return std::string(ptr_ ? ptr_ : "", len_); }
    char        *ptr_;
    std::size_t  len_;
};

/* -----------------------------------------------------------------------
 * delimiter  –  shadows ctoon_delimiter
 * -------------------------------------------------------------------- */

enum class delimiter : uint32_t {
    COMMA = CTOON_DELIMITER_COMMA,
    TAB   = CTOON_DELIMITER_TAB,
    PIPE  = CTOON_DELIMITER_PIPE,
};

/* -----------------------------------------------------------------------
 * write_flag  –  shadows ctoon_write_flag
 *
 * Use these directly wherever ctoon_write_flag was used.
 * Implicit cast to ctoon_write_flag is provided via to_c().
 * -------------------------------------------------------------------- */

enum class write_flag : uint32_t {
    NOFLAG                = static_cast<uint32_t>(CTOON_WRITE_NOFLAG),
    ESCAPE_UNICODE        = static_cast<uint32_t>(CTOON_WRITE_ESCAPE_UNICODE),
    ESCAPE_SLASHES        = static_cast<uint32_t>(CTOON_WRITE_ESCAPE_SLASHES),
    ALLOW_INF_AND_NAN     = static_cast<uint32_t>(CTOON_WRITE_ALLOW_INF_AND_NAN),
    INF_AND_NAN_AS_NULL   = static_cast<uint32_t>(CTOON_WRITE_INF_AND_NAN_AS_NULL),
    ALLOW_INVALID_UNICODE = static_cast<uint32_t>(CTOON_WRITE_ALLOW_INVALID_UNICODE),
    LENGTH_MARKER         = static_cast<uint32_t>(CTOON_WRITE_LENGTH_MARKER),
    NEWLINE_AT_END        = static_cast<uint32_t>(CTOON_WRITE_NEWLINE_AT_END),
};

inline write_flag operator|(write_flag l, write_flag r) CTOON_NOEXCEPT {
    return static_cast<write_flag>(static_cast<uint32_t>(l) | static_cast<uint32_t>(r));
}
inline write_flag &operator|=(write_flag &l, write_flag r) CTOON_NOEXCEPT {
    l = l | r; return l;
}
/// Convert to the C type for passing to C API.
inline ctoon_write_flag to_c(write_flag f) CTOON_NOEXCEPT {
    return static_cast<ctoon_write_flag>(static_cast<uint32_t>(f));
}

/* -----------------------------------------------------------------------
 * read_flag  –  shadows ctoon_read_flag
 * -------------------------------------------------------------------- */

enum class read_flag : uint32_t {
    NOFLAG                  = static_cast<uint32_t>(CTOON_READ_NOFLAG),
    INSITU                  = static_cast<uint32_t>(CTOON_READ_INSITU),
    STOP_WHEN_DONE          = static_cast<uint32_t>(CTOON_READ_STOP_WHEN_DONE),
    ALLOW_TRAILING_COMMAS   = static_cast<uint32_t>(CTOON_READ_ALLOW_TRAILING_COMMAS),
    ALLOW_COMMENTS          = static_cast<uint32_t>(CTOON_READ_ALLOW_COMMENTS),
    ALLOW_INF_AND_NAN       = static_cast<uint32_t>(CTOON_READ_ALLOW_INF_AND_NAN),
    NUMBER_AS_RAW           = static_cast<uint32_t>(CTOON_READ_NUMBER_AS_RAW),
    ALLOW_INVALID_UNICODE   = static_cast<uint32_t>(CTOON_READ_ALLOW_INVALID_UNICODE),
    BIGNUM_AS_RAW           = static_cast<uint32_t>(CTOON_READ_BIGNUM_AS_RAW),
    ALLOW_BOM               = static_cast<uint32_t>(CTOON_READ_ALLOW_BOM),
    ALLOW_EXT_NUMBER        = static_cast<uint32_t>(CTOON_READ_ALLOW_EXT_NUMBER),
    ALLOW_EXT_ESCAPE        = static_cast<uint32_t>(CTOON_READ_ALLOW_EXT_ESCAPE),
    ALLOW_EXT_WHITESPACE    = static_cast<uint32_t>(CTOON_READ_ALLOW_EXT_WHITESPACE),
    ALLOW_SINGLE_QUOTED_STR = static_cast<uint32_t>(CTOON_READ_ALLOW_SINGLE_QUOTED_STR),
    ALLOW_UNQUOTED_KEY      = static_cast<uint32_t>(CTOON_READ_ALLOW_UNQUOTED_KEY),
};

inline read_flag operator|(read_flag l, read_flag r) CTOON_NOEXCEPT {
    return static_cast<read_flag>(static_cast<uint32_t>(l) | static_cast<uint32_t>(r));
}
inline read_flag &operator|=(read_flag &l, read_flag r) CTOON_NOEXCEPT {
    l = l | r; return l;
}
/// Convert to the C type for passing to C API.
inline ctoon_read_flag to_c(read_flag f) CTOON_NOEXCEPT {
    return static_cast<ctoon_read_flag>(static_cast<uint32_t>(f));
}

/* -----------------------------------------------------------------------
 * write_options  –  builder pattern
 * -------------------------------------------------------------------- */

class write_options {
public:
    write_options() CTOON_NOEXCEPT
        : flag_(write_flag::NOFLAG), delimiter_(delimiter::COMMA), indent_(2) {}

    write_options &with_flag(write_flag f)    CTOON_NOEXCEPT { flag_      = f; return *this; }
    write_options &with_delimiter(delimiter d) CTOON_NOEXCEPT { delimiter_ = d; return *this; }
    write_options &with_indent(int i)          CTOON_NOEXCEPT { indent_    = i; return *this; }

    write_flag flag()           const CTOON_NOEXCEPT { return flag_;      }
    delimiter  get_delimiter()  const CTOON_NOEXCEPT { return delimiter_; }
    int        indent()         const CTOON_NOEXCEPT { return indent_;    }

    ctoon_write_options to_c() const CTOON_NOEXCEPT {
        ctoon_write_options o;
        o.flag      = ctoon::to_c(flag_);
        o.delimiter = static_cast<ctoon_delimiter>(delimiter_);
        o.indent    = indent_;
        return o;
    }
private:
    write_flag flag_;
    delimiter  delimiter_;
    int        indent_;
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

class value {
public:
    value() CTOON_NOEXCEPT : v_(NULL) {}
    explicit value(ctoon_val *v) CTOON_NOEXCEPT : v_(v) {}

    ctoon_val *raw()   const CTOON_NOEXCEPT { return v_; }
    bool       valid() const CTOON_NOEXCEPT { return v_ != NULL; }
    operator   bool()  const CTOON_NOEXCEPT { return valid(); }

    /* --- type predicates --- */
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

    /* --- scalar getters --- */
    bool          get_bool() const CTOON_NOEXCEPT { return v_ && ctoon_get_bool(v_);      }
    std::uint64_t get_uint() const CTOON_NOEXCEPT { return v_ ? ctoon_get_uint(v_) : 0;   }
    std::int64_t  get_sint() const CTOON_NOEXCEPT { return v_ ? ctoon_get_sint(v_) : 0;   }
    double        get_real() const CTOON_NOEXCEPT { return v_ ? ctoon_get_real(v_) : 0.0; }
    double        get_num()  const CTOON_NOEXCEPT { return v_ ? ctoon_get_num(v_)  : 0.0; }
    std::size_t   get_len()  const CTOON_NOEXCEPT { return v_ ? ctoon_get_len(v_)  : 0;   }
    string_view   get_str()  const CTOON_NOEXCEPT {
        if (!v_) return string_view();
        const char *s = ctoon_get_str(v_);
        return s ? string_view(s, ctoon_get_len(v_)) : string_view();
    }

    /* --- array access --- */
    std::size_t arr_size()           const CTOON_NOEXCEPT { return v_ ? ctoon_arr_size(v_) : 0; }
    value arr_get(std::size_t i)     const CTOON_NOEXCEPT { return value(v_ ? ctoon_arr_get(v_, i) : NULL); }
    value arr_first()                const CTOON_NOEXCEPT { return value(v_ ? ctoon_arr_get_first(v_) : NULL); }
    value arr_last()                 const CTOON_NOEXCEPT { return value(v_ ? ctoon_arr_get_last(v_)  : NULL); }
    value operator[](std::size_t i)  const CTOON_NOEXCEPT { return arr_get(i); }

    /* --- object access --- */
    std::size_t obj_size()                   const CTOON_NOEXCEPT { return v_ ? ctoon_obj_size(v_) : 0; }
    value obj_get(const char *key)           const CTOON_NOEXCEPT { return value(v_ ? ctoon_obj_get(v_, key) : NULL); }
    value obj_get(const string_view &key)    const CTOON_NOEXCEPT {
        return value(v_ ? ctoon_obj_getn(v_, key.data(), key.size()) : NULL);
    }
    value operator[](const char *key)        const CTOON_NOEXCEPT { return obj_get(key); }
    value operator[](const string_view &key) const CTOON_NOEXCEPT { return obj_get(key); }

    /* --- equality --- */
    bool equals(const value &rhs)      const CTOON_NOEXCEPT { return v_ && rhs.v_ && ctoon_equals(v_, rhs.v_); }
    bool equals(const char *s)         const CTOON_NOEXCEPT { return v_ && ctoon_equals_str(v_, s); }
    bool equals(const string_view &s)  const CTOON_NOEXCEPT { return v_ && ctoon_equals_strn(v_, s.data(), s.size()); }

    /* --- pointer path --- */
    value get_pointer(const string_view &ptr) const CTOON_NOEXCEPT {
        return value(v_ ? ctoon_ptr_getn(v_, ptr.data(), ptr.size()) : NULL);
    }

    /* --- serialisation --- */
    write_result write(const write_options &opts = write_options()) const {
        ctoon_write_options co = opts.to_c();
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_val_write_opts(v_, &co, NULL, &len, &err);
        if (!raw) throw write_error(err);
        return write_result(raw, len);
    }

    /* --- iterators --- */
    /// @cond INTERNAL
    class arr_iterator {
    public:
        explicit arr_iterator(ctoon_val *arr, bool end = false) : cur_(NULL) {
            if (!arr || end) { std::memset(&iter_, 0, sizeof(iter_)); return; }
            ctoon_arr_iter_init(arr, &iter_);
            cur_ = ctoon_arr_iter_next(&iter_);
        }
        value         operator*()  const CTOON_NOEXCEPT { return value(cur_); }
        arr_iterator &operator++() CTOON_NOEXCEPT { cur_ = ctoon_arr_iter_next(&iter_); return *this; }
        bool          operator!=(const arr_iterator &o) const CTOON_NOEXCEPT { return cur_ != o.cur_; }
    private:
        ctoon_arr_iter iter_;
        ctoon_val     *cur_;
    };
    struct arr_range {
        ctoon_val *arr;
        arr_iterator begin() const { return arr_iterator(arr); }
        arr_iterator end()   const { return arr_iterator(arr, true); }
    };
    arr_range arr_items() const CTOON_NOEXCEPT { arr_range r; r.arr = v_; return r; }

    struct kv_pair {
        ctoon_val *key_raw, *val_raw;
        value key() const { return value(key_raw); }
        value val() const { return value(val_raw); }
    };
    class obj_iterator {
    public:
        explicit obj_iterator(ctoon_val *obj, bool end = false) : key_(NULL) {
            if (!obj || end) { std::memset(&iter_, 0, sizeof(iter_)); return; }
            ctoon_obj_iter_init(obj, &iter_);
            key_ = ctoon_obj_iter_next(&iter_);
        }
        kv_pair       operator*()  const CTOON_NOEXCEPT {
            kv_pair kv; kv.key_raw = key_; kv.val_raw = ctoon_obj_iter_get_val(key_); return kv;
        }
        obj_iterator &operator++() CTOON_NOEXCEPT { key_ = ctoon_obj_iter_next(&iter_); return *this; }
        bool          operator!=(const obj_iterator &o) const CTOON_NOEXCEPT { return key_ != o.key_; }
    private:
        ctoon_obj_iter iter_;
        ctoon_val     *key_;
    };
    struct obj_range {
        ctoon_val *obj;
        obj_iterator begin() const { return obj_iterator(obj); }
        obj_iterator end()   const { return obj_iterator(obj, true); }
    };
    obj_range obj_items() const CTOON_NOEXCEPT { obj_range r; r.obj = v_; return r; }
    /// @endcond

protected:
    ctoon_val *v_;
};

/* -----------------------------------------------------------------------
 * mut_value  –  non-owning mutable view over ctoon_mut_val*
 * -------------------------------------------------------------------- */

class mut_value {
public:
    mut_value() CTOON_NOEXCEPT : v_(NULL) {}
    explicit mut_value(ctoon_mut_val *v) CTOON_NOEXCEPT : v_(v) {}

    ctoon_mut_val *raw()   const CTOON_NOEXCEPT { return v_; }
    bool           valid() const CTOON_NOEXCEPT { return v_ != NULL; }
    operator       bool()  const CTOON_NOEXCEPT { return valid(); }

    /* --- type predicates --- */
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

    /* --- scalar getters --- */
    bool          get_bool() const CTOON_NOEXCEPT { return v_ && ctoon_mut_get_bool(v_);      }
    std::uint64_t get_uint() const CTOON_NOEXCEPT { return v_ ? ctoon_mut_get_uint(v_) : 0;   }
    std::int64_t  get_sint() const CTOON_NOEXCEPT { return v_ ? ctoon_mut_get_sint(v_) : 0;   }
    double        get_real() const CTOON_NOEXCEPT { return v_ ? ctoon_mut_get_real(v_) : 0.0; }
    double        get_num()  const CTOON_NOEXCEPT { return v_ ? ctoon_mut_get_num(v_)  : 0.0; }
    std::size_t   get_len()  const CTOON_NOEXCEPT { return v_ ? ctoon_mut_get_len(v_)  : 0;   }
    string_view   get_str()  const CTOON_NOEXCEPT {
        if (!v_) return string_view();
        const char *s = ctoon_mut_get_str(v_);
        return s ? string_view(s, ctoon_mut_get_len(v_)) : string_view();
    }

    /* --- array access --- */
    std::size_t arr_size()              const CTOON_NOEXCEPT { return v_ ? ctoon_mut_arr_size(v_) : 0; }
    mut_value arr_get(std::size_t i)    const CTOON_NOEXCEPT { return mut_value(v_ ? ctoon_mut_arr_get(v_, i) : NULL); }
    mut_value arr_first()               const CTOON_NOEXCEPT { return mut_value(v_ ? ctoon_mut_arr_get_first(v_) : NULL); }
    mut_value arr_last()                const CTOON_NOEXCEPT { return mut_value(v_ ? ctoon_mut_arr_get_last(v_)  : NULL); }
    mut_value operator[](std::size_t i) const CTOON_NOEXCEPT { return arr_get(i); }

    /* --- object access --- */
    std::size_t obj_size()                       const CTOON_NOEXCEPT { return v_ ? ctoon_mut_obj_size(v_) : 0; }
    mut_value obj_get(const char *key)           const CTOON_NOEXCEPT { return mut_value(v_ ? ctoon_mut_obj_get(v_, key) : NULL); }
    mut_value obj_get(const string_view &key)    const CTOON_NOEXCEPT {
        return mut_value(v_ ? ctoon_mut_obj_getn(v_, key.data(), key.size()) : NULL);
    }
    mut_value operator[](const char *key)        const CTOON_NOEXCEPT { return obj_get(key); }
    mut_value operator[](const string_view &key) const CTOON_NOEXCEPT { return obj_get(key); }

    /* --- equality --- */
    bool equals(const mut_value &rhs)   const CTOON_NOEXCEPT { return v_ && rhs.v_ && ctoon_mut_equals(v_, rhs.v_); }
    bool equals(const char *s)          const CTOON_NOEXCEPT { return v_ && ctoon_mut_equals_str(v_, s); }
    bool equals(const string_view &s)   const CTOON_NOEXCEPT { return v_ && ctoon_mut_equals_strn(v_, s.data(), s.size()); }

    /* --- pointer --- */
    mut_value get_pointer(const string_view &ptr) const CTOON_NOEXCEPT {
        return mut_value(v_ ? ctoon_mut_ptr_getn(v_, ptr.data(), ptr.size()) : NULL);
    }

    /* --- array mutation --- */
    bool arr_append(mut_value item)  const CTOON_NOEXCEPT { return v_ && item.v_ && ctoon_mut_arr_append(v_, item.v_); }
    bool arr_prepend(mut_value item) const CTOON_NOEXCEPT { return v_ && item.v_ && ctoon_mut_arr_prepend(v_, item.v_); }
    bool arr_clear()                 const CTOON_NOEXCEPT { return v_ && ctoon_mut_arr_clear(v_); }
    mut_value arr_remove(std::size_t i) const CTOON_NOEXCEPT {
        return mut_value(v_ ? ctoon_mut_arr_remove(v_, i) : NULL);
    }

    /* --- object mutation --- */
    bool obj_add(mut_value key, mut_value val) const CTOON_NOEXCEPT {
        return v_ && key.v_ && val.v_ && ctoon_mut_obj_add(v_, key.v_, val.v_);
    }
    bool obj_put(mut_value key, mut_value val) const CTOON_NOEXCEPT {
        return v_ && key.v_ && val.v_ && ctoon_mut_obj_put(v_, key.v_, val.v_);
    }
    bool obj_clear() const CTOON_NOEXCEPT { return v_ && ctoon_mut_obj_clear(v_); }
    mut_value obj_remove(const string_view &key) const CTOON_NOEXCEPT {
        return mut_value(v_ ? ctoon_mut_obj_remove_keyn(v_, key.data(), key.size()) : NULL);
    }

    /* --- iterators --- */
    /// @cond INTERNAL
    class arr_iterator {
    public:
        explicit arr_iterator(ctoon_mut_val *arr, bool end = false) : cur_(NULL) {
            if (!arr || end) { std::memset(&iter_, 0, sizeof(iter_)); return; }
            ctoon_mut_arr_iter_init(arr, &iter_);
            cur_ = ctoon_mut_arr_iter_next(&iter_);
        }
        mut_value     operator*()  const CTOON_NOEXCEPT { return mut_value(cur_); }
        arr_iterator &operator++() CTOON_NOEXCEPT { cur_ = ctoon_mut_arr_iter_next(&iter_); return *this; }
        bool          operator!=(const arr_iterator &o) const CTOON_NOEXCEPT { return cur_ != o.cur_; }
    private:
        ctoon_mut_arr_iter iter_;
        ctoon_mut_val     *cur_;
    };
    struct arr_range {
        ctoon_mut_val *arr;
        arr_iterator begin() const { return arr_iterator(arr); }
        arr_iterator end()   const { return arr_iterator(arr, true); }
    };
    arr_range arr_items() const CTOON_NOEXCEPT { arr_range r; r.arr = v_; return r; }

    struct kv_pair {
        ctoon_mut_val *key_raw, *val_raw;
        mut_value key() const { return mut_value(key_raw); }
        mut_value val() const { return mut_value(val_raw); }
    };
    class obj_iterator {
    public:
        explicit obj_iterator(ctoon_mut_val *obj, bool end = false) : key_(NULL) {
            if (!obj || end) { std::memset(&iter_, 0, sizeof(iter_)); return; }
            ctoon_mut_obj_iter_init(obj, &iter_);
            key_ = ctoon_mut_obj_iter_next(&iter_);
        }
        kv_pair       operator*()  const CTOON_NOEXCEPT {
            kv_pair kv; kv.key_raw = key_; kv.val_raw = ctoon_mut_obj_iter_get_val(key_); return kv;
        }
        obj_iterator &operator++() CTOON_NOEXCEPT { key_ = ctoon_mut_obj_iter_next(&iter_); return *this; }
        bool          operator!=(const obj_iterator &o) const CTOON_NOEXCEPT { return key_ != o.key_; }
    private:
        ctoon_mut_obj_iter iter_;
        ctoon_mut_val     *key_;
    };
    struct obj_range {
        ctoon_mut_val *obj;
        obj_iterator begin() const { return obj_iterator(obj); }
        obj_iterator end()   const { return obj_iterator(obj, true); }
    };
    obj_range obj_items() const CTOON_NOEXCEPT { obj_range r; r.obj = v_; return r; }
    /// @endcond

protected:
    ctoon_mut_val *v_;
};

/* -----------------------------------------------------------------------
 * document  –  RAII owner of ctoon_doc*
 * -------------------------------------------------------------------- */

class document {
public:
    document() CTOON_NOEXCEPT : doc_(NULL) {}
    explicit document(ctoon_doc *doc) CTOON_NOEXCEPT : doc_(doc) {}
    ~document() { ctoon_doc_free(doc_); }

    document(document &&o) CTOON_NOEXCEPT : doc_(o.doc_) { o.doc_ = NULL; }
    document &operator=(document &&o) CTOON_NOEXCEPT {
        if (this != &o) { ctoon_doc_free(doc_); doc_ = o.doc_; o.doc_ = NULL; }
        return *this;
    }

    /* --- parse factories --- */
    static document parse(const char *toon, std::size_t len,
                           read_flag flags = read_flag::NOFLAG) {
        ctoon_read_err err; std::memset(&err, 0, sizeof(err));
        ctoon_doc *d = ctoon_read_opts(const_cast<char *>(toon), len, to_c(flags), NULL, &err);
        if (!d) throw parse_error(err);
        return document(d);
    }
    static document parse(const char *toon, read_flag flags = read_flag::NOFLAG) {
        return parse(toon, std::strlen(toon), flags);
    }
    static document parse(const std::string &toon, read_flag flags = read_flag::NOFLAG) {
        return parse(toon.data(), toon.size(), flags);
    }
    static document parse(const string_view &toon, read_flag flags = read_flag::NOFLAG) {
        return parse(toon.data(), toon.size(), flags);
    }
    static document parse_file(const char *path, read_flag flags = read_flag::NOFLAG) {
        ctoon_read_err err; std::memset(&err, 0, sizeof(err));
        ctoon_doc *d = ctoon_read_file(path, to_c(flags), NULL, &err);
        if (!d) throw parse_error(err);
        return document(d);
    }

#if defined(CTOON_ENABLE_JSON) && CTOON_ENABLE_JSON
    static document from_json(const char *json, std::size_t len,
                               read_flag flags = read_flag::NOFLAG) {
        ctoon_read_err err; std::memset(&err, 0, sizeof(err));
        ctoon_doc *d = ctoon_read_json(const_cast<char *>(json), len, to_c(flags), NULL, &err);
        if (!d) throw parse_error(err);
        return document(d);
    }
    static document from_json(const char *json, read_flag flags = read_flag::NOFLAG) {
        return from_json(json, std::strlen(json), flags);
    }
    static document from_json(const std::string &json, read_flag flags = read_flag::NOFLAG) {
        return from_json(json.data(), json.size(), flags);
    }
    static document from_json(const string_view &json, read_flag flags = read_flag::NOFLAG) {
        return from_json(json.data(), json.size(), flags);
    }
    static document from_json_file(const char *path, read_flag flags = read_flag::NOFLAG) {
        ctoon_read_err err; std::memset(&err, 0, sizeof(err));
        ctoon_doc *d = ctoon_read_json_file(path, to_c(flags), NULL, &err);
        if (!d) throw parse_error(err);
        return document(d);
    }
#endif /* CTOON_ENABLE_JSON */

    /* --- access --- */
    bool        valid()      const CTOON_NOEXCEPT { return doc_ != NULL; }
    operator    bool()       const CTOON_NOEXCEPT { return valid(); }
    value       root()       const CTOON_NOEXCEPT { return value(ctoon_doc_get_root(doc_)); }
    value       operator*()  const CTOON_NOEXCEPT { return root(); }
    ctoon_doc  *raw()        const CTOON_NOEXCEPT { return doc_; }
    std::size_t read_size()  const CTOON_NOEXCEPT { return ctoon_doc_get_read_size(doc_); }
    std::size_t val_count()  const CTOON_NOEXCEPT { return ctoon_doc_get_val_count(doc_); }

    value get_pointer(const string_view &ptr) const CTOON_NOEXCEPT {
        return value(ctoon_doc_ptr_getn(doc_, ptr.data(), ptr.size()));
    }

    /* --- TOON serialisation --- */
    write_result write(const write_options &opts = write_options()) const {
        ctoon_write_options co = opts.to_c();
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_write_opts(doc_, &co, NULL, &len, &err);
        if (!raw) throw write_error(err);
        return write_result(raw, len);
    }
    void write_file(const char *path,
                    const write_options &opts = write_options()) const {
        ctoon_write_options co = opts.to_c();
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        if (!ctoon_write_file(path, doc_, &co, NULL, &err)) throw write_error(err);
    }

    /* --- JSON serialisation --- */
#if defined(CTOON_ENABLE_JSON) && CTOON_ENABLE_JSON
    write_result to_json(int indent = 2,
                         write_flag flags = write_flag::NOFLAG) const {
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_doc_to_json(doc_, indent, to_c(flags), NULL, &len, &err);
        if (!raw) throw write_error(err);
        return write_result(raw, len);
    }
    void to_json_file(const char *path, int indent = 2,
                      write_flag flags = write_flag::NOFLAG) const {
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_doc_to_json(doc_, indent, to_c(flags), NULL, &len, &err);
        if (!raw) throw write_error(err);
        FILE *fp = std::fopen(path, "wb");
        if (!fp) { std::free(raw); throw write_error(err); }
        std::fwrite(raw, 1, len, fp);
        std::fclose(fp);
        std::free(raw);
    }
#endif /* CTOON_ENABLE_JSON */

    /* --- convert to mutable --- */
    mut_document mut_copy() const;   /* defined after mut_document */

    ctoon_doc *release() CTOON_NOEXCEPT { ctoon_doc *d = doc_; doc_ = NULL; return d; }

    ctoon_doc *doc_;
};

/* -----------------------------------------------------------------------
 * mut_document  –  RAII owner of ctoon_mut_doc*
 * -------------------------------------------------------------------- */

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

    static mut_document create() {
        ctoon_mut_doc *d = ctoon_mut_doc_new(NULL);
        if (!d) throw std::bad_alloc();
        return mut_document(d);
    }

    /* --- access --- */
    bool           valid()     const CTOON_NOEXCEPT { return doc_ != NULL; }
    operator       bool()      const CTOON_NOEXCEPT { return valid(); }
    mut_value      root()      const CTOON_NOEXCEPT { return mut_value(ctoon_mut_doc_get_root(doc_)); }
    mut_value      operator*() const CTOON_NOEXCEPT { return root(); }
    void           set_root(mut_value v) CTOON_NOEXCEPT { ctoon_mut_doc_set_root(doc_, v.raw()); }
    ctoon_mut_doc *raw()       const CTOON_NOEXCEPT { return doc_; }

    mut_value get_pointer(const string_view &ptr) const CTOON_NOEXCEPT {
        return mut_value(ctoon_mut_doc_ptr_getn(doc_, ptr.data(), ptr.size()));
    }

    /* --- value factories --- */
    mut_value make_null()                    CTOON_NOEXCEPT { return mut_value(ctoon_mut_null(doc_)); }
    mut_value make_true()                    CTOON_NOEXCEPT { return mut_value(ctoon_mut_true(doc_)); }
    mut_value make_false()                   CTOON_NOEXCEPT { return mut_value(ctoon_mut_false(doc_)); }
    mut_value make_bool(bool v)              CTOON_NOEXCEPT { return mut_value(ctoon_mut_bool(doc_, v)); }
    mut_value make_uint(std::uint64_t v)     CTOON_NOEXCEPT { return mut_value(ctoon_mut_uint(doc_, v)); }
    mut_value make_sint(std::int64_t  v)     CTOON_NOEXCEPT { return mut_value(ctoon_mut_sint(doc_, v)); }
    mut_value make_real(double v)            CTOON_NOEXCEPT { return mut_value(ctoon_mut_real(doc_, v)); }
    mut_value make_arr()                     CTOON_NOEXCEPT { return mut_value(ctoon_mut_arr(doc_)); }
    mut_value make_obj()                     CTOON_NOEXCEPT { return mut_value(ctoon_mut_obj(doc_)); }
    mut_value make_str(const char *s)        CTOON_NOEXCEPT { return mut_value(ctoon_mut_strcpy(doc_, s)); }
    mut_value make_str(const std::string &s) CTOON_NOEXCEPT {
        return mut_value(ctoon_mut_strncpy(doc_, s.data(), s.size()));
    }
    mut_value make_str(const string_view &s) CTOON_NOEXCEPT {
        return mut_value(ctoon_mut_strncpy(doc_, s.data(), s.size()));
    }

    /* --- TOON serialisation --- */
    write_result write(const write_options &opts = write_options()) const {
        ctoon_write_options co = opts.to_c();
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_mut_write_opts(doc_, &co, NULL, &len, &err);
        if (!raw) throw write_error(err);
        return write_result(raw, len);
    }
    void write_file(const char *path,
                    const write_options &opts = write_options()) const {
        ctoon_write_options co = opts.to_c();
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        if (!ctoon_mut_write_file(path, doc_, &co, NULL, &err)) throw write_error(err);
    }

    /* --- JSON serialisation --- */
#if defined(CTOON_ENABLE_JSON) && CTOON_ENABLE_JSON
    write_result to_json(int indent = 2,
                         write_flag flags = write_flag::NOFLAG) const {
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_mut_doc_to_json(doc_, indent, to_c(flags), NULL, &len, &err);
        if (!raw) throw write_error(err);
        return write_result(raw, len);
    }
    void to_json_file(const char *path, int indent = 2,
                      write_flag flags = write_flag::NOFLAG) const {
        ctoon_write_err err; std::memset(&err, 0, sizeof(err));
        std::size_t len = 0;
        char *raw = ctoon_mut_doc_to_json(doc_, indent, to_c(flags), NULL, &len, &err);
        if (!raw) throw write_error(err);
        FILE *fp = std::fopen(path, "wb");
        if (!fp) { std::free(raw); throw write_error(err); }
        std::fwrite(raw, 1, len, fp);
        std::fclose(fp);
        std::free(raw);
    }
#endif /* CTOON_ENABLE_JSON */

    ctoon_mut_doc *release() CTOON_NOEXCEPT { ctoon_mut_doc *d = doc_; doc_ = NULL; return d; }

    ctoon_mut_doc *doc_;
};

/* --- deferred --- */
inline mut_document document::mut_copy() const {
    ctoon_mut_doc *d = ctoon_doc_mut_copy(doc_, NULL);
    if (!d) throw std::bad_alloc();
    return mut_document(d);
}

/* -----------------------------------------------------------------------
 * Free-function API  (thin wrappers, prefer member functions)
 * -------------------------------------------------------------------- */

inline document parse(const char *toon, read_flag flags = read_flag::NOFLAG) {
    return document::parse(toon, flags);
}
inline document parse(const std::string &toon, read_flag flags = read_flag::NOFLAG) {
    return document::parse(toon, flags);
}
inline document parse_file(const char *path, read_flag flags = read_flag::NOFLAG) {
    return document::parse_file(path, flags);
}
inline mut_document make_document() { return mut_document::create(); }
inline write_result write(const document    &d, const write_options &o = write_options()) { return d.write(o); }
inline write_result write(const mut_document &d, const write_options &o = write_options()) { return d.write(o); }

#if defined(CTOON_ENABLE_JSON) && CTOON_ENABLE_JSON
inline document from_json(const char *json, std::size_t len, read_flag flags = read_flag::NOFLAG) {
    return document::from_json(json, len, flags);
}
inline document from_json(const char *json, read_flag flags = read_flag::NOFLAG) {
    return document::from_json(json, flags);
}
inline document from_json(const std::string &json, read_flag flags = read_flag::NOFLAG) {
    return document::from_json(json, flags);
}
inline document from_json_file(const char *path, read_flag flags = read_flag::NOFLAG) {
    return document::from_json_file(path, flags);
}
inline write_result to_json(const document    &d, int indent = 2, write_flag flags = write_flag::NOFLAG) {
    return d.to_json(indent, flags);
}
inline write_result to_json(const mut_document &d, int indent = 2, write_flag flags = write_flag::NOFLAG) {
    return d.to_json(indent, flags);
}
#endif /* CTOON_ENABLE_JSON */

} // namespace ctoon