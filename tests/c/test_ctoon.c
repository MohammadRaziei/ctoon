#include "utest/utest.h"
#include "ctoon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* =========================================================================
 * Helpers
 * ========================================================================= */

static ctoon_doc *parse(const char *toon) {
    return ctoon_read(toon, strlen(toon), 0);
}

/* =========================================================================
 * Basic parsing
 * ========================================================================= */

UTEST(ctoon_tests, test_parse_basic_object) {
    ctoon_doc *doc = parse("name: Alice\nage: 30\nactive: true\ntags[3]: programming,c++,serialization");
    ASSERT_TRUE(doc != NULL);

    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(root != NULL);
    ASSERT_TRUE(ctoon_is_obj(root));

    ctoon_val *name = ctoon_obj_get(root, "name");
    ASSERT_TRUE(name != NULL);
    ASSERT_TRUE(ctoon_is_str(name));
    ASSERT_STREQ("Alice", ctoon_get_str(name));

    ctoon_val *age = ctoon_obj_get(root, "age");
    ASSERT_TRUE(age != NULL);
    ASSERT_TRUE(ctoon_is_num(age));
    ASSERT_EQ(30, (int)ctoon_get_int(age));

    ctoon_val *active = ctoon_obj_get(root, "active");
    ASSERT_TRUE(active != NULL);
    ASSERT_TRUE(ctoon_is_true(active));

    ctoon_val *tags = ctoon_obj_get(root, "tags");
    ASSERT_TRUE(tags != NULL);
    ASSERT_TRUE(ctoon_is_arr(tags));
    ASSERT_EQ(3, (int)ctoon_arr_size(tags));

    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_string) {
    ctoon_doc *doc = parse("Hello World");
    ASSERT_TRUE(doc != NULL);
    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(ctoon_is_str(root));
    ASSERT_STREQ("Hello World", ctoon_get_str(root));
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_number_uint) {
    ctoon_doc *doc = parse("42");
    ASSERT_TRUE(doc != NULL);
    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(ctoon_is_num(root));
    ASSERT_EQ(42, (int)ctoon_get_int(root));
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_number_sint) {
    ctoon_doc *doc = parse("-7");
    ASSERT_TRUE(doc != NULL);
    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(ctoon_is_sint(root));
    ASSERT_EQ(-7, (int)ctoon_get_sint(root));
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_number_real) {
    ctoon_doc *doc = parse("3.14");
    ASSERT_TRUE(doc != NULL);
    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(ctoon_is_real(root));
    double v = ctoon_get_real(root);
    ASSERT_TRUE(fabs(v - 3.14) < 1e-9);
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_boolean_true) {
    ctoon_doc *doc = parse("true");
    ASSERT_TRUE(doc != NULL);
    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(ctoon_is_true(root));
    ASSERT_TRUE(ctoon_get_bool(root));
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_boolean_false) {
    ctoon_doc *doc = parse("false");
    ASSERT_TRUE(doc != NULL);
    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(ctoon_is_false(root));
    ASSERT_FALSE(ctoon_get_bool(root));
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_null) {
    ctoon_doc *doc = parse("null");
    ASSERT_TRUE(doc != NULL);
    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(ctoon_is_null(root));
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_array) {
    ctoon_doc *doc = parse("[5]: 1,2,3,4,5");
    ASSERT_TRUE(doc != NULL);
    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(ctoon_is_arr(root));
    ASSERT_EQ(5, (int)ctoon_arr_size(root));

    for (int i = 0; i < 5; i++) {
        ctoon_val *item = ctoon_arr_get(root, i);
        ASSERT_TRUE(item != NULL);
        ASSERT_TRUE(ctoon_is_num(item));
        ASSERT_EQ(i + 1, (int)ctoon_get_int(item));
    }
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_nested_object) {
    ctoon_doc *doc = parse("person:\n  name: Bob\n  age: 25\n  active: false");
    ASSERT_TRUE(doc != NULL);

    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(ctoon_is_obj(root));

    ctoon_val *person = ctoon_obj_get(root, "person");
    ASSERT_TRUE(person != NULL);
    ASSERT_TRUE(ctoon_is_obj(person));
    ASSERT_STREQ("Bob", ctoon_get_str(ctoon_obj_get(person, "name")));
    ASSERT_EQ(25, (int)ctoon_get_int(ctoon_obj_get(person, "age")));
    ASSERT_TRUE(ctoon_is_false(ctoon_obj_get(person, "active")));

    ctoon_doc_free(doc);
}

/* =========================================================================
 * Mutable document building
 * ========================================================================= */

UTEST(ctoon_tests, test_mut_doc_build_object) {
    ctoon_mut_doc *doc = ctoon_mut_doc_new(NULL);
    ASSERT_TRUE(doc != NULL);

    ctoon_mut_val *obj = ctoon_mut_obj(doc);
    ASSERT_TRUE(obj != NULL);
    ASSERT_TRUE(ctoon_mut_obj_add_str(doc, obj, "name", "Charlie"));
    ASSERT_TRUE(ctoon_mut_obj_add_uint(doc, obj, "score", 99));
    ASSERT_TRUE(ctoon_mut_obj_add_bool(doc, obj, "active", true));
    ctoon_mut_doc_set_root(doc, obj);

    ASSERT_EQ(3, (int)ctoon_mut_obj_size(obj));
    ASSERT_STREQ("Charlie", ctoon_mut_get_str(ctoon_mut_obj_get(obj, "name")));
    ASSERT_EQ(99, (int)ctoon_mut_get_uint(ctoon_mut_obj_get(obj, "score")));

    ctoon_mut_doc_free(doc);
}

UTEST(ctoon_tests, test_mut_doc_build_array) {
    ctoon_mut_doc *doc = ctoon_mut_doc_new(NULL);
    ASSERT_TRUE(doc != NULL);

    ctoon_mut_val *arr = ctoon_mut_arr(doc);
    for (int i = 1; i <= 5; i++)
        ASSERT_TRUE(ctoon_mut_arr_add_sint(doc, arr, i));
    ctoon_mut_doc_set_root(doc, arr);

    ASSERT_EQ(5, (int)ctoon_mut_arr_size(arr));
    ASSERT_EQ(1, (int)ctoon_mut_get_sint(ctoon_mut_arr_get_first(arr)));

    ctoon_mut_doc_free(doc);
}

UTEST(ctoon_tests, test_mut_doc_write_toon) {
    ctoon_mut_doc *doc = ctoon_mut_doc_new(NULL);
    ctoon_mut_val *obj = ctoon_mut_obj(doc);
    ctoon_mut_obj_add_str(doc, obj, "key", "value");
    ctoon_mut_doc_set_root(doc, obj);

    size_t len = 0;
    char *out = ctoon_mut_write(doc, &len);
    ASSERT_TRUE(out != NULL);
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(out, "key") != NULL);
    free(out);
    ctoon_mut_doc_free(doc);
}

/* =========================================================================
 * Mutable <-> Immutable conversion
 * ========================================================================= */

UTEST(ctoon_tests, test_imut_to_mut_copy) {
    ctoon_doc *idoc = parse("name: Dave\nval: 77");
    ASSERT_TRUE(idoc != NULL);

    ctoon_mut_doc *mdoc = ctoon_doc_mut_copy(idoc, NULL);
    ASSERT_TRUE(mdoc != NULL);

    ctoon_mut_val *root = ctoon_mut_doc_get_root(mdoc);
    ASSERT_TRUE(ctoon_mut_is_obj(root));
    ASSERT_STREQ("Dave", ctoon_mut_get_str(ctoon_mut_obj_get(root, "name")));
    ASSERT_EQ(77, (int)ctoon_mut_get_int(ctoon_mut_obj_get(root, "val")));

    ctoon_doc_free(idoc);
    ctoon_mut_doc_free(mdoc);
}

UTEST(ctoon_tests, test_mut_to_imut_copy) {
    ctoon_mut_doc *mdoc = ctoon_mut_doc_new(NULL);
    ctoon_mut_val *obj = ctoon_mut_obj(mdoc);
    ctoon_mut_obj_add_uint(mdoc, obj, "x", 42);
    ctoon_mut_obj_add_str(mdoc, obj, "y", "hello");
    ctoon_mut_doc_set_root(mdoc, obj);

    ctoon_doc *idoc = ctoon_mut_doc_imut_copy(mdoc, NULL);
    ASSERT_TRUE(idoc != NULL);

    ctoon_val *root = ctoon_doc_get_root(idoc);
    ASSERT_TRUE(ctoon_is_obj(root));
    ASSERT_EQ(42, (int)ctoon_get_uint(ctoon_obj_get(root, "x")));
    ASSERT_STREQ("hello", ctoon_get_str(ctoon_obj_get(root, "y")));

    ctoon_mut_doc_free(mdoc);
    ctoon_doc_free(idoc);
}

/* =========================================================================
 * JSON API
 * ========================================================================= */

#if defined(CTOON_ENABLE_JSON) && CTOON_ENABLE_JSON

UTEST(ctoon_tests, test_read_json_object) {
    const char *json = "{\"name\":\"Eve\",\"age\":28,\"active\":true}";
    ctoon_doc *doc = ctoon_read_json((char *)(const void *)json,
                                     strlen(json), CTOON_READ_NOFLAG, NULL, NULL);
    ASSERT_TRUE(doc != NULL);

    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(ctoon_is_obj(root));
    ASSERT_STREQ("Eve", ctoon_get_str(ctoon_obj_get(root, "name")));
    ASSERT_EQ(28, (int)ctoon_get_int(ctoon_obj_get(root, "age")));
    ASSERT_TRUE(ctoon_is_true(ctoon_obj_get(root, "active")));

    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_read_json_array) {
    const char *json = "[10,20,30]";
    ctoon_doc *doc = ctoon_read_json((char *)(const void *)json,
                                     strlen(json), CTOON_READ_NOFLAG, NULL, NULL);
    ASSERT_TRUE(doc != NULL);
    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(ctoon_is_arr(root));
    ASSERT_EQ(3, (int)ctoon_arr_size(root));
    ASSERT_EQ(10, (int)ctoon_get_int(ctoon_arr_get(root, 0)));
    ASSERT_EQ(20, (int)ctoon_get_int(ctoon_arr_get(root, 1)));
    ASSERT_EQ(30, (int)ctoon_get_int(ctoon_arr_get(root, 2)));
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_write_json_from_imut_doc) {
    ctoon_doc *doc = parse("name: Frank\nage: 35");
    ASSERT_TRUE(doc != NULL);

    size_t len = 0;
    char *json = ctoon_doc_to_json(doc, 0, CTOON_WRITE_NOFLAG, NULL, &len, NULL);
    ASSERT_TRUE(json != NULL);
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(json, "Frank") != NULL);
    ASSERT_TRUE(strstr(json, "\"name\"") != NULL);
    free(json);
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_write_json_from_mut_doc) {
    ctoon_mut_doc *doc = ctoon_mut_doc_new(NULL);
    ctoon_mut_val *obj = ctoon_mut_obj(doc);
    ctoon_mut_obj_add_str(doc, obj, "city", "Tehran");
    ctoon_mut_obj_add_sint(doc, obj, "pop", 9000000);
    ctoon_mut_doc_set_root(doc, obj);

    size_t len = 0;
    char *json = ctoon_mut_doc_to_json(doc, 0, CTOON_WRITE_NOFLAG, NULL, &len, NULL);
    ASSERT_TRUE(json != NULL);
    ASSERT_TRUE(strstr(json, "Tehran") != NULL);
    ASSERT_TRUE(strstr(json, "\"city\"") != NULL);
    free(json);
    ctoon_mut_doc_free(doc);
}

UTEST(ctoon_tests, test_json_roundtrip) {
    const char *src = "{\"x\":1,\"y\":2,\"z\":3}";
    ctoon_doc *doc = ctoon_read_json((char *)(const void *)src,
                                     strlen(src), CTOON_READ_NOFLAG, NULL, NULL);
    ASSERT_TRUE(doc != NULL);

    size_t len = 0;
    char *out = ctoon_doc_to_json(doc, 0, CTOON_WRITE_NOFLAG, NULL, &len, NULL);
    ASSERT_TRUE(out != NULL);
    ASSERT_TRUE(strstr(out, "\"x\"") != NULL);
    ASSERT_TRUE(strstr(out, "\"y\"") != NULL);
    ASSERT_TRUE(strstr(out, "\"z\"") != NULL);
    free(out);
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_toon_to_json_roundtrip) {
    ctoon_doc *tdoc = parse("score: 100\nlevel: 5\nname: Grace");
    ASSERT_TRUE(tdoc != NULL);

    size_t jlen = 0;
    char *json_str = ctoon_doc_to_json(tdoc, 0, CTOON_WRITE_NOFLAG, NULL, &jlen, NULL);
    ASSERT_TRUE(json_str != NULL);

    ctoon_doc *jdoc = ctoon_read_json(json_str, jlen, CTOON_READ_NOFLAG, NULL, NULL);
    free(json_str);
    ASSERT_TRUE(jdoc != NULL);

    ctoon_val *root = ctoon_doc_get_root(jdoc);
    ASSERT_TRUE(ctoon_is_obj(root));
    ASSERT_EQ(100, (int)ctoon_get_int(ctoon_obj_get(root, "score")));
    ASSERT_STREQ("Grace", ctoon_get_str(ctoon_obj_get(root, "name")));

    ctoon_doc_free(tdoc);
    ctoon_doc_free(jdoc);
}

#endif /* CTOON_ENABLE_JSON */

/* =========================================================================
 * Iterator API
 * ========================================================================= */

UTEST(ctoon_tests, test_obj_iter) {
    ctoon_doc *doc = parse("a: 1\nb: 2\nc: 3");
    ASSERT_TRUE(doc != NULL);
    ctoon_val *root = ctoon_doc_get_root(doc);
    ASSERT_EQ(3, (int)ctoon_obj_size(root));

    int count = 0;
    ctoon_obj_iter iter = ctoon_obj_iter_with(root);
    ctoon_val *key;
    while ((key = ctoon_obj_iter_next(&iter)) != NULL) {
        ASSERT_TRUE(ctoon_is_str(key));
        ASSERT_TRUE(ctoon_obj_iter_get_val(key) != NULL);
        count++;
    }
    ASSERT_EQ(3, count);
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_arr_iter) {
    ctoon_doc *doc = parse("[4]: 10,20,30,40");
    ASSERT_TRUE(doc != NULL);
    ctoon_val *root = ctoon_doc_get_root(doc);

    int expected[] = {10, 20, 30, 40};
    int idx = 0;
    ctoon_arr_iter iter = ctoon_arr_iter_with(root);
    ctoon_val *item;
    while ((item = ctoon_arr_iter_next(&iter)) != NULL) {
        ASSERT_EQ(expected[idx], (int)ctoon_get_int(item));
        idx++;
    }
    ASSERT_EQ(4, idx);
    ctoon_doc_free(doc);
}

/* =========================================================================
 * Mutable array modification
 * ========================================================================= */

UTEST(ctoon_tests, test_mut_arr_append_remove) {
    ctoon_mut_doc *doc = ctoon_mut_doc_new(NULL);
    ctoon_mut_val *arr = ctoon_mut_arr(doc);
    ctoon_mut_doc_set_root(doc, arr);

    ctoon_mut_arr_add_sint(doc, arr, 1);
    ctoon_mut_arr_add_sint(doc, arr, 2);
    ctoon_mut_arr_add_sint(doc, arr, 3);
    ASSERT_EQ(3, (int)ctoon_mut_arr_size(arr));

    ctoon_mut_arr_remove_last(arr);
    ASSERT_EQ(2, (int)ctoon_mut_arr_size(arr));
    ASSERT_EQ(2, (int)ctoon_mut_get_sint(ctoon_mut_arr_get_last(arr)));

    ctoon_mut_doc_free(doc);
}

UTEST(ctoon_tests, test_mut_obj_put_dedup) {
    ctoon_mut_doc *doc = ctoon_mut_doc_new(NULL);
    ctoon_mut_val *obj = ctoon_mut_obj(doc);
    ctoon_mut_doc_set_root(doc, obj);

    ctoon_mut_obj_add_sint(doc, obj, "k", 1);
    ASSERT_EQ(1, (int)ctoon_mut_obj_size(obj));

    ctoon_mut_val *key = ctoon_mut_str(doc, "k");
    ctoon_mut_val *val = ctoon_mut_sint(doc, 99);
    ctoon_mut_obj_put(obj, key, val);
    ASSERT_EQ(1, (int)ctoon_mut_obj_size(obj));
    ASSERT_EQ(99, (int)ctoon_mut_get_sint(ctoon_mut_obj_get(obj, "k")));

    ctoon_mut_doc_free(doc);
}

/* =========================================================================
 * Edge cases
 * ========================================================================= */

UTEST(ctoon_tests, test_null_safe_getters) {
    ASSERT_FALSE(ctoon_is_null(NULL));
    ASSERT_FALSE(ctoon_is_bool(NULL));
    ASSERT_FALSE(ctoon_is_str(NULL));
    ASSERT_FALSE(ctoon_is_arr(NULL));
    ASSERT_FALSE(ctoon_is_obj(NULL));
    ASSERT_EQ(0, (int)ctoon_get_int(NULL));
    ASSERT_EQ(0, (int)ctoon_get_uint(NULL));
    ASSERT_TRUE(ctoon_get_str(NULL) == NULL);
    ASSERT_EQ(0, (int)ctoon_arr_size(NULL));
    ASSERT_EQ(0, (int)ctoon_obj_size(NULL));
}

UTEST(ctoon_tests, test_deeply_nested) {
    const char *toon =
        "a:\n"
        "  b:\n"
        "    c:\n"
        "      d:\n"
        "        e: deep_value";
    ctoon_doc *doc = parse(toon);
    ASSERT_TRUE(doc != NULL);

    ctoon_val *root = ctoon_doc_get_root(doc);
    ctoon_val *a = ctoon_obj_get(root, "a");
    ctoon_val *b = ctoon_obj_get(a, "b");
    ctoon_val *c = ctoon_obj_get(b, "c");
    ctoon_val *d = ctoon_obj_get(c, "d");
    ctoon_val *e = ctoon_obj_get(d, "e");
    ASSERT_TRUE(e != NULL);
    ASSERT_STREQ("deep_value", ctoon_get_str(e));

    ctoon_doc_free(doc);
}