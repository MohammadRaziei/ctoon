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

namespace nb = nanobind;

/* =========================================================================
 * Python <-> TOON value conversion
 * ========================================================================= */

static ctoon::mut_value py_to_mutval(ctoon::mut_document &doc, nb::handle obj) {
    if (obj.is_none()) {
        return doc.make_null();
    }
    if (nb::isinstance<nb::bool_>(obj)) {
        return doc.make_bool(nb::cast<bool>(obj));
    }
    if (nb::isinstance<nb::int_>(obj)) {
        /* try uint first to preserve large positive values */
        try {
            auto u = nb::cast<uint64_t>(obj);
            int64_t s = nb::cast<int64_t>(obj);
            return (s < 0) ? doc.make_sint(s) : doc.make_uint(u);
        } catch (...) {
            return doc.make_sint(nb::cast<int64_t>(obj));
        }
    }
    if (nb::isinstance<nb::float_>(obj)) {
        return doc.make_real(nb::cast<double>(obj));
    }
    if (nb::isinstance<nb::str>(obj)) {
        auto s = nb::cast<std::string>(obj);
        return doc.make_str(s);
    }
    if (nb::isinstance<nb::list>(obj) || nb::isinstance<nb::tuple>(obj)) {
        ctoon::mut_value arr = doc.make_arr();
        for (nb::handle item : obj) {
            arr.arr_append(py_to_mutval(doc, item));
        }
        return arr;
    }
    if (nb::isinstance<nb::dict>(obj)) {
        ctoon::mut_value o = doc.make_obj();
        nb::dict d = nb::cast<nb::dict>(obj);
        for (auto [k, v] : d) {
            if (!nb::isinstance<nb::str>(k))
                throw std::runtime_error("TOON object keys must be strings");
            auto ks = nb::cast<std::string>(k);
            o.obj_put(doc.make_str(ks), py_to_mutval(doc, v));
        }
        return o;
    }
    throw std::runtime_error(
        std::string("Unsupported Python type: ") +
        nb::cast<std::string>(nb::str(obj.type())));
}

static nb::object val_to_py(ctoon::value val) {
    if (!val.valid() || val.is_null()) return nb::none();
    if (val.is_true())  return nb::bool_(true);
    if (val.is_false()) return nb::bool_(false);
    if (val.is_uint())  return nb::cast(val.get_uint());
    if (val.is_sint())  return nb::cast(val.get_sint());
    if (val.is_real())  return nb::cast(val.get_real());
    if (val.is_str()) {
        auto sv = val.get_str();
        return nb::str(sv.data(), sv.size());
    }
    if (val.is_arr()) {
        nb::list out;
        for (auto item : val.arr_items())
            out.append(val_to_py(item));
        return out;
    }
    if (val.is_obj()) {
        nb::dict out;
        for (auto kv : val.obj_items()) {
            auto ksv = kv.key().get_str();
            nb::str key(ksv.data(), ksv.size());
            out[key] = val_to_py(kv.val());
        }
        return out;
    }
    throw std::runtime_error("Unknown ctoon value type");
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
 * Module
 * ========================================================================= */

NB_MODULE(ctoon_py, m) {
    /* Declare that this module requires the GIL.
     * In Python 3.14+ free-threaded builds this suppresses the
     * RuntimeWarning about GIL-unaware extension modules. */
#ifdef Py_GIL_DISABLED
    PyUnstable_Module_SetGIL(m.ptr(), Py_MOD_GIL_USED);
#endif

    m.doc() = "CToon – Compact TOON serialisation (C++ nanobind backend)";
    m.attr("__version__") = ctoon::version::string().data();

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
     * loads(s, *, flags=ReadFlag.NOFLAG) -> object
     *
     * Parse a TOON string or bytes object.
     * ------------------------------------------------------------------ */
    m.def("loads",
        [](nb::object s, ctoon::read_flag flags) -> nb::object {
            std::string buf;
            if (nb::isinstance<nb::bytes>(s)) {
                auto b = nb::cast<nb::bytes>(s);
                buf = std::string(b.c_str(), nb::len(b));
            } else {
                buf = nb::cast<std::string>(s);
            }
            auto doc = ctoon::document::parse(buf.data(), buf.size(), flags);
            return val_to_py(doc.root());
        },
        nb::arg("s"),
        nb::arg("flags") = ctoon::read_flag::NOFLAG,
        "Parse a TOON string or bytes object. Returns a Python object.\n\n"
        "    loads(s, flags=ReadFlag.NOFLAG) -> object");

    /* ------------------------------------------------------------------
     * load(fp, *, flags=ReadFlag.NOFLAG) -> object
     *
     * fp can be:
     *   - a str / bytes path  -> read the file from disk
     *   - a file-like object with .read() -> read from it
     * ------------------------------------------------------------------ */
    m.def("load",
        [](nb::object fp, ctoon::read_flag flags) -> nb::object {
            ctoon::document doc(nullptr);
            if (nb::isinstance<nb::str>(fp) || nb::isinstance<nb::bytes>(fp)) {
                /* path string */
                std::string path = nb::cast<std::string>(fp);
                doc = ctoon::document::parse_file(path.c_str(), flags);
            } else {
                /* file-like: call .read() */
                std::string buf = read_filelike(fp);
                doc = ctoon::document::parse(buf.data(), buf.size(), flags);
            }
            return val_to_py(doc.root());
        },
        nb::arg("fp"),
        nb::arg("flags") = ctoon::read_flag::NOFLAG,
        "Load TOON from a file path (str) or a file-like object with .read().\n\n"
        "    load(fp, flags=ReadFlag.NOFLAG) -> object\n\n"
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
           ctoon::write_flag flags) -> std::string {
            auto doc  = ctoon::mut_document::create();
            doc.set_root(py_to_mutval(doc, obj));
            auto result = doc.write(
                ctoon::write_options()
                    .with_indent(indent)
                    .with_delimiter(delim)
                    .with_flag(flags));
            return result.str();
        },
        nb::arg("obj"),
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
                write_filelike(fp, doc.write(wo));
            }
        },
        nb::arg("obj"),
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
            std::string buf;
            if (nb::isinstance<nb::bytes>(s)) {
                auto b = nb::cast<nb::bytes>(s);
                buf = std::string(b.c_str(), nb::len(b));
            } else {
                buf = nb::cast<std::string>(s);
            }
            auto doc = ctoon::document::from_json(buf.data(), buf.size(), flags);
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
        [](nb::handle obj, int indent, ctoon::write_flag flags) -> std::string {
            auto doc = ctoon::mut_document::create();
            doc.set_root(py_to_mutval(doc, obj));
            return doc.to_json(indent, flags).str();
        },
        nb::arg("obj"),
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
        nb::arg("obj"),
        nb::arg("fp"),
        nb::arg("indent") = 2,
        nb::arg("flags")  = ctoon::write_flag::NOFLAG,
        "Serialize a Python object to a JSON file path or file-like object.\n\n"
        "    dump_json(obj, fp, indent=2, flags=WriteFlag.NOFLAG)\n\n"
        "    with open('out.json', 'w') as f:\n"
        "        ctoon.dump_json(data, f)\n"
        "    ctoon.dump_json(data, 'out.json')");

    /* encode / decode – aliases matching original toon-python API
     * nanobind cannot use m.attr() as a callable in m.def(), so we
     * assign the attribute directly after the module is populated. */
    m.attr("encode") = m.attr("dumps");
    m.attr("decode") = m.attr("loads");
}