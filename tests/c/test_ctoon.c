#include "utest/utest.h"
#include "ctoon.h"
#include <string.h>
#include <stdio.h>

// Test basic TOON parsing
UTEST(ctoon_tests, test_parse_basic_object) {
    const char* toon_data = "name: Alice\nage: 30\nactive: true\ntags[3]: programming,c++,serialization";
    
    ctoon_doc* doc = ctoon_read(toon_data, strlen(toon_data), 0);
    ASSERT_TRUE(doc != NULL);
    
    ctoon_val* root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(root != NULL);
    ASSERT_TRUE(ctoon_is_obj(root));
    
    // Check name
    ctoon_val* name = ctoon_obj_get(root, "name");
    ASSERT_TRUE(name != NULL);
    ASSERT_TRUE(ctoon_is_str(name));
    ASSERT_STREQ("Alice", ctoon_get_str(name));
    
    // Check age
    ctoon_val* age = ctoon_obj_get(root, "age");
    ASSERT_TRUE(age != NULL);
    ASSERT_TRUE(ctoon_is_num(age));
    ASSERT_EQ(30, (int)ctoon_get_int(age));
    
    // Check active
    ctoon_val* active = ctoon_obj_get(root, "active");
    ASSERT_TRUE(active != NULL);
    ASSERT_TRUE(ctoon_is_true(active));
    
    // Check tags
    ctoon_val* tags = ctoon_obj_get(root, "tags");
    ASSERT_TRUE(tags != NULL);
    ASSERT_TRUE(ctoon_is_arr(tags));
    ASSERT_EQ(3, (int)ctoon_arr_size(tags));
    
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_string) {
    const char* toon_data = "Hello World";
    
    ctoon_doc* doc = ctoon_read(toon_data, strlen(toon_data), 0);
    ASSERT_TRUE(doc != NULL);
    
    ctoon_val* root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(root != NULL);
    ASSERT_TRUE(ctoon_is_str(root));
    ASSERT_STREQ("Hello World", ctoon_get_str(root));
    
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_number) {
    const char* toon_data = "42";
    
    ctoon_doc* doc = ctoon_read(toon_data, strlen(toon_data), 0);
    ASSERT_TRUE(doc != NULL);
    
    ctoon_val* root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(root != NULL);
    ASSERT_TRUE(ctoon_is_num(root));
    ASSERT_EQ(42, (int)ctoon_get_int(root));
    
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_boolean) {
    const char* toon_data = "true";
    
    ctoon_doc* doc = ctoon_read(toon_data, strlen(toon_data), 0);
    ASSERT_TRUE(doc != NULL);
    
    ctoon_val* root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(root != NULL);
    ASSERT_TRUE(ctoon_is_true(root));
    
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_array) {
    const char* toon_data = "[5]: 1,2,3,4,5";
    
    ctoon_doc* doc = ctoon_read(toon_data, strlen(toon_data), 0);
    ASSERT_TRUE(doc != NULL);
    
    ctoon_val* root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(root != NULL);
    ASSERT_TRUE(ctoon_is_arr(root));
    ASSERT_EQ(5, (int)ctoon_arr_size(root));
    
    for (int i = 0; i < 5; i++) {
        ctoon_val* item = ctoon_arr_get(root, i);
        ASSERT_TRUE(item != NULL);
        ASSERT_TRUE(ctoon_is_num(item));
        ASSERT_EQ(i + 1, (int)ctoon_get_int(item));
    }
    
    ctoon_doc_free(doc);
}

UTEST(ctoon_tests, test_parse_nested_object) {
    const char* toon_data = "person:\n  name: Bob\n  age: 25\n  active: false";
    
    ctoon_doc* doc = ctoon_read(toon_data, strlen(toon_data), 0);
    ASSERT_TRUE(doc != NULL);
    
    ctoon_val* root = ctoon_doc_get_root(doc);
    ASSERT_TRUE(root != NULL);
    ASSERT_TRUE(ctoon_is_obj(root));
    
    ctoon_val* person = ctoon_obj_get(root, "person");
    ASSERT_TRUE(person != NULL);
    ASSERT_TRUE(ctoon_is_obj(person));
    
    ctoon_val* name = ctoon_obj_get(person, "name");
    ASSERT_TRUE(name != NULL);
    ASSERT_TRUE(ctoon_is_str(name));
    ASSERT_STREQ("Bob", ctoon_get_str(name));
    
    ctoon_val* age = ctoon_obj_get(person, "age");
    ASSERT_TRUE(age != NULL);
    ASSERT_TRUE(ctoon_is_num(age));
    ASSERT_EQ(25, (int)ctoon_get_int(age));
    
    ctoon_val* active = ctoon_obj_get(person, "active");
    ASSERT_TRUE(active != NULL);
    ASSERT_TRUE(ctoon_is_false(active));
    
    ctoon_doc_free(doc);
}