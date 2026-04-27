/**
 * @file binding.cpp
 * @brief Python bindings for CToon using nanobind.
 *
 * API similar to toon-python: encode(dict) -> str, decode(str) -> dict
 * Uses ctoon.hpp C++11 API with nanobind.
 */

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/variant.h>
#include "ctoon.hpp"

namespace nb = nanobind;

/* -----------------------------------------------------------------------
 * Python <-> TOON conversion helpers
 * ----------------------------------------------------------------------- */

/**
 * Convert a Python object to a mut_value in the given document.
 * Supports: None, bool, int, float, str, list, dict
 */
static ctoon::mut_value py_to_mutval(ctoon::mut_document& doc, nb::handle obj) {
    if (obj.is_none()) {
        return doc.make_null();
    } else if (nb::isinstance<nb::bool_>(obj)) {
        return doc.make_bool(nb::cast<bool>(obj));
    } else if (nb::isinstance<nb::int_>(obj)) {
        int64_t v = nb::cast<int64_t>(obj);
        if (v < 0) return doc.make_sint(v);
        return doc.make_uint(static_cast<uint64_t>(v));
    } else if (nb::isinstance<nb::float_>(obj)) {
        return doc.make_real(nb::cast<double>(obj));
    } else if (nb::isinstance<nb::str>(obj)) {
        return doc.make_str(nb::cast<std::string>(obj));
    } else if (nb::isinstance<nb::list>(obj) || nb::isinstance<nb::tuple>(obj)) {
        ctoon::mut_value arr = doc.make_arr();
        nb::iterator it(nb::iter(obj));
        nb::iterator end;
        for (; it != end; ++it) {
            arr.arr_append(py_to_mutval(doc, *it));
        }
        return arr;
    } else if (nb::isinstance<nb::dict>(obj)) {
        ctoon::mut_value obj2 = doc.make_obj();
        nb::dict d = nb::cast<nb::dict>(obj);
        for (nb::handle key : d.keys()) {
            std::string k = nb::cast<std::string>(key);
            ctoon::mut_value v = py_to_mutval(doc, d[key]);
            obj2.obj_put(doc.make_str(k), v);
        }
        return obj2;
    }
    throw std::runtime_error("Unsupported Python type for TOON conversion");
}

/**
 * Convert a mut_value to a Python object.
 */
static nb::object mutval_to_py(ctoon::mut_value val) {
    if (!val.valid()) return nb::none();
    else if (val.is_null()) return nb::none();
    else if (val.is_true()) return nb::bool_(true);
    else if (val.is_false()) return nb::bool_(false);
    else if (val.is_uint()) return nb::cast(nb::cast<std::uint64_t>(val.get_uint()));
    else if (val.is_sint()) return nb::cast(nb::cast<std::int64_t>(val.get_sint()));
    else if (val.is_real()) return nb::cast(nb::cast<double>(val.get_real()));
    else if (val.is_str()) {
        return nb::str(val.get_str().data(), val.get_str().size());
    } else if (val.is_arr()) {
        nb::list out = nb::list();
        for (std::size_t i = 0; i < val.arr_size(); ++i) {
            out.append(mutval_to_py(val.arr_get(i)));
        }
        return out;
    } else if (val.is_obj()) {
        nb::dict out = nb::dict();
        for (auto kv : val.obj_items()) {
            auto sv = kv.key().get_str();
            std::string k(sv.data(), sv.size());
            out[nb::cast(k)] = mutval_to_py(kv.val());
        }
        return out;
    }
    throw std::runtime_error("Unknown mut_value type");
}

/**
 * Convert a value (immutable) to a Python object.
 */
static nb::object val_to_py(ctoon::value val) {
    if (!val.valid()) return nb::none();
    else if (val.is_null()) return nb::none();
    else if (val.is_true()) return nb::bool_(true);
    else if (val.is_false()) return nb::bool_(false);
    else if (val.is_uint()) return nb::cast(nb::cast<std::uint64_t>(val.get_uint()));
    else if (val.is_sint()) return nb::cast(nb::cast<std::int64_t>(val.get_sint()));
    else if (val.is_real()) return nb::cast(nb::cast<double>(val.get_real()));
    else if (val.is_str()) {
        return nb::str(val.get_str().data(), val.get_str().size());
    } else if (val.is_arr()) {
        nb::list out = nb::list();
        for (std::size_t i = 0; i < val.arr_size(); ++i) {
            out.append(val_to_py(val.arr_get(i)));
        }
        return out;
    } else if (val.is_obj()) {
        nb::dict out = nb::dict();
        for (auto kv : val.obj_items()) {
            auto sv = kv.key().get_str();
            std::string k(sv.data(), sv.size());
            out[nb::cast(k)] = val_to_py(kv.val());
        }
        return out;
    }
    throw std::runtime_error("Unknown value type");
}

/* -----------------------------------------------------------------------
 * Enum: Delimiter
 * -------------------------------------------------------------------- */
void bind_enums(nb::module_& m) {
    nb::enum_<ctoon_delimiter>(m, "Delimiter")
        .value("COMMA", CTOON_DELIMITER_COMMA)
        .value("TAB", CTOON_DELIMITER_TAB)
        .value("PIPE", CTOON_DELIMITER_PIPE)
        .export_values();
}

/* -----------------------------------------------------------------------
 * Options - unified options for both encode and decode
 * -------------------------------------------------------------------- */
struct Options {
    /* Read options (used by decode/load) */
    ctoon_read_flag read_flag = CTOON_READ_NOFLAG;

    /* Write options (used by encode/dump) */
    int indent = 2;
    ctoon_delimiter delimiter = CTOON_DELIMITER_COMMA;
    ctoon_write_flag write_flag = CTOON_WRITE_NOFLAG;
};

void bind_options(nb::module_& m) {
    nb::class_<Options>(m, "Options")
        .def(nb::init<>())
        .def_rw("read_flag", &Options::read_flag, "CTOON_READ_* flags (use ReadFlag values)")
        .def_rw("indent", &Options::indent, "Spaces per indent level (0 = compact)")
        .def_rw("delimiter", &Options::delimiter, "Array value delimiter (use Delimiter values)")
        .def_rw("write_flag", &Options::write_flag, "CTOON_WRITE_* flags (use WriteFlag values)")
        .def("__repr__", [](const Options& o) {
            std::string s = "<Options read_flag=0x" + std::to_string(o.read_flag) +
                            " write_flag=0x" + std::to_string(o.write_flag) +
                            " indent=" + std::to_string(o.indent) + ">";
            return s;
        });
}

/* -----------------------------------------------------------------------
 * Module definition
 * -------------------------------------------------------------------- */
NB_MODULE(ctoon_py, m) {
    m.doc() = "CToon - Compact TOON format for Python (C++ backend with nanobind)";

    // Version
    m.attr("__version__") = nb::str(ctoon::version::string());

    // Enums
    bind_enums(m);

    // Options
    bind_options(m);

    /* ---- decode: TOON string -> Python object ---- */
    m.def("decode",
        [](std::string_view input, const Options& opts) {
            auto doc = ctoon::document::parse(input.data(), input.size(), opts.read_flag);
            return val_to_py(doc.root());
        },
        nb::arg("input"),
        nb::arg("options") = Options(),
        "Decode a TOON string to a Python object");

    m.def("decode",
        [](nb::bytes input, const Options& opts) {
            auto doc = ctoon::document::parse(input.c_str(), nb::len(input), opts.read_flag);
            return val_to_py(doc.root());
        },
        nb::arg("input"),
        nb::arg("options") = Options(),
        "Decode TOON bytes to a Python object");

    /* ---- encode: Python object -> TOON string ---- */
    m.def("encode",
        [](nb::handle obj, const Options& opts) {
            auto doc = ctoon::mut_document::create();
            ctoon::mut_value root = py_to_mutval(doc, obj);
            doc.set_root(root);

            ctoon::write_options wo;
            wo.indent = opts.indent;
            wo.delimiter = opts.delimiter;
            ctoon::write_options wo;
            wo.indent = opts.indent;
            wo.delimiter = opts.delimiter;
            wo.flag = static_cast<ctoon::write_flag>(opts.write_flag);

            auto result = doc.write(wo);
            return std::string(result.c_str(), result.size());
        },
        nb::arg("data"),
        nb::arg("options") = Options(),
        "Encode a Python object to a TOON string");

    /* ---- loads / dumps aliases ---- */
    m.def("loads",
        [](std::string_view s, const Options& opts) {
            auto doc = ctoon::document::parse(s.data(), s.size(), opts.read_flag);
            return val_to_py(doc.root());
        },
        nb::arg("s"),
        nb::arg("options") = Options(),
        "Alias for decode()");

    m.def("dumps",
        [](nb::handle obj, const Options& opts) {
            auto doc = ctoon::mut_document::create();
            ctoon::mut_value root = py_to_mutval(doc, obj);
            doc.set_root(root);

            ctoon::write_options wo;
            wo.indent = opts.indent;
            wo.delimiter = opts.delimiter;
            wo.flag = static_cast<ctoon::write_flag>(opts.write_flag);

            auto result = doc.write(wo);
            return std::string(result.c_str(), result.size());
        },
        nb::arg("obj"),
        nb::arg("options") = Options(),
        "Alias for encode()");

    /* ---- load / dump (file I/O) ---- */
    m.def("load",
        [](const std::string& filename, const Options& opts) {
            auto doc = ctoon::document::parse_file(filename.c_str(), opts.read_flag);
            return val_to_py(doc.root());
        },
        nb::arg("filename"),
        nb::arg("options") = Options(),
        "Load a TOON file to a Python object");

    m.def("dump",
        [](nb::handle obj, const std::string& filename, const Options& opts) {
            auto doc = ctoon::mut_document::create();
            ctoon::mut_value root = py_to_mutval(doc, obj);
            doc.set_root(root);

            ctoon::write_options wo;
            wo.indent = opts.indent;
            wo.delimiter = opts.delimiter;
            wo.flag = static_cast<ctoon::write_flag>(opts.write_flag);

            doc.write_file(filename.c_str(), wo);
        },
        nb::arg("data"),
        nb::arg("filename"),
        nb::arg("options") = Options(),
        "Dump a Python object to a TOON file");

    /* ---- JSON support ---- */
    m.def("loads_json",
        [](std::string_view json) {
            auto doc = ctoon::document::parse(json);
            return val_to_py(doc.root());
        },
        nb::arg("json"),
        "Parse JSON string");

    m.def("dumps_json",
        [](nb::handle obj, int indent = 2) {
            auto doc = ctoon::mut_document::create();
            ctoon::mut_value root = py_to_mutval(doc, obj);
            doc.set_root(root);

            auto result = doc.to_json(indent);
            return std::string(result.c_str(), result.size());
        },
        nb::arg("data"),
        nb::arg("indent") = 2,
        "Serialize to JSON string");

    m.def("load_json",
        [](const std::string& filename) {
            auto doc = ctoon::document::parse_file(filename.c_str());
            return val_to_py(doc.root());
        },
        nb::arg("filename"),
        "Load JSON file");

    m.def("dump_json",
        [](nb::handle obj, const std::string& filename, int indent = 2) {
            auto doc = ctoon::mut_document::create();
            ctoon::mut_value root = py_to_mutval(doc, obj);
            doc.set_root(root);
            doc.write_file(filename.c_str());
        },
        nb::arg("data"),
        nb::arg("filename"),
        nb::arg("indent") = 2,
        "Dump to JSON file");

    /* ---- Token estimation (requires tiktoken) ---- */
    m.def("estimate_savings",
        [&m](nb::handle data) {
            // Encode to TOON and JSON, then compare token counts
            auto toon_str = nb::cast<std::string>(m.attr("encode")(data));
            auto json_str = nb::cast<std::string>(m.attr("dumps_json")(data));

            // Simple token estimation: split on whitespace/punctuation
            auto count_tokens = [](const std::string& s) -> int {
                int tokens = 0;
                bool in_token = false;
                for (char c : s) {
                    if (std::isspace(static_cast<unsigned char>(c))) {
                        if (in_token) ++tokens;
                        in_token = false;
                    } else {
                        in_token = true;
                    }
                }
                if (in_token) ++tokens;
                return tokens;
            };

            int toon_tokens = count_tokens(toon_str);
            int json_tokens = count_tokens(json_str);
            int savings = json_tokens - toon_tokens;
            double percent = json_tokens > 0 ? (100.0 * savings / json_tokens) : 0.0;

            nb::dict result;
            result["toon_tokens"] = toon_tokens;
            result["json_tokens"] = json_tokens;
            result["savings"] = savings;
            result["savings_percent"] = percent;
            return result;
        },
        nb::arg("data"),
        "Estimate token savings when using TOON vs JSON");

    m.def("compare_formats",
        [&m](nb::handle data) {
            auto toon_str = nb::cast<std::string>(m.attr("encode")(data));
            auto json_str = nb::cast<std::string>(m.attr("dumps_json")(data));

            auto count_tokens = [](const std::string& s) -> int {
                int tokens = 0;
                bool in_token = false;
                for (char c : s) {
                    if (std::isspace(static_cast<unsigned char>(c))) {
                        if (in_token) ++tokens;
                        in_token = false;
                    } else {
                        in_token = true;
                    }
                }
                if (in_token) ++tokens;
                return tokens;
            };

            int toon_tokens = count_tokens(toon_str);
            int json_tokens = count_tokens(json_str);

            nb::dict result;
            result["json_tokens"] = json_tokens;
            result["json_size"] = json_str.size();
            result["toon_tokens"] = toon_tokens;
            result["toon_size"] = toon_str.size();
            return result;
        },
        nb::arg("data"),
        "Compare token counts between JSON and TOON formats");
}