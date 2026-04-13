//
// Created by mohammad on 11/1/25.
//

// ctoon_bind.cpp
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/variant.h>
#include "ctoon.h"


#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)


namespace nb = nanobind;


ctoon::Value dict2value(nb::handle obj) {
    if (obj.is_none()) {
        return Value(nullptr);
    } else if (nb::isinstance<nb::bool_>(obj)) {
        return Value(static_cast<bool>(nb::cast<bool>(obj)));
    } else if (nb::isinstance<nb::int_>(obj)) {
        return Value(static_cast<int64_t>(nb::cast<int64_t>(obj)));
    } else if (nb::isinstance<nb::float_>(obj)) {
        return Value(static_cast<double>(nb::cast<double>(obj)));
    } else if (nb::isinstance<nb::str>(obj)) {
        return Value(nb::cast<std::string>(obj));
    } else if (nb::isinstance<nb::list>(obj)) {
        ctoon::Array arr;
        nb::list pylist = nb::cast<nb::list>(obj);
        for (auto item : pylist)
            arr.push_back(dict2value(item));
        return Value(arr);
    } else if (nb::isinstance<nb::dict>(obj)) {
        ctoon::Object map;
        nb::dict d = nb::cast<nb::dict>(obj);
        for (auto [k, v] : d) {
            map[nb::cast<std::string>(k)] = dict2value(v);
        }
        return Value(map);
    }
    throw std::runtime_error("Unsupported Python type for ctoon::Value conversion");
}

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
        for (const auto& e : val.asArray())
            out.append(value2dict(e));
        return std::move(out);
    } else if (val.isObject()) {
        nb::dict out;
        for (const auto& kv : val.asObject())
            out[nb::cast(kv.first)] = value2dict(kv.second);
        return std::move(out);
    }
    throw std::runtime_error("Unknown ctoon::Value type");
}


template<typename Func, typename... Args>
std::string dumpsDict(Func f, const ctoon::Object& dict, Args&&... args) {
    ctoon::Value v(dict);
    return f(v, std::forward<Args>(args)...);
}

template<typename Func, typename... Args>
std::string dumpDict(Func f, const ctoon::Object& dict, Args&&... args) {
    ctoon::Value v(dict);
    return f(v, std::forward<Args>(args)...);

// loadsXDict : str -> dict
template<typename Func, typename... Args>
ctoon::Object loadsDict(Func f, const std::string& s, Args&&... args) {
    ctoon::Value v = f(s, std::forward<Args>(args)...);
    return v.asObject();
}



NB_MODULE(NB_MODULE_NAME, m) {
    nb::class_<ctoon::Value>(m, "Value")
    .def(nb::init<>())
    .def("is_object", &ctoon::Value::isObject)
    .def("is_array", &ctoon::Value::isArray)
    .def("is_primitive", &ctoon::Value::isPrimitive);

    nb::enum_<ctoon::Delimiter>(m, "Delimiter")
        .value("Comma", ctoon::Delimiter::Comma)
        .value("Tab", ctoon::Delimiter::Tab)
        .value("Pipe", ctoon::Delimiter::Pipe);

    nb::class_<ctoon::ToonOptions>(m, "ToonOptions")
        .def(nb::init<>())
        .def(nb::init<int>(), nb::arg("indent"))
        .def("set_indent", &ctoon::ToonOptions::setIndent, nb::arg("indent"), nb::rv_policy::reference_internal)
        .def("set_delimiter", &ctoon::ToonOptions::setDelimiter, nb::arg("delimiter"), nb::rv_policy::reference_internal)
        .def("set_length_marker", &ctoon::ToonOptions::setLengthMarker, nb::arg("enabled"), nb::rv_policy::reference_internal)
        .def("set_strict", &ctoon::ToonOptions::setStrict, nb::arg("strict"), nb::rv_policy::reference_internal)
        .def("indent", &ctoon::ToonOptions::indent)
        .def("delimiter", &ctoon::ToonOptions::delimiter)
        .def("length_marker", &ctoon::ToonOptions::lengthMarker)
        .def("strict", &ctoon::ToonOptions::strict);

    m.def("value_loads_json", &ctoon::loadsJson);
    m.def("value_dumps_json", &ctoon::dumpsJson);
    m.def("value_loads_toon",
          [](const std::string& toon, const ctoon::ToonOptions& options) {
              return ctoon::loadsToon(toon, options);
          },
          nb::arg("toon"), nb::arg("options") = ctoon::ToonOptions());
    m.def("value_dumps_toon",
          [](const ctoon::Value& value, const ctoon::ToonOptions& options) {
              return ctoon::dumpsToon(value, options);
          },
          nb::arg("value"), nb::arg("options") = ctoon::ToonOptions());

    m.def("loads_json", &ctoon::loadsJson)

    #ifdef VERSION_INFO
        m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
    #else
        m.attr("__version__") = "dev";
    #endif
}
