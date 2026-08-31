/* =========================================================================
 * Spec conformance tests
 *
 * Runs ctoon against the official fixture suite published in
 * https://github.com/toon-format/spec (tests/fixtures/{encode,decode}/*.json).
 * A copy of those fixtures lives under tests/data/toon-spec/ so the suite
 * is self-contained and doesn't need network access at test time.
 *
 * Each fixture file is itself JSON, shaped like:
 *   { "tests": [ { "name", "input", "expected", "options"?, "shouldError"? } ] }
 *
 * We parse the fixture file with ctoon's own JSON reader, then:
 *  - encode/*.json  : take "input" (already a ctoon::value), write it as
 *                      TOON with the requested options, compare to "expected".
 *  - decode/*.json  : parse "input" (TOON text) and compare the resulting
 *                      tree against "expected" (JSON) via value::equals(),
 *                      or expect a parse failure when "shouldError" is set.
 * ========================================================================= */

#include "utest/utest.h"
#include "ctoon.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#if defined(CTOON_ENABLE_JSON) && CTOON_ENABLE_JSON

namespace {

#ifndef CTOON_SPEC_FIXTURES_DIR
#define CTOON_SPEC_FIXTURES_DIR "tests/data/toon-spec"
#endif

std::string read_file(const std::string &path) {
    std::ifstream f(path.c_str(), std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

ctoon::delimiter parse_delimiter(const ctoon::value &options) {
    ctoon::value d = options.obj_get("delimiter");
    if (d && d.is_str()) {
        ctoon::string_view sv = d.get_str();
        if (!sv.empty()) {
            switch (sv[0]) {
                case '\t': return ctoon::delimiter::TAB;
                case '|':  return ctoon::delimiter::PIPE;
                default:   return ctoon::delimiter::COMMA;
            }
        }
    }
    return ctoon::delimiter::COMMA;
}

int parse_indent(const ctoon::value &options, int fallback) {
    ctoon::value i = options.obj_get("indentSize");
    if (i && i.is_num()) return static_cast<int>(i.get_uint());
    return fallback;
}

bool parse_strict(const ctoon::value &options, bool fallback) {
    ctoon::value s = options.obj_get("strict");
    if (s && (s.is_true() || s.is_false())) return s.is_true();
    return fallback;
}

// Runs every "encode" case in one fixture file: JSON value -> TOON text.
// utest's ASSERT_* macros rely on a local variable that only exists inside a
// UTEST(...) body, so this helper reports failures itself and returns a
// pass/fail bool for the caller to assert on.
bool run_encode_fixture(const char *filename) {
    std::string path = std::string(CTOON_SPEC_FIXTURES_DIR) + "/encode/" + filename;
    std::string src  = read_file(path);
    if (src.empty()) { std::fprintf(stderr, "  fixture not found: %s\n", path.c_str()); return false; }

    ctoon::document fixture = ctoon::document::from_json(src);
    if (!fixture) return false;

    ctoon::value tests = fixture.root()["tests"];
    if (!tests.is_arr()) return false;

    bool all_ok = true;
    for (std::size_t i = 0; i < tests.arr_size(); ++i) {
        ctoon::value t = tests[i];
        ctoon::value input    = t["input"];
        ctoon::value expected = t["expected"];
        ctoon::value options  = t["options"];
        ctoon::value name     = t["name"];

        ctoon::write_options opts;
        opts.with_delimiter(parse_delimiter(options))
            .with_indent(parse_indent(options, 2));

        std::string got;
        try {
            got = input.to_string(opts).str();
        } catch (const ctoon::error &e) {
            std::fprintf(stderr, "  [%s] '%.*s': unexpected throw: %s\n", filename,
                         (int)name.get_str().size(), name.get_str().data(), e.what());
            all_ok = false;
            continue;
        }

        std::string want(expected.get_str());
        if (want != got) {
            std::fprintf(stderr, "  [%s] '%.*s': expected %s, got %s\n", filename,
                         (int)name.get_str().size(), name.get_str().data(),
                         want.c_str(), got.c_str());
            all_ok = false;
        }
    }
    return all_ok;
}

// Runs every "decode" case in one fixture file: TOON text -> JSON value.
bool run_decode_fixture(const char *filename) {
    std::string path = std::string(CTOON_SPEC_FIXTURES_DIR) + "/decode/" + filename;
    std::string src  = read_file(path);
    if (src.empty()) { std::fprintf(stderr, "  fixture not found: %s\n", path.c_str()); return false; }

    ctoon::document fixture = ctoon::document::from_json(src);
    if (!fixture) return false;

    ctoon::value tests = fixture.root()["tests"];
    if (!tests.is_arr()) return false;

    bool all_ok = true;
    for (std::size_t i = 0; i < tests.arr_size(); ++i) {
        ctoon::value t = tests[i];
        ctoon::value input       = t["input"];
        ctoon::value expected    = t["expected"];
        ctoon::value options     = t["options"];
        ctoon::value name        = t["name"];
        ctoon::value should_err  = t["shouldError"];
        bool expect_error = should_err && should_err.is_true();

        std::string toon_text(input.get_str());

        // strict=false in the spec maps to ALLOW_INF_AND_NAN, mirroring the
        // ctoon CLI's own --no-strict handling (see src/cli/cli.cpp).
        bool strict = parse_strict(options, true);
        ctoon::read_flag flags = strict ? ctoon::read_flag::NOFLAG
                                         : ctoon::read_flag::ALLOW_INF_AND_NAN;
        int indent_size = parse_indent(options, 2);

        bool threw = false;
        ctoon::document doc;
        try {
            doc = indent_size == 2
                ? ctoon::document::parse(toon_text, flags)
                : ctoon::document::parse_indent(toon_text, indent_size, flags);
        } catch (const ctoon::parse_error &) {
            threw = true;
        }

        if (expect_error) {
            if (!threw && doc.valid()) {
                std::fprintf(stderr, "  [%s] '%.*s': expected a parse error, but succeeded\n",
                             filename, (int)name.get_str().size(), name.get_str().data());
                all_ok = false;
            }
            continue;
        }

        if (threw || !doc.valid()) {
            std::fprintf(stderr, "  [%s] '%.*s': unexpected parse failure\n",
                         filename, (int)name.get_str().size(), name.get_str().data());
            all_ok = false;
            continue;
        }

        bool matches;
        if (expected.is_obj() && expected.obj_size() == 0) {
            // "empty object" is the canonical result for an empty document.
            matches = doc.root().is_obj() && doc.root().obj_size() == 0;
        } else {
            matches = doc.root().equals(expected);
        }
        if (!matches) {
            std::fprintf(stderr, "  [%s] '%.*s': decoded value did not match expected\n",
                         filename, (int)name.get_str().size(), name.get_str().data());
            all_ok = false;
        }
    }
    return all_ok;
}

} // namespace

/* --- encode fixtures ---------------------------------------------------- */

UTEST(spec_encode, primitives)      { ASSERT_TRUE(run_encode_fixture("primitives.json")); }
UTEST(spec_encode, objects)         { ASSERT_TRUE(run_encode_fixture("objects.json")); }
UTEST(spec_encode, objects_keyed)   { ASSERT_TRUE(run_encode_fixture("objects-keyed.json")); }
UTEST(spec_encode, arrays_primitive){ ASSERT_TRUE(run_encode_fixture("arrays-primitive.json")); }
UTEST(spec_encode, arrays_nested)   { ASSERT_TRUE(run_encode_fixture("arrays-nested.json")); }
UTEST(spec_encode, arrays_tabular)  { ASSERT_TRUE(run_encode_fixture("arrays-tabular.json")); }
UTEST(spec_encode, arrays_objects)  { ASSERT_TRUE(run_encode_fixture("arrays-objects.json")); }
UTEST(spec_encode, delimiters)      { ASSERT_TRUE(run_encode_fixture("delimiters.json")); }
UTEST(spec_encode, whitespace)      { ASSERT_TRUE(run_encode_fixture("whitespace.json")); }

/* --- decode fixtures ----------------------------------------------------- */

UTEST(spec_decode, primitives)          { ASSERT_TRUE(run_decode_fixture("primitives.json")); }
UTEST(spec_decode, objects)             { ASSERT_TRUE(run_decode_fixture("objects.json")); }
UTEST(spec_decode, objects_keyed)       { ASSERT_TRUE(run_decode_fixture("objects-keyed.json")); }
UTEST(spec_decode, arrays_primitive)    { ASSERT_TRUE(run_decode_fixture("arrays-primitive.json")); }
UTEST(spec_decode, arrays_nested)       { ASSERT_TRUE(run_decode_fixture("arrays-nested.json")); }
UTEST(spec_decode, arrays_tabular)      { ASSERT_TRUE(run_decode_fixture("arrays-tabular.json")); }
UTEST(spec_decode, numbers)             { ASSERT_TRUE(run_decode_fixture("numbers.json")); }
UTEST(spec_decode, delimiters)          { ASSERT_TRUE(run_decode_fixture("delimiters.json")); }
UTEST(spec_decode, whitespace)          { ASSERT_TRUE(run_decode_fixture("whitespace.json")); }
UTEST(spec_decode, blank_lines)         { ASSERT_TRUE(run_decode_fixture("blank-lines.json")); }
UTEST(spec_decode, comments)            { ASSERT_TRUE(run_decode_fixture("comments.json")); }
UTEST(spec_decode, root_form)           { ASSERT_TRUE(run_decode_fixture("root-form.json")); }
UTEST(spec_decode, validation_errors)   { ASSERT_TRUE(run_decode_fixture("validation-errors.json")); }
UTEST(spec_decode, indentation_errors)  { ASSERT_TRUE(run_decode_fixture("indentation-errors.json")); }

#endif /* CTOON_ENABLE_JSON */
