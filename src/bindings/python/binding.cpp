/**
 * @file binding.cpp
 * @brief Python bindings for CToon using nanobind.
 *
 * API design goals:
 *   - Python-friendly: mirrors json module (loads/dumps/load/dump)
 *   - ReadFlag / WriteFlag enums exposed to Python (not raw integers)
 *   - load/dump accept file-like objects (anything with .read()/.write())
 *   - JSON support (loads_json/dumps_json/load_json/dump_json)
 */

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/optional.h>
#include "ctoon.hpp"

#include <string_view>
#include <unordered_map>

namespace nb = nanobind;

/* =========================================================================
 * Python <-> TOON value conversion
 * ========================================================================= */

/* ---- Python -> TOON ------------------------------------------------------
 *
 * Raw CPython C-API on the hot path (same technique as liburlparser's binding):
 *   - dispatch on the exact type pointer first (str/int/dict/list/float/None/
 *     bool cover virtually all real data); subclasses fall back to the
 *     Py*_Check slow path, so behaviour for them is unchanged;
 *   - str  -> PyUnicode_AsUTF8AndSize + ctoon_mut_strn: NO copy. The document
 *     only borrows CPython's own UTF-8 buffer, which is valid because every
 *     caller keeps `obj` alive, holds the GIL and serialises the document
 *     before returning (no Python code runs in between, so nothing can free
 *     or mutate those strings);
 *   - dict -> PyDict_Next + ctoon_mut_obj_add: Python dict keys are unique, and
 *     ctoon_mut_obj_put would rescan every existing key per insert (O(k^2) for
 *     a k-key dict);
 *   - int  -> PyLong_AsLongLongAndOverflow: the old code probed with
 *     nb::cast<uint64_t> and relied on a C++ exception for every negative
 *     integer (~25x slower than a positive one).
 * ------------------------------------------------------------------------ */

static ctoon::mut_value py_to_mutval(ctoon::mut_document &doc, nb::handle obj);

static inline ctoon::mut_value py_str_to_mut(ctoon::mut_document &doc, PyObject *o) {
    Py_ssize_t n = 0;
    const char *p = PyUnicode_AsUTF8AndSize(o, &n);
    if (!p) throw nb::python_error();
    return ctoon::mut_value(ctoon_mut_strn(doc.raw(), p, (size_t)n));
}

static ctoon::mut_value py_int_to_mut(ctoon::mut_document &doc, PyObject *o) {
    /* Non-negative -> uint, negative -> sint, so values up to 2^64-1 keep
     * their exact digits; only magnitudes beyond uint64/int64 fall back to a
     * double (spec §2: still a valid number, just a real). */
    int overflow = 0;
    long long v = PyLong_AsLongLongAndOverflow(o, &overflow);
    if (v == -1 && PyErr_Occurred()) throw nb::python_error();
    if (overflow == 0)
        return v < 0 ? doc.make_sint((std::int64_t)v)
                     : doc.make_uint((std::uint64_t)v);
    if (overflow > 0) {
        unsigned long long u = PyLong_AsUnsignedLongLong(o);
        if (!(u == (unsigned long long)-1 && PyErr_Occurred()))
            return doc.make_uint((std::uint64_t)u);
        PyErr_Clear();
    }
    double d = PyLong_AsDouble(o);
    if (d == -1.0 && PyErr_Occurred()) throw nb::python_error();
    return doc.make_real(d);
}

static inline ctoon::mut_value py_float_to_mut(ctoon::mut_document &doc, PyObject *o) {
    double d = PyFloat_AsDouble(o);          /* limited-API: no PyFloat_AS_DOUBLE */
    if (d == -1.0 && PyErr_Occurred()) throw nb::python_error();
    return doc.make_real(d);
}

static ctoon::mut_value py_dict_to_mut(ctoon::mut_document &doc, PyObject *d) {
    ctoon::mut_value o = doc.make_obj();
    Py_ssize_t pos = 0;
    PyObject *k, *v;
    while (PyDict_Next(d, &pos, &k, &v)) {
        if (!PyUnicode_Check(k))
            throw std::runtime_error("TOON object keys must be strings");
        ctoon::mut_value key = py_str_to_mut(doc, k);
        ctoon::mut_value val = py_to_mutval(doc, nb::handle(v));
        ctoon_mut_obj_add(o.raw(), key.raw(), val.raw());
    }
    return o;
}

static ctoon::mut_value py_to_mutval(ctoon::mut_document &doc, nb::handle h) {
    PyObject *o = h.ptr();
    if (o == Py_None)  return doc.make_null();
    if (o == Py_True)  return doc.make_bool(true);
    if (o == Py_False) return doc.make_bool(false);

    PyTypeObject *t = Py_TYPE(o);
    if (t == &PyUnicode_Type) return py_str_to_mut(doc, o);
    if (t == &PyDict_Type)    return py_dict_to_mut(doc, o);
    if (t == &PyLong_Type)    return py_int_to_mut(doc, o);
    if (t == &PyFloat_Type)   return py_float_to_mut(doc, o);
    if (t == &PyList_Type) {
        ctoon::mut_value arr = doc.make_arr();
        const Py_ssize_t n = PyList_Size(o);   /* limited-API: no *_GET_SIZE/_ITEM */
        for (Py_ssize_t i = 0; i < n; i++)
            ctoon_mut_arr_append(arr.raw(),
                py_to_mutval(doc, nb::handle(PyList_GetItem(o, i))).raw());
        return arr;
    }

    /* Slow path: subclasses (IntEnum, OrderedDict, str subclasses, ...) and
     * tuples, exactly as before. bool is only ever Py_True/Py_False above. */
    if (PyLong_Check(o))    return py_int_to_mut(doc, o);
    if (PyFloat_Check(o))   return py_float_to_mut(doc, o);
    if (PyUnicode_Check(o)) return py_str_to_mut(doc, o);
    if (PyDict_Check(o))    return py_dict_to_mut(doc, o);
    if (PyList_Check(o) || PyTuple_Check(o)) {
        ctoon::mut_value arr = doc.make_arr();
        for (nb::handle item : h)
            ctoon_mut_arr_append(arr.raw(), py_to_mutval(doc, item).raw());
        return arr;
    }
    throw std::runtime_error(
        std::string("Unsupported Python type: ") +
        nb::cast<std::string>(nb::str(h.type())));
}

/* ---- TOON -> Python ------------------------------------------------------
 *
 * Raw C-API again, plus a per-call cache of dict-key str objects: a decoded
 * array of 1000 rows has the same few keys 1000 times, and building a fresh
 * str (allocation + UTF-8 decode + hash) for each is pure waste. The cache
 * hands back the same object, whose hash is already computed. It is bounded
 * and switches itself off when keys turn out not to repeat (a dict whose keys
 * are all different would only pay for the lookups).
 * ------------------------------------------------------------------------ */

struct PyRef {                     /* owns one reference; released on unwind */
    PyObject *p;
    explicit PyRef(PyObject *x) : p(x) {}
    PyRef(const PyRef &) = delete;
    PyRef &operator=(const PyRef &) = delete;
    ~PyRef() { Py_XDECREF(p); }
    PyObject *release() { PyObject *t = p; p = nullptr; return t; }
};

class KeyCache {
public:
    KeyCache() { map_.reserve(64); }
    ~KeyCache() { for (auto &e : map_) Py_DECREF(e.second); }
    KeyCache(const KeyCache &) = delete;
    KeyCache &operator=(const KeyCache &) = delete;

    /* Returns a NEW reference to the str for these UTF-8 bytes. */
    PyObject *get(const char *p, std::size_t n) {
        if (enabled_ && n <= kMaxKeyLen) {
            std::string_view sv(p, n);
            auto it = map_.find(sv);
            ++lookups_;
            if (it != map_.end()) {
                ++hits_;
                Py_INCREF(it->second);
                return it->second;
            }
            if (lookups_ >= kProbe && hits_ * 4 < lookups_) {
                enabled_ = false;                   /* keys don't repeat */
            } else if (map_.size() < kMaxEntries) {
                PyObject *k = PyUnicode_FromStringAndSize(p, (Py_ssize_t)n);
                if (!k) throw nb::python_error();
                /* the key view must outlive the TOON document it points into:
                 * it does, the document is alive for the whole conversion */
                map_.emplace(sv, k);
                Py_INCREF(k);                       /* one for the cache */
                return k;
            }
        }
        PyObject *k = PyUnicode_FromStringAndSize(p, (Py_ssize_t)n);
        if (!k) throw nb::python_error();
        return k;
    }

private:
    static constexpr std::size_t kMaxKeyLen  = 64;
    static constexpr std::size_t kMaxEntries = 1024;
    static constexpr std::size_t kProbe      = 512;
    std::unordered_map<std::string_view, PyObject *> map_;
    std::size_t lookups_ = 0, hits_ = 0;
    bool enabled_ = true;
};

static PyObject *val_to_py_raw(ctoon::value val, KeyCache &keys) {
    if (!val.valid() || val.is_null()) { Py_RETURN_NONE; }
    if (val.is_true())  { Py_RETURN_TRUE;  }
    if (val.is_false()) { Py_RETURN_FALSE; }

    PyObject *r = nullptr;
    if (val.is_uint())      r = PyLong_FromUnsignedLongLong(val.get_uint());
    else if (val.is_sint()) r = PyLong_FromLongLong(val.get_sint());
    else if (val.is_real()) r = PyFloat_FromDouble(val.get_real());
    else if (val.is_str()) {
        auto sv = val.get_str();
        r = PyUnicode_FromStringAndSize(sv.data(), (Py_ssize_t)sv.size());
    }
    else if (val.is_arr()) {
        PyRef list(PyList_New((Py_ssize_t)val.arr_size()));
        if (!list.p) throw nb::python_error();
        Py_ssize_t i = 0;
        for (auto item : val.arr_items()) {
            PyObject *child = val_to_py_raw(item, keys);
            if (PyList_SetItem(list.p, i++, child) < 0)   /* steals child, even on error */
                throw nb::python_error();
        }
        return list.release();
    }
    else if (val.is_obj()) {
        PyRef dict(PyDict_New());
        if (!dict.p) throw nb::python_error();
        for (auto kv : val.obj_items()) {
            auto ksv = kv.key().get_str();
            PyRef k(keys.get(ksv.data(), ksv.size()));
            PyRef v(val_to_py_raw(kv.val(), keys));
            if (PyDict_SetItem(dict.p, k.p, v.p) < 0) throw nb::python_error();
        }
        return dict.release();
    }
    else throw std::runtime_error("Unknown ctoon value type");

    if (!r) throw nb::python_error();
    return r;
}

static nb::object val_to_py(ctoon::value val) {
    KeyCache keys;
    return nb::steal(val_to_py_raw(val, keys));
}

/* =========================================================================
 * Helpers: read bytes from a file-like object
 * ========================================================================= */

/**
 * Read all bytes from a Python file-like object (must have .read()).
 * Returns the content as std::string.
 */
static std::string read_filelike(nb::handle fp) {
    nb::object data = fp.attr("read")();
    if (nb::isinstance<nb::bytes>(data)) {
        auto b = nb::cast<nb::bytes>(data);
        return std::string(b.c_str(), nb::len(b));
    }
    /* text mode – encode to UTF-8 */
    return nb::cast<std::string>(data);
}

/**
 * Write string content to a Python file-like object (must have .write()).
 */
static void write_filelike(nb::handle fp, const ctoon::write_result &result) {
    /* detect if the file was opened in binary mode */
    bool binary = false;
    try {
        nb::object mode = fp.attr("mode");
        std::string m = nb::cast<std::string>(mode);
        binary = (m.find('b') != std::string::npos);
    } catch (...) {
        /* no .mode attribute – try writing str, fall back to bytes */
    }
    if (binary) {
        fp.attr("write")(nb::bytes(result.c_str(), result.size()));
    } else {
        fp.attr("write")(nb::str(result.c_str(), result.size()));
    }
}



/* =========================================================================
 * Direct JSON <-> TOON text conversion helpers
 *
 * These call straight into the C core. No Python dict/list is built, and the
 * GIL is released while the conversion runs. str inputs are read in place
 * (CPython's cached UTF-8 buffer); bytes inputs are read in place too.
 * ========================================================================= */

struct text_view {
    const char *data;
    size_t      size;
};

static text_view text_of(nb::handle h) {
    if (PyUnicode_Check(h.ptr())) {
        Py_ssize_t n = 0;
        const char *p = PyUnicode_AsUTF8AndSize(h.ptr(), &n);
        if (!p) throw nb::python_error();
        return {p, (size_t)n};
    }
    if (PyBytes_Check(h.ptr())) {
        char *p = nullptr; Py_ssize_t n = 0;
        if (PyBytes_AsStringAndSize(h.ptr(), &p, &n) < 0) throw nb::python_error();
        return {p, (size_t)n};
    }
    throw nb::type_error("expected str or bytes");
}

[[noreturn]] static void throw_convert_error(const ctoon_read_err &r,
                                             const ctoon_write_err &w) {
    if (r.code) {
        char buf[32];
        std::snprintf(buf, sizeof buf, " (pos %zu)", (size_t)r.pos);
        throw std::runtime_error(std::string(r.msg ? r.msg : "parse error") + buf);
    }
    throw std::runtime_error(w.msg ? w.msg : "write error");
}

static nb::str take_str(char *raw, size_t n) {
    nb::str out(raw, n);
    std::free(raw);
    return out;
}

/* A parse never runs in place inside a Python str/bytes: INSITU would make the
 * parser write into the object's (immutable, shared) buffer. */
static inline ctoon::read_flag strip_insitu(ctoon::read_flag f) {
    return static_cast<ctoon::read_flag>(
        static_cast<std::uint32_t>(f) &
        ~static_cast<std::uint32_t>(ctoon::read_flag::INSITU));
}

/* Result text -> Python str straight from the C buffer (no std::string hop). */
static nb::object str_from_result(const ctoon::write_result &r) {
    PyObject *o = PyUnicode_FromStringAndSize(r.c_str(), (Py_ssize_t)r.size());
    if (!o) throw nb::python_error();
    return nb::steal(o);
}

/* Parsing is pure C work on an immutable, referenced buffer, so large inputs
 * release the GIL; small ones skip the (comparatively costly) release. */
template <class F>
static inline void parse_maybe_nogil(std::size_t n, F &&f) {
    if (n >= 4096) { nb::gil_scoped_release nogil; f(); }
    else f();
}

/* =========================================================================
 * Module
 * ========================================================================= */

NB_MODULE(ctoon_py, m) {
    m.doc() = "CToon – Compact TOON serialisation (C++ nanobind backend)";
    m.attr("__version__") = ctoon::version::string().data();
    // TOON spec this build targets (spec §13), from the C core.
    {
        const uint32_t v = ctoon_spec_tag_hex();
        const std::string tag = "v" + std::to_string((v >> 16) & 0xFFu) + "." +
                                std::to_string((v >> 8) & 0xFFu) + "." +
                                std::to_string(v & 0xFFu);
        m.attr("__toon_spec__")      = nb::str(ctoon_spec_version_string());  // "4.1"
        m.attr("__toon_spec_tag__")  = nb::str(tag.c_str());                  // "v4.1.2"
        m.attr("__toon_spec_date__") = nb::str(ctoon_spec_date_string());     // "2026-07-26"
    }

    /* ------------------------------------------------------------------
     * ReadFlag enum
     * ------------------------------------------------------------------ */
    nb::enum_<ctoon::read_flag>(m, "ReadFlag", nb::is_flag())
        .value("NOFLAG",                  ctoon::read_flag::NOFLAG)
        .value("INSITU",                  ctoon::read_flag::INSITU)
        .value("STOP_WHEN_DONE",          ctoon::read_flag::STOP_WHEN_DONE)
        .value("ALLOW_TRAILING_COMMAS",   ctoon::read_flag::ALLOW_TRAILING_COMMAS)
        .value("ALLOW_COMMENTS",          ctoon::read_flag::ALLOW_COMMENTS)
        .value("ALLOW_INF_AND_NAN",       ctoon::read_flag::ALLOW_INF_AND_NAN)
        .value("NUMBER_AS_RAW",           ctoon::read_flag::NUMBER_AS_RAW)
        .value("ALLOW_INVALID_UNICODE",   ctoon::read_flag::ALLOW_INVALID_UNICODE)
        .value("BIGNUM_AS_RAW",           ctoon::read_flag::BIGNUM_AS_RAW)
        .value("ALLOW_BOM",               ctoon::read_flag::ALLOW_BOM)
        .value("ALLOW_EXT_NUMBER",        ctoon::read_flag::ALLOW_EXT_NUMBER)
        .value("ALLOW_EXT_ESCAPE",        ctoon::read_flag::ALLOW_EXT_ESCAPE)
        .value("ALLOW_EXT_WHITESPACE",    ctoon::read_flag::ALLOW_EXT_WHITESPACE)
        .value("ALLOW_SINGLE_QUOTED_STR", ctoon::read_flag::ALLOW_SINGLE_QUOTED_STR)
        .value("ALLOW_UNQUOTED_KEY",      ctoon::read_flag::ALLOW_UNQUOTED_KEY)
        .value("NON_STRICT",              ctoon::read_flag::NON_STRICT)
        /* __or__: combine two flags and return a new ReadFlag instance
         * using nb::type<T>() to construct without validation. */
        .def("__or__",  [](nb::object self, nb::object other) -> nb::object {
            uint32_t v = nb::cast<uint32_t>(self.attr("value"))
                       | nb::cast<uint32_t>(other.attr("value"));
            return self.type()(v);
        })
        .def("__ror__", [](nb::object self, nb::object other) -> nb::object {
            uint32_t v = nb::cast<uint32_t>(self.attr("value"))
                       | nb::cast<uint32_t>(other.attr("value"));
            return self.type()(v);
        })
        .def("__int__", [](ctoon::read_flag a) {
            return static_cast<uint32_t>(a); })
        .export_values();

    /* ------------------------------------------------------------------
     * WriteFlag enum
     * ------------------------------------------------------------------ */
    nb::enum_<ctoon::write_flag>(m, "WriteFlag", nb::is_flag())
        .value("NOFLAG",                ctoon::write_flag::NOFLAG)
        .value("ESCAPE_UNICODE",        ctoon::write_flag::ESCAPE_UNICODE)
        .value("ESCAPE_SLASHES",        ctoon::write_flag::ESCAPE_SLASHES)
        .value("ALLOW_INF_AND_NAN",     ctoon::write_flag::ALLOW_INF_AND_NAN)
        .value("INF_AND_NAN_AS_NULL",   ctoon::write_flag::INF_AND_NAN_AS_NULL)
        .value("ALLOW_INVALID_UNICODE", ctoon::write_flag::ALLOW_INVALID_UNICODE)
        .value("LENGTH_MARKER",         ctoon::write_flag::LENGTH_MARKER)
        .value("NEWLINE_AT_END",        ctoon::write_flag::NEWLINE_AT_END)
        .def("__or__",  [](nb::object self, nb::object other) -> nb::object {
            uint32_t v = nb::cast<uint32_t>(self.attr("value"))
                       | nb::cast<uint32_t>(other.attr("value"));
            return self.type()(v);
        })
        .def("__ror__", [](nb::object self, nb::object other) -> nb::object {
            uint32_t v = nb::cast<uint32_t>(self.attr("value"))
                       | nb::cast<uint32_t>(other.attr("value"));
            return self.type()(v);
        })
        .def("__int__", [](ctoon::write_flag a) {
            return static_cast<uint32_t>(a); })
        .export_values();

    /* ------------------------------------------------------------------
     * Delimiter enum
     * ------------------------------------------------------------------ */
    nb::enum_<ctoon::delimiter>(m, "Delimiter")
        .value("COMMA", ctoon::delimiter::COMMA)
        .value("TAB",   ctoon::delimiter::TAB)
        .value("PIPE",  ctoon::delimiter::PIPE)
        .export_values();

    /* ------------------------------------------------------------------
     * loads(s, *, flags=ReadFlag.NOFLAG, indent=2, strict=True) -> object
     *
     * Parse a TOON string or bytes object.
     * strict=False ORs in ReadFlag.NON_STRICT (spec §13 `strict: false`).
     * ------------------------------------------------------------------ */
    m.def("loads",
        [](nb::object s, ctoon::read_flag flags, int indent, bool strict) -> nb::object {
            if (!strict) flags |= ctoon::read_flag::NON_STRICT;
            flags = strip_insitu(flags);
            text_view tv = text_of(s);            /* in place: no copy of the input */
            ctoon::document doc(nullptr);
            parse_maybe_nogil(tv.size, [&] {
                doc = (indent == 2)
                    ? ctoon::document::parse(tv.data, tv.size, flags)
                    : ctoon::document::parse_indent(tv.data, tv.size, indent, flags);
            });
            return val_to_py(doc.root());
        },
        nb::arg("s"),
        nb::arg("flags") = ctoon::read_flag::NOFLAG,
        nb::arg("indent") = 2,
        nb::arg("strict") = true,
        "Parse a TOON string or bytes object. Returns a Python object.\n\n"
        "    loads(s, flags=ReadFlag.NOFLAG, indent=2, strict=True) -> object\n\n"
        "indent overrides indentSize (spec §12) - the number of spaces\n"
        "that count as one indentation level when decoding (default 2).\n"
        "strict=False decodes in non-strict mode (spec §13 `strict: false`),\n"
        "equivalent to passing ReadFlag.NON_STRICT.");

    /* ------------------------------------------------------------------
     * load(fp, *, flags=ReadFlag.NOFLAG, indent=2, strict=True) -> object
     *
     * fp can be:
     *   - a str / bytes path  -> read the file from disk
     *   - a file-like object with .read() -> read from it
     * ------------------------------------------------------------------ */
    m.def("load",
        [](nb::object fp, ctoon::read_flag flags, int indent, bool strict) -> nb::object {
            if (!strict) flags |= ctoon::read_flag::NON_STRICT;
            ctoon::document doc(nullptr);
            if (nb::isinstance<nb::str>(fp) || nb::isinstance<nb::bytes>(fp)) {
                /* path string */
                std::string path = nb::cast<std::string>(fp);
                doc = ctoon::document::parse_file(path.c_str(), flags);
            } else {
                /* file-like: call .read() */
                std::string buf = read_filelike(fp);
                doc = (indent == 2)
                    ? ctoon::document::parse(buf.data(), buf.size(), flags)
                    : ctoon::document::parse_indent(buf.data(), buf.size(), indent, flags);
            }
            return val_to_py(doc.root());
        },
        nb::arg("fp"),
        nb::arg("flags") = ctoon::read_flag::NOFLAG,
        nb::arg("indent") = 2,
        nb::arg("strict") = true,
        "Load TOON from a file path (str) or a file-like object with .read().\n\n"
        "    load(fp, flags=ReadFlag.NOFLAG, indent=2, strict=True) -> object\n\n"
        "strict=False decodes in non-strict mode (spec §13 `strict: false`).\n\n"
        "    with open('data.toon') as f:\n"
        "        obj = ctoon.load(f)\n"
        "    obj = ctoon.load('data.toon')");

    /* ------------------------------------------------------------------
     * dumps(obj, *, indent=2, delimiter=Delimiter.COMMA,
     *        flags=WriteFlag.NOFLAG) -> str
     * ------------------------------------------------------------------ */
    m.def("dumps",
        [](nb::handle obj,
           int indent,
           ctoon::delimiter delim,
           ctoon::write_flag flags) -> nb::object {
            auto doc  = ctoon::mut_document::create();
            doc.set_root(py_to_mutval(doc, obj));
            auto result = doc.to_string(
                ctoon::write_options()
                    .with_indent(indent)
                    .with_delimiter(delim)
                    .with_flag(flags));
            return str_from_result(result);
        },
        nb::arg("obj").none(),
        nb::arg("indent")    = 2,
        nb::arg("delimiter") = ctoon::delimiter::COMMA,
        nb::arg("flags")     = ctoon::write_flag::NOFLAG,
        "Serialize a Python object to a TOON string.\n\n"
        "    dumps(obj, indent=2, delimiter=Delimiter.COMMA,\n"
        "          flags=WriteFlag.NOFLAG) -> str");

    /* ------------------------------------------------------------------
     * dump(obj, fp, *, indent=2, delimiter=Delimiter.COMMA,
     *       flags=WriteFlag.NOFLAG)
     *
     * fp can be:
     *   - a str / bytes path  -> write the file to disk
     *   - a file-like object with .write() -> write to it
     * ------------------------------------------------------------------ */
    m.def("dump",
        [](nb::handle obj,
           nb::object fp,
           int indent,
           ctoon::delimiter delim,
           ctoon::write_flag flags) {
            auto doc  = ctoon::mut_document::create();
            doc.set_root(py_to_mutval(doc, obj));
            auto wo   = ctoon::write_options()
                            .with_indent(indent)
                            .with_delimiter(delim)
                            .with_flag(flags);

            if (nb::isinstance<nb::str>(fp) || nb::isinstance<nb::bytes>(fp)) {
                std::string path = nb::cast<std::string>(fp);
                doc.write_file(path.c_str(), wo);
            } else {
                write_filelike(fp, doc.to_string(wo));
            }
        },
        nb::arg("obj").none(),
        nb::arg("fp"),
        nb::arg("indent")    = 2,
        nb::arg("delimiter") = ctoon::delimiter::COMMA,
        nb::arg("flags")     = ctoon::write_flag::NOFLAG,
        "Serialize a Python object to a TOON file path or file-like object.\n\n"
        "    dump(obj, fp, indent=2, delimiter=Delimiter.COMMA,\n"
        "         flags=WriteFlag.NOFLAG)\n\n"
        "    with open('out.toon', 'w') as f:\n"
        "        ctoon.dump(data, f)\n"
        "    ctoon.dump(data, 'out.toon')");

    /* ------------------------------------------------------------------
     * JSON support
     * ------------------------------------------------------------------ */

    /* loads_json(s, *, flags=ReadFlag.NOFLAG) -> object */
    m.def("loads_json",
        [](nb::object s, ctoon::read_flag flags) -> nb::object {
            flags = strip_insitu(flags);
            text_view tv = text_of(s);
            ctoon::document doc(nullptr);
            parse_maybe_nogil(tv.size, [&] {
                doc = ctoon::document::from_json(tv.data, tv.size, flags);
            });
            return val_to_py(doc.root());
        },
        nb::arg("s"),
        nb::arg("flags") = ctoon::read_flag::NOFLAG,
        "Parse a JSON string or bytes object.\n\n"
        "    loads_json(s, flags=ReadFlag.NOFLAG) -> object");

    /* load_json(fp, *, flags=ReadFlag.NOFLAG) -> object */
    m.def("load_json",
        [](nb::object fp, ctoon::read_flag flags) -> nb::object {
            ctoon::document doc(nullptr);
            if (nb::isinstance<nb::str>(fp) || nb::isinstance<nb::bytes>(fp)) {
                std::string path = nb::cast<std::string>(fp);
                doc = ctoon::document::from_json_file(path.c_str(), flags);
            } else {
                std::string buf = read_filelike(fp);
                doc = ctoon::document::from_json(buf.data(), buf.size(), flags);
            }
            return val_to_py(doc.root());
        },
        nb::arg("fp"),
        nb::arg("flags") = ctoon::read_flag::NOFLAG,
        "Load JSON from a file path (str) or a file-like object with .read().\n\n"
        "    load_json(fp, flags=ReadFlag.NOFLAG) -> object\n\n"
        "    with open('data.json') as f:\n"
        "        obj = ctoon.load_json(f)");

    /* dumps_json(obj, *, indent=2, flags=WriteFlag.NOFLAG) -> str */
    m.def("dumps_json",
        [](nb::handle obj, int indent, ctoon::write_flag flags) -> nb::object {
            auto doc = ctoon::mut_document::create();
            doc.set_root(py_to_mutval(doc, obj));
            return str_from_result(doc.to_json(indent, flags));
        },
        nb::arg("obj").none(),
        nb::arg("indent") = 2,
        nb::arg("flags")  = ctoon::write_flag::NOFLAG,
        "Serialize a Python object to a JSON string.\n\n"
        "    dumps_json(obj, indent=2, flags=WriteFlag.NOFLAG) -> str");

    /* dump_json(obj, fp, *, indent=2, flags=WriteFlag.NOFLAG) */
    m.def("dump_json",
        [](nb::handle obj,
           nb::object fp,
           int indent,
           ctoon::write_flag flags) {
            auto doc = ctoon::mut_document::create();
            doc.set_root(py_to_mutval(doc, obj));
            if (nb::isinstance<nb::str>(fp) || nb::isinstance<nb::bytes>(fp)) {
                std::string path = nb::cast<std::string>(fp);
                doc.to_json_file(path.c_str(), indent, flags);
            } else {
                write_filelike(fp, doc.to_json(indent, flags));
            }
        },
        nb::arg("obj").none(),
        nb::arg("fp"),
        nb::arg("indent") = 2,
        nb::arg("flags")  = ctoon::write_flag::NOFLAG,
        "Serialize a Python object to a JSON file path or file-like object.\n\n"
        "    dump_json(obj, fp, indent=2, flags=WriteFlag.NOFLAG)\n\n"
        "    with open('out.json', 'w') as f:\n"
        "        ctoon.dump_json(data, f)\n"
        "    ctoon.dump_json(data, 'out.json')");


    /* ------------------------------------------------------------------
     * Direct text <-> text conversion (no Python intermediate object)
     * ------------------------------------------------------------------ */

    m.def("json_to_toon",
        [](nb::object json, int indent, ctoon::delimiter delim,
           ctoon::write_flag wflags, ctoon::read_flag rflags) -> nb::str {
            text_view in = text_of(json);
            ctoon_write_options wo;
            wo.flag = ctoon::to_c(wflags);
            wo.delimiter = static_cast<ctoon_delimiter>(delim);
            wo.indent = indent;
            ctoon_read_err rerr{};  ctoon_write_err werr{};
            size_t n = 0; char *raw;
            {
                nb::gil_scoped_release nogil;
                raw = ctoon_json_to_toon_opts(in.data, in.size,
                        ctoon::to_c(rflags), &wo, nullptr, &n, &rerr, &werr);
            }
            if (!raw) throw_convert_error(rerr, werr);
            return take_str(raw, n);
        },
        nb::arg("json"),
        nb::arg("indent")    = 2,
        nb::arg("delimiter") = ctoon::delimiter::COMMA,
        nb::arg("flags")     = ctoon::write_flag::NOFLAG,
        nb::arg("read_flags") = ctoon::read_flag::NOFLAG,
        "Convert JSON text (str or bytes) to a TOON string, directly.\n\n"
        "    json_to_toon(json, indent=2, delimiter=Delimiter.COMMA,\n"
        "                 flags=WriteFlag.NOFLAG, read_flags=ReadFlag.NOFLAG) -> str");

    m.def("toon_to_json",
        [](nb::object toon, int indent, ctoon::write_flag wflags,
           ctoon::read_flag rflags) -> nb::str {
            text_view in = text_of(toon);
            ctoon_read_err rerr{};  ctoon_write_err werr{};
            size_t n = 0; char *raw;
            {
                nb::gil_scoped_release nogil;
                raw = ctoon_toon_to_json_opts(in.data, in.size,
                        ctoon::to_c(rflags), indent, ctoon::to_c(wflags),
                        nullptr, &n, &rerr, &werr);
            }
            if (!raw) throw_convert_error(rerr, werr);
            return take_str(raw, n);
        },
        nb::arg("toon"),
        nb::arg("indent")     = 0,
        nb::arg("flags")      = ctoon::write_flag::NOFLAG,
        nb::arg("read_flags") = ctoon::read_flag::NOFLAG,
        "Convert TOON text (str or bytes) to a JSON string, directly.\n\n"
        "    toon_to_json(toon, indent=0, flags=WriteFlag.NOFLAG,\n"
        "                 read_flags=ReadFlag.NOFLAG) -> str");

    m.def("json_to_toon_file",
        [](const std::string &json_path, const std::string &toon_path,
           int indent, ctoon::delimiter delim, ctoon::write_flag wflags,
           ctoon::read_flag rflags) {
            ctoon_write_options wo;
            wo.flag = ctoon::to_c(wflags);
            wo.delimiter = static_cast<ctoon_delimiter>(delim);
            wo.indent = indent;
            ctoon_read_err rerr{};  ctoon_write_err werr{};
            bool ok;
            {
                nb::gil_scoped_release nogil;
                ok = ctoon_json_to_toon_file_opts(json_path.c_str(), toon_path.c_str(),
                        ctoon::to_c(rflags), &wo, nullptr, &rerr, &werr);
            }
            if (!ok) throw_convert_error(rerr, werr);
        },
        nb::arg("json_path"), nb::arg("toon_path"),
        nb::arg("indent")    = 2,
        nb::arg("delimiter") = ctoon::delimiter::COMMA,
        nb::arg("flags")     = ctoon::write_flag::NOFLAG,
        nb::arg("read_flags") = ctoon::read_flag::NOFLAG,
        "Convert a JSON file to a TOON file, directly.");

    m.def("toon_to_json_file",
        [](const std::string &toon_path, const std::string &json_path,
           int indent, ctoon::write_flag wflags, ctoon::read_flag rflags) {
            ctoon_read_err rerr{};  ctoon_write_err werr{};
            bool ok;
            {
                nb::gil_scoped_release nogil;
                ok = ctoon_toon_to_json_file_opts(toon_path.c_str(), json_path.c_str(),
                        ctoon::to_c(rflags), indent, ctoon::to_c(wflags),
                        nullptr, &rerr, &werr);
            }
            if (!ok) throw_convert_error(rerr, werr);
        },
        nb::arg("toon_path"), nb::arg("json_path"),
        nb::arg("indent")     = 0,
        nb::arg("flags")      = ctoon::write_flag::NOFLAG,
        nb::arg("read_flags") = ctoon::read_flag::NOFLAG,
        "Convert a TOON file to a JSON file, directly.");

    /* encode / decode – aliases matching original toon-python API
     * nanobind cannot use m.attr() as a callable in m.def(), so we
     * assign the attribute directly after the module is populated. */
    m.attr("encode") = m.attr("dumps");
    m.attr("decode") = m.attr("loads");
}