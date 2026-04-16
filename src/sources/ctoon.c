#include "../../include/ctoon/ctoon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/*==============================================================================
 * MARK: - Internal Structures
 *============================================================================*/

typedef struct ctoon_node {
    ctoon_type type;
    union {
        double num;
        char* str;
        struct {
            struct ctoon_node** items;
            size_t count;
            size_t capacity;
        } arr;
        struct {
            char** keys;
            struct ctoon_node** values;
            size_t count;
            size_t capacity;
        } obj;
    } u;
} ctoon_node;

struct ctoon_doc {
    ctoon_node* root;
    char* error;
    size_t error_pos;
    char* alloc_ptr;
    size_t alloc_size;
};

struct ctoon_val {
    ctoon_node* node;
};

/*==============================================================================
 * MARK: - Memory Management
 *============================================================================*/

static void* ctoon_alloc(ctoon_doc* doc, size_t size) {
    if (doc->alloc_ptr + size > doc->alloc_ptr + doc->alloc_size) {
        return NULL;
    }
    void* ptr = doc->alloc_ptr;
    doc->alloc_ptr += size;
    return ptr;
}

static ctoon_node* ctoon_new_node(ctoon_doc* doc, ctoon_type type) {
    ctoon_node* node = ctoon_alloc(doc, sizeof(ctoon_node));
    if (!node) return NULL;
    node->type = type;
    return node;
}

ctoon_doc* ctoon_doc_new(const char* data, size_t len, size_t max_memory) {
    ctoon_doc* doc = malloc(sizeof(ctoon_doc));
    if (!doc) return NULL;
    
    doc->root = NULL;
    doc->error = NULL;
    doc->error_pos = 0;
    doc->alloc_size = max_memory ? max_memory : len * 2;
    doc->alloc_ptr = malloc(doc->alloc_size);
    if (!doc->alloc_ptr) {
        free(doc);
        return NULL;
    }
    
    return doc;
}

void ctoon_doc_free(ctoon_doc* doc) {
    if (doc) {
        free(doc->alloc_ptr);
        free(doc);
    }
}

ctoon_val* ctoon_doc_get_root(ctoon_doc* doc) {
    return doc ? (ctoon_val*)&doc->root : NULL;
}

size_t ctoon_doc_get_read_size(ctoon_doc* doc) {
    return doc ? doc->alloc_size : 0;
}

size_t ctoon_doc_get_val_count(ctoon_doc* doc) {
    // Simple implementation - count nodes
    return 0;
}

/*==============================================================================
 * MARK: - Value Type
 *============================================================================*/

ctoon_type ctoon_get_type(ctoon_val* val) {
    return val && val->node ? val->node->type : CTOON_TYPE_NULL;
}

ctoon_subtype ctoon_get_subtype(ctoon_val* val) {
    return CTOON_SUBTYPE_NONE;
}

ctoon_type ctoon_get_tag(ctoon_val* val) {
    return ctoon_get_type(val);
}

bool ctoon_is_null(ctoon_val* val) {
    return ctoon_get_type(val) == CTOON_TYPE_NULL;
}

bool ctoon_is_true(ctoon_val* val) {
    return ctoon_get_type(val) == CTOON_TYPE_TRUE;
}

bool ctoon_is_false(ctoon_val* val) {
    return ctoon_get_type(val) == CTOON_TYPE_FALSE;
}

bool ctoon_is_bool(ctoon_val* val) {
    ctoon_type type = ctoon_get_type(val);
    return type == CTOON_TYPE_TRUE || type == CTOON_TYPE_FALSE;
}

bool ctoon_is_uint(ctoon_val* val) {
    return ctoon_is_num(val);
}

bool ctoon_is_sint(ctoon_val* val) {
    return ctoon_is_num(val);
}

bool ctoon_is_int(ctoon_val* val) {
    return ctoon_is_num(val);
}

bool ctoon_is_real(ctoon_val* val) {
    return ctoon_is_num(val);
}

bool ctoon_is_num(ctoon_val* val) {
    return ctoon_get_type(val) == CTOON_TYPE_NUMBER;
}

bool ctoon_is_str(ctoon_val* val) {
    return ctoon_get_type(val) == CTOON_TYPE_STRING;
}

bool ctoon_is_arr(ctoon_val* val) {
    return ctoon_get_type(val) == CTOON_TYPE_ARRAY;
}

bool ctoon_is_obj(ctoon_val* val) {
    return ctoon_get_type(val) == CTOON_TYPE_OBJECT;
}

bool ctoon_is_ctn(ctoon_val* val) {
    return ctoon_is_arr(val) || ctoon_is_obj(val);
}

bool ctoon_is_raw(ctoon_val* val) {
    return ctoon_get_type(val) == CTOON_TYPE_RAW;
}

/*==============================================================================
 * MARK: - Value Content
 *============================================================================*/

bool ctoon_get_bool(ctoon_val* val) {
    return ctoon_is_true(val);
}

uint64_t ctoon_get_uint(ctoon_val* val) {
    if (!ctoon_is_num(val)) return 0;
    return (uint64_t)val->node->u.num;
}

int64_t ctoon_get_sint(ctoon_val* val) {
    if (!ctoon_is_num(val)) return 0;
    return (int64_t)val->node->u.num;
}

int ctoon_get_int(ctoon_val* val) {
    if (!ctoon_is_num(val)) return 0;
    return (int)val->node->u.num;
}

double ctoon_get_real(ctoon_val* val) {
    if (!ctoon_is_num(val)) return 0.0;
    return val->node->u.num;
}

double ctoon_get_num(ctoon_val* val) {
    return ctoon_get_real(val);
}

const char* ctoon_get_str(ctoon_val* val) {
    if (!ctoon_is_str(val)) return NULL;
    return val->node->u.str;
}

size_t ctoon_get_len(ctoon_val* val) {
    if (!ctoon_is_str(val)) return 0;
    return strlen(val->node->u.str);
}

bool ctoon_equals_str(ctoon_val* val, const char* str) {
    if (!ctoon_is_str(val) || !str) return false;
    return strcmp(val->node->u.str, str) == 0;
}

bool ctoon_equals_strn(ctoon_val* val, const char* str, size_t len) {
    if (!ctoon_is_str(val) || !str) return false;
    return strncmp(val->node->u.str, str, len) == 0 && val->node->u.str[len] == '\0';
}

const char* ctoon_get_raw(ctoon_val* val) {
    return NULL;
}

/*==============================================================================
 * MARK: - Array Access
 *============================================================================*/

size_t ctoon_arr_size(ctoon_val* val) {
    if (!ctoon_is_arr(val)) return 0;
    return val->node->u.arr.count;
}

ctoon_val* ctoon_arr_get(ctoon_val* val, size_t idx) {
    if (!ctoon_is_arr(val) || idx >= val->node->u.arr.count) return NULL;
    return (ctoon_val*)&val->node->u.arr.items[idx];
}

ctoon_val* ctoon_arr_get_first(ctoon_val* val) {
    return ctoon_arr_get(val, 0);
}

ctoon_val* ctoon_arr_get_next(ctoon_val* val) {
    return NULL;
}

/*==============================================================================
 * MARK: - Object Access
 *============================================================================*/

size_t ctoon_obj_size(ctoon_val* val) {
    if (!ctoon_is_obj(val)) return 0;
    return val->node->u.obj.count;
}

ctoon_val* ctoon_obj_get(ctoon_val* val, const char* key) {
    return ctoon_obj_getn(val, key, strlen(key));
}

ctoon_val* ctoon_obj_getn(ctoon_val* val, const char* key, size_t len) {
    if (!ctoon_is_obj(val)) return NULL;
    for (size_t i = 0; i < val->node->u.obj.count; i++) {
        if (strncmp(val->node->u.obj.keys[i], key, len) == 0 &&
            val->node->u.obj.keys[i][len] == '\0') {
            return (ctoon_val*)&val->node->u.obj.values[i];
        }
    }
    return NULL;
}

ctoon_val* ctoon_obj_iter_get(ctoon_val* val, ctoon_val** key) {
    if (!ctoon_is_obj(val) || val->node->u.obj.count == 0) return NULL;
    if (key) *key = (ctoon_val*)&val->node->u.obj.keys[0];
    return (ctoon_val*)&val->node->u.obj.values[0];
}

ctoon_val* ctoon_obj_iter_next(ctoon_val* val, ctoon_val** key) {
    return NULL;
}

const char* ctoon_obj_iter_key(ctoon_val* key) {
    return (const char*)key;
}

size_t ctoon_obj_iter_key_len(ctoon_val* key) {
    const char* str = ctoon_obj_iter_key(key);
    return str ? strlen(str) : 0;
}

/*==============================================================================
 * MARK: - TOON Parsing Implementation
 *============================================================================*/

static void skip_whitespace(const char** p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
}

static int parse_string(ctoon_doc* doc, const char** p, char** out) {
    const char* start = *p;
    while (**p && **p != ':' && **p != '\n' && **p != '\r') (*p)++;
    
    size_t len = *p - start;
    char* str = ctoon_alloc(doc, len + 1);
    if (!str) return 0;
    
    memcpy(str, start, len);
    str[len] = '\0';
    
    // Trim trailing whitespace
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
    
    *out = str;
    return 1;
}

static int parse_value(ctoon_doc* doc, const char** p, ctoon_node** node);

static int parse_array(ctoon_doc* doc, const char** p, ctoon_node** node) {
    *node = ctoon_new_node(doc, CTOON_TYPE_ARRAY);
    if (!*node) return 0;
    
    (*node)->u.arr.count = 0;
    (*node)->u.arr.capacity = 4;
    (*node)->u.arr.items = ctoon_alloc(doc, sizeof(ctoon_node*) * 4);
    if (!(*node)->u.arr.items) return 0;
    
    const char* start = *p;
    while (**p && **p != ']') {
        if (**p == ',') (*p)++;
        
        ctoon_node* item;
        if (!parse_value(doc, p, &item)) return 0;
        
        if ((*node)->u.arr.count >= (*node)->u.arr.capacity) {
            size_t new_cap = (*node)->u.arr.capacity * 2;
            ctoon_node** new_items = ctoon_alloc(doc, sizeof(ctoon_node*) * new_cap);
            if (!new_items) return 0;
            memcpy(new_items, (*node)->u.arr.items, sizeof(ctoon_node*) * (*node)->u.arr.count);
            (*node)->u.arr.items = new_items;
            (*node)->u.arr.capacity = new_cap;
        }
        
        (*node)->u.arr.items[(*node)->u.arr.count++] = item;
    }
    
    if (**p == ']') (*p)++;
    return 1;
}

static int parse_object(ctoon_doc* doc, const char** p, ctoon_node** node) {
    *node = ctoon_new_node(doc, CTOON_TYPE_OBJECT);
    if (!*node) return 0;
    
    (*node)->u.obj.count = 0;
    (*node)->u.obj.capacity = 4;
    (*node)->u.obj.keys = ctoon_alloc(doc, sizeof(char*) * 4);
    (*node)->u.obj.values = ctoon_alloc(doc, sizeof(ctoon_node*) * 4);
    if (!(*node)->u.obj.keys || !(*node)->u.obj.values) return 0;
    
    while (**p && **p != '}') {
        skip_whitespace(p);
        if (**p == ',') (*p)++;
        skip_whitespace(p);
        
        char* key;
        if (!parse_string(doc, p, &key)) return 0;
        
        skip_whitespace(p);
        if (**p != ':') {
            doc->error = "Expected ':' after key";
            return 0;
        }
        (*p)++;
        skip_whitespace(p);
        
        ctoon_node* value;
        if (!parse_value(doc, p, &value)) return 0;
        
        if ((*node)->u.obj.count >= (*node)->u.obj.capacity) {
            size_t new_cap = (*node)->u.obj.capacity * 2;
            char** new_keys = ctoon_alloc(doc, sizeof(char*) * new_cap);
            ctoon_node** new_values = ctoon_alloc(doc, sizeof(ctoon_node*) * new_cap);
            if (!new_keys || !new_values) return 0;
            
            memcpy(new_keys, (*node)->u.obj.keys, sizeof(char*) * (*node)->u.obj.count);
            memcpy(new_values, (*node)->u.obj.values, sizeof(ctoon_node*) * (*node)->u.obj.count);
            
            (*node)->u.obj.keys = new_keys;
            (*node)->u.obj.values = new_values;
            (*node)->u.obj.capacity = new_cap;
        }
        
        (*node)->u.obj.keys[(*node)->u.obj.count] = key;
        (*node)->u.obj.values[(*node)->u.obj.count] = value;
        (*node)->u.obj.count++;
        
        skip_whitespace(p);
    }
    
    if (**p == '}') (*p)++;
    return 1;
}

static int parse_value(ctoon_doc* doc, const char** p, ctoon_node** node) {
    skip_whitespace(p);
    
    if (!**p) {
        doc->error = "Unexpected end of input";
        return 0;
    }
    
    if (**p == '{') {
        (*p)++;
        return parse_object(doc, p, node);
    }
    
    if (**p == '[') {
        (*p)++;
        return parse_array(doc, p, node);
    }
    
    if (**p == '"') {
        // String
        (*p)++;
        const char* start = *p;
        while (**p && **p != '"') (*p)++;
        if (**p != '"') {
            doc->error = "Unterminated string";
            return 0;
        }
        
        size_t len = *p - start;
        *node = ctoon_new_node(doc, CTOON_TYPE_STRING);
        if (!*node) return 0;
        
        (*node)->u.str = ctoon_alloc(doc, len + 1);
        if (!(*node)->u.str) return 0;
        
        memcpy((*node)->u.str, start, len);
        (*node)->u.str[len] = '\0';
        (*p)++;
        return 1;
    }
    
    if (isdigit((unsigned char)**p) || **p == '-' || **p == '+') {
        // Number
        char* end;
        double num = strtod(*p, &end);
        if (end == *p) {
            doc->error = "Invalid number";
            return 0;
        }
        
        *node = ctoon_new_node(doc, CTOON_TYPE_NUMBER);
        if (!*node) return 0;
        
        (*node)->u.num = num;
        *p = end;
        return 1;
    }
    
    if (strncmp(*p, "true", 4) == 0) {
        *node = ctoon_new_node(doc, CTOON_TYPE_TRUE);
        if (!*node) return 0;
        *p += 4;
        return 1;
    }
    
    if (strncmp(*p, "false", 5) == 0) {
        *node = ctoon_new_node(doc, CTOON_TYPE_FALSE);
        if (!*node) return 0;
        *p += 5;
        return 1;
    }
    
    if (strncmp(*p, "null", 4) == 0) {
        *node = ctoon_new_node(doc, CTOON_TYPE_NULL);
        if (!*node) return 0;
        *p += 4;
        return 1;
    }
    
    // Simple