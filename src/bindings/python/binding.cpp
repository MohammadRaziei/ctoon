#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/map.h>
#include "ctoon.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace nb = nanobind;

// Convert Python dict/list/primitive to ctoon::Value
ctoon::Value dict2value(nb::handle obj) {
    if (obj.is_none()) {
        return ctoon::Value(nullptr);
    } else if (nb::isinstance<nb::bool_>(obj)) {
        return ctoon::Value(static_cast<bool>(nb::cast<bool>(obj)));
    } else if (nb::isinstance<nb::int_>(obj)) {
        return ctoon::Value(static_cast<int64_t>(nb::cast<int64_t>(obj)));
    } else if (nb::isinstance<nb::float_>(obj)) {
        return ctoon::Value(static_cast<double>(nb::cast<double>(obj)));
    } else if (nb::isinstance<nb::str>(obj)) {
        return ctoon::Value(nb::cast<std::string>(obj));
    } else if (nb::isinstance<nb::list>(obj)) {
        ctoon::Array arr;
        nb::list pylist = nb::cast<nb::list>(obj);
        for (auto item : pylist) {
            arr.push_back(dict2value(item));
        }
        return ctoon::Value(arr);
    } else if (nb::isinstance<nb::dict>(obj)) {
        ctoon::Object map;
        nb::dict d = nb::cast<nb::dict>(obj);
        for (auto [k, v] : d) {
            map[nb::cast<std::string>(k)] = dict2value(v);
        }
        return ctoon::Value(map);
    }
    throw std::runtime_error("Unsupported Python type for ctoon::Value conversion");
}

// Convert ctoon::Value to Python dict/list/primitive
nb::object value2dict(const ctoon::Value& val) {
    if (val.isPrimitive()) {
        const auto& p = val.asPrimitive();
        return std::visit([](auto&& arg) -> nb::object {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) return nb::none();
            else return nb::cast(arg);
        }, p);
    } else if (val.isArray()) {
        nb::list out;
        for (const auto& e : val.asArray()) {
            out.append(value2dict(e));
        }
        return std::move(out);
    } else if (val.isObject()) {
        nb::dict out;
        for (const auto& kv : val.asObject()) {
            out[nb::cast(kv.first)] = value2dict(kv.second);
        }
        return std::move(out);
    }
    throw std::runtime_error("Unknown ctoon::Value type");
}

NB_MODULE(ctoon_py, m) {
    // Version information
    m.attr("__version__") = ctoon::Version::string();

    // Delimiter enum
    nb::enum_<ctoon::Delimiter>(m, "Delimiter")
        .value("Comma", ctoon::Delimiter::Comma)
        .value("Tab", ctoon::Delimiter::Tab)
        .value("Pipe", ctoon::Delimiter::Pipe);

    // EncodeOptions class
    nb::class_<ctoon::EncodeOptions>(m, "EncodeOptions")
        .def(nb::init<>())
        .def(nb::init<int>(), nb::arg("indent"))
        .def_rw("indent", &ctoon::EncodeOptions::indent)
        .def_rw("delimiter", &ctoon::EncodeOptions::delimiter)
        .def_rw("length_marker", &ctoon::EncodeOptions::lengthMarker)
        .def("set_indent", &ctoon::EncodeOptions::setIndent, nb::arg("indent"), nb::rv_policy::reference_internal)
        .def("set_delimiter", &ctoon::EncodeOptions::setDelimiter, nb::arg("delimiter"), nb::rv_policy::reference_internal)
        .def("set_length_marker", &ctoon::EncodeOptions::setLengthMarker, nb::arg("enabled"), nb::rv_policy::reference_internal);

    // DecodeOptions class
    nb::class_<ctoon::DecodeOptions>(m, "DecodeOptions")
        .def(nb::init<>())
        .def(nb::init<bool>(), nb::arg("strict"))
        .def_rw("strict", &ctoon::DecodeOptions::strict)
        .def_rw("indent", &ctoon::DecodeOptions::indent)
        .def("set_strict", &ctoon::DecodeOptions::setStrict, nb::arg("strict"), nb::rv_policy::reference_internal)
        .def("set_indent", &ctoon::DecodeOptions::setIndent, nb::arg("indent"), nb::rv_policy::reference_internal);

    // JSON functions
    m.def("loads_json",
        [](const std::string& json) {
            return value2dict(ctoon::loadsJson(json));
        },
        nb::arg("json"));

    m.def("dumps_json",
        [](nb::handle obj, int indent = 2) {
            return ctoon::dumpsJson(dict2value(obj), indent);
        },
        nb::arg("data"), nb::arg("indent") = 2);

    m.def("load_json",
        [](const std::string& filename) {
            return value2dict(ctoon::loadJson(filename));
        },
        nb::arg("filename"));

    m.def("dump_json",
        [](nb::handle obj, const std::string& filename, int indent = 2) {
            ctoon::dumpJson(dict2value(obj), filename, indent);
        },
        nb::arg("data"), nb::arg("filename"), nb::arg("indent") = 2);

    // TOON encode/decode (main API)
    m.def("encode",
        [](nb::handle obj, const ctoon::EncodeOptions& options = {}) {
            return ctoon::encode(dict2value(obj), options);
        },
        nb::arg("data"), nb::arg("options") = ctoon::EncodeOptions());

    m.def("decode",
        [](const std::string& input, const ctoon::DecodeOptions& options = {}) {
            return value2dict(ctoon::decode(input, options));
        },
        nb::arg("input"), nb::arg("options") = ctoon::DecodeOptions());

    m.def("encode_to_file",
        [](nb::handle obj, const std::string& filename, const ctoon::EncodeOptions& options = {}) {
            ctoon::encodeToFile(dict2value(obj), filename, options);
        },
        nb::arg("data"), nb::arg("filename"), nb::arg("options") = ctoon::EncodeOptions());

    m.def("decode_from_file",
        [](const std::string& filename, const ctoon::DecodeOptions& options = {}) {
            return value2dict(ctoon::decodeFromFile(filename, options));
        },
        nb::arg("filename"), nb::arg("options") = ctoon::DecodeOptions());

    // Legacy TOON functions
    m.def("loads_toon",
        [](const std::string& toon, bool strict = true) {
            return value2dict(ctoon::loadsToon(toon, strict));
        },
        nb::arg("toon"), nb::arg("strict") = true);

    m.def("dumps_toon",
        [](nb::handle obj, const ctoon::EncodeOptions& options = {}) {
            return ctoon::dumpsToon(dict2value(obj), options);
        },
        nb::arg("data"), nb::arg("options") = ctoon::EncodeOptions());

    // Generic format functions
    m.def("loads",
        [](const std::string& content, ctoon::Type format) {
            return value2dict(ctoon::loads(content, format));
        },
        nb::arg("content"), nb::arg("format"));

    m.def("dumps",
        [](nb::handle obj, ctoon::Type format, int indent = 2) {
            return ctoon::dumps(dict2value(obj), format, indent);
        },
        nb::arg("data"), nb::arg("format"), nb::arg("indent") = 2);

    // Type enum
    nb::enum_<ctoon::Type>(m, "Type")
        .value("JSON", ctoon::Type::JSON)
        .value("TOON", ctoon::Type::TOON)
        .value("UNKNOWN", ctoon::Type::UNKNOWN);
}
