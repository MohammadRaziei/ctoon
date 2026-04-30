/**
 * @file binding.cpp
 * @brief Python bindings for CToon using nanobind.
 *
 * Formula:  TOON string  <->  Python dict/list
 *
 * decode path: document  (immutable) + value
 * encode path: mut_document + mut_value
 */

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include "ctoon.hpp"

namespace nb = nanobind;

/* -----------------------------------------------------------------------
 * value (immutable) → Python
 * ----------------------------------------------------------------------- */
static nb::object val_to_py(ctoon::value v) {
    if (!v.valid() || v.is_null()) return nb::none();
    if (v.is_true())               return nb::bool_(true);
    if (v.is_false())              return nb::bool_(false);
    if (v.is_uint())               return nb::cast(static_cast<uint64_t>(v.get_uint()));
    if (v.is_sint())               return nb::cast(static_cast<int64_t>(v.get_sint()));
    if (v.is_real())               return nb::cast(v.get_real());
    if (v.is_str()) {
        auto sv = v.get_str();
        return nb::str(sv.data(), sv.size());
    }
    if (v.is_arr()) {
        nb::list out;
        for (std::size_t i = 0; i < v.arr_size(); ++i)
            out.append(val_to_py(v.arr_get(i)));
        return out;
    }
    if (v.is_obj()) {
        nb::dict out;
        for (auto kv : v.obj_items()) {
            auto sv = kv.key().get_str();
            out[nb::str(sv.data(), sv.size())] = val_to_py(kv.val());
        }
        return out;
    }
    throw std::runtime_error("Unknown value type");
}

/* -----------------------------------------------------------------------
 * Python → mut_value
 * ----------------------------------------------------------------------- */
static ctoon::mut_value py_to_mutval(ctoon::mut_document& doc, nb::handle obj) {
    if (obj.is_none())                  return doc.make_null();
    if (nb::isinstance<nb::bool_>(obj)) return doc.make_bool(nb::cast<bool>(obj));
    if (nb::isinstance<nb::int_>(obj)) {
        int64_t v = nb::cast<int64_t>(obj);
        return v < 0 ? doc.make_sint(v) : doc.make_uint(static_cast<uint64_t>(v));
    }
    if (nb::isinstance<nb::float_>(obj)) return doc.make_real(nb::cast<double>(obj));
    if (nb::isinstance<nb::str>(obj))
        return doc.make_str(nb::cast<std::string>(obj));
    if (nb::isinstance<nb::list>(obj) || nb::isinstance<nb::tuple>(obj)) {
        auto arr = doc.make_arr();
        for (nb::handle item : obj)
            arr.arr_append(py_to_mutval(doc, item));
        return arr;
    }
    if (nb::isinstance<nb::dict>(obj)) {
        auto out = doc.make_obj();
        nb::dict d = nb::cast<nb::dict>(obj);
        for (nb::handle key : d.keys())
            out.obj_put(doc.make_str(nb::cast<std::string>(key)),
                        py_to_mutval(doc, d[key]));
        return out;
    }
    throw std::runtime_error("Unsupported Python type for TOON conversion");
}

/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */
static nb::object decode_impl(std::string_view s, ctoon_read_flag flags) {
    return val_to_py(ctoon::document::parse(s.data(), s.size(), flags).root());
}

static std::string encode_impl(nb::handle obj, const ctoon::write_options& wo) {
    auto doc = ctoon::mut_document::create();
    doc.set_root(py_to_mutval(doc, obj));
    auto res = doc.write(wo);
    return std::string(res.c_str(), res.size());
}

/* -----------------------------------------------------------------------
 * Options
 * ----------------------------------------------------------------------- */
struct Options {
    ctoon::read_flag  read_flag  = ctoon::read_flag::TOON5;
    ctoon::write_flag write_flag = ctoon::write_flag::NOFLAG;
    ctoon::delimiter  delimiter  = ctoon::delimiter::COMMA;
    int               indent     = 0;
};

static ctoon::write_options make_wo(const Options& o) {
    ctoon::write_options wo;
    wo.with_flag(o.write_flag).with_delimiter(o.delimiter).with_indent(o.indent);
    return wo;
}

static ctoon_read_flag rf(const Options& o) {
    return static_cast<ctoon_read_flag>(static_cast<uint32_t>(o.read_flag));
}

/* -----------------------------------------------------------------------
 * Module
 * ----------------------------------------------------------------------- */
NB_MODULE(ctoon_py, m) {
    m.doc() = "CToon – compact TOON format for Python (nanobind backend)";
    m.attr("__version__") = ctoon::version::string();

    /* ── Delimiter ──────────────────────────────────────────────────────── */
    nb::enum_<ctoon::delimiter>(m, "Delimiter")
        .value("COMMA", ctoon::delimiter::COMMA)
        .value("TAB",   ctoon::delimiter::TAB)
        .value("PIPE",  ctoon::delimiter::PIPE)
        .export_values();

    /* ── Options ────────────────────────────────────────────────────────── */
    nb::class_<Options>(m, "Options")
        .def(nb::init<>())
        .def_rw("read_flag",  &Options::read_flag)
        .def_rw("write_flag", &Options::write_flag)
        .def_rw("delimiter",  &Options::delimiter)
        .def_rw("indent",     &Options::indent);

    /* ── encode / decode ────────────────────────────────────────────────── */
    m.def("encode",
        [](nb::handle obj, const Options& o) { return encode_impl(obj, make_wo(o)); },
        nb::arg("data"), nb::arg("options") = Options());

    m.def("decode",
        [](std::string_view s, const Options& o) { return decode_impl(s, rf(o)); },
        nb::arg("input"), nb::arg("options") = Options());

    /* ── dumps / loads  (mirrors json.dumps / json.loads) ───────────────── */
    m.def("dumps",
        [](nb::handle obj, const Options& o) { return encode_impl(obj, make_wo(o)); },
        nb::arg("obj"), nb::arg("options") = Options());

    m.def("loads",
        [](std::string_view s, const Options& o) { return decode_impl(s, rf(o)); },
        nb::arg("s"), nb::arg("options") = Options());

    /* ── dump / load  (file I/O, mirrors json.dump / json.load) ─────────── */
    m.def("dump",
        [](nb::handle obj, const std::string& path, const Options& o) {
            auto doc = ctoon::mut_document::create();
            doc.set_root(py_to_mutval(doc, obj));
            doc.write_file(path.c_str(), make_wo(o));
        },
        nb::arg("obj"), nb::arg("fp"), nb::arg("options") = Options());

    m.def("load",
        [](const std::string& path, const Options& o) {
            return val_to_py(ctoon::document::parse_file(path.c_str(), rf(o)).root());
        },
        nb::arg("fp"), nb::arg("options") = Options());

    /* ── JSON ───────────────────────────────────────────────────────────── */
    m.def("dumps_json",
        [](nb::handle obj, int indent) {
            auto doc = ctoon::mut_document::create();
            doc.set_root(py_to_mutval(doc, obj));
            auto res = doc.to_json(indent);
            return std::string(res.c_str(), res.size());
        },
        nb::arg("obj"), nb::arg("indent") = 2);

    m.def("loads_json",
        [](std::string_view s) {
            // explicit (data, size) overload — no MSVC ambiguity
            return val_to_py(ctoon::document::from_json(s.data(), s.size()).root());
        },
        nb::arg("s"));

    m.def("dump_json",
        [](nb::handle obj, const std::string& path, int indent) {
            auto doc = ctoon::mut_document::create();
            doc.set_root(py_to_mutval(doc, obj));
            auto res = doc.to_json(indent);
            FILE *fp = std::fopen(path.c_str(), "wb");
            if (!fp) throw std::runtime_error("Cannot open file: " + path);
            std::fwrite(res.c_str(), 1, res.size(), fp);
            std::fclose(fp);
        },
        nb::arg("obj"), nb::arg("fp"), nb::arg("indent") = 2);

    m.def("load_json",
        [](const std::string& path) {
            return val_to_py(ctoon::document::from_json_file(path.c_str()).root());
        },
        nb::arg("fp"));
}