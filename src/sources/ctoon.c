#include "ctoon/ctoon.h"
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
    char* alloc_base;  // Base of allocated memory
    char* alloc_ptr;   // Current allocation pointer
    size_t alloc_size;
};

struct ctoon_val {
    ctoon_node* node;
};

/*==============================================================================
 * MARK: - Memory Management
 *============================================================================*/

static void* ctoon_alloc(ctoon_doc* doc, size_t size) {
    if (doc->alloc_ptr + size > doc->alloc_base + doc->alloc_size) {
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
    // data parameter is kept for API compatibility but not used in this implementation
    ctoon_doc* doc = malloc(sizeof(ctoon_doc));
    if (!doc) return NULL;
    
    doc->root = NULL;
    doc->error = NULL;
    doc->error_pos = 0;
    doc->alloc_size = max_memory ? max_memory : len * 2 + 1024;
    doc->alloc_base = malloc(doc->alloc_size);
    if (!doc->alloc_base) {
        free(doc);
        return NULL;
    }
    
    doc->alloc_ptr = doc->alloc_base;
    
    return doc;
}

void ctoon_doc_free(ctoon_doc* doc) {
    if (doc) {
        free(doc->alloc_base);
        free(doc);
    }
}

ctoon_val* ctoon_doc_get_root(ctoon_doc* doc) {
    return doc ? (ctoon_val*)&doc->root : NULL;
}

size_t ctoon_doc_get_read_size(ctoon_doc* doc) {
    return doc ? doc->alloc_size : 0;
}

static size_t count_nodes(ctoon_node* node) {
    if (!node) return 0;
    
    size_t count = 1; // Count this node
    
    if (node->type == CTOON_TYPE_ARRAY) {
        for (size_t i = 0; i < node->u.arr.count; i++) {
            count += count_nodes(node->u.arr.items[i]);
        }
    } else if (node->type == CTOON_TYPE_OBJECT) {
        for (size_t i = 0; i < node->u.obj.count; i++) {
            count += count_nodes(node->u.obj.values[i]);
        }
    }
    
    return count;
}

size_t ctoon_doc_get_val_count(ctoon_doc* doc) {
    if (!doc || !doc->root) return 0;
    return count_nodes(doc->root);
}

/*==============================================================================
 * MARK: - Value Type Checking
 *============================================================================*/

ctoon_type ctoon_get_type(ctoon_val* val) {
    return val && val->node ? val->node->type : CTOON_TYPE_NULL;
}

ctoon_subtype ctoon_get_subtype(ctoon_val* val) {
    if (!val || !val->node) return CTOON_SUBTYPE_NONE;
    
    if (val->node->type == CTOON_TYPE_NUMBER) {
        double num = val->node->u.num;
        if (num >= 0 && num == (uint64_t)num) {
            return CTOON_SUBTYPE_UINT;
        } else if (num == (int64_t)num) {
            return CTOON_SUBTYPE_SINT;
        } else {
            return CTOON_SUBTYPE_REAL;
        }
    }
    
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
 * MARK: - Value Access
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
    // RAW type not implemented in this version
    if (!val || !val->node || val->node->type != CTOON_TYPE_RAW) {
        return NULL;
    }
    // In a full implementation, this would return raw data
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

// Simple iterator state (not thread-safe)
static struct {
    ctoon_node* current_array;
    size_t current_index;
} arr_iter_state = {NULL, 0};

ctoon_val* ctoon_arr_get_next(ctoon_val* val) {
    if (!val || !val->node || val->node->type != CTOON_TYPE_ARRAY) {
        return NULL;
    }
    
    // If this is a different array than before, reset iterator
    if (arr_iter_state.current_array != val->node) {
        arr_iter_state.current_array = val->node;
        arr_iter_state.current_index = 0;
    }
    
    // Return current element and advance
    if (arr_iter_state.current_index < val->node->u.arr.count) {
        return (ctoon_val*)&val->node->u.arr.items[arr_iter_state.current_index++];
    }
    
    // End of array
    arr_iter_state.current_array = NULL;
    arr_iter_state.current_index = 0;
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

// Object iterator state (not thread-safe)
static struct {
    ctoon_node* current_object;
    size_t current_index;
} obj_iter_state = {NULL, 0};

ctoon_val* ctoon_obj_iter_next(ctoon_val* val, ctoon_val** key) {
    if (!val || !val->node || val->node->type != CTOON_TYPE_OBJECT) {
        return NULL;
    }
    
    // If this is a different object than before, reset iterator
    if (obj_iter_state.current_object != val->node) {
        obj_iter_state.current_object = val->node;
        obj_iter_state.current_index = 0;
    }
    
    // Return current key-value pair and advance
    if (obj_iter_state.current_index < val->node->u.obj.count) {
        if (key) {
            *key = (ctoon_val*)&val->node->u.obj.keys[obj_iter_state.current_index];
        }
        ctoon_val* result = (ctoon_val*)&val->node->u.obj.values[obj_iter_state.current_index];
        obj_iter_state.current_index++;
        return result;
    }
    
    // End of object
    obj_iter_state.current_object = NULL;
    obj_iter_state.current_index = 0;
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
 * MARK: - Simple TOON Parser
 *============================================================================*/

static void skip_whitespace(const char** p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
}

static int parse_primitive(ctoon_doc* doc, const char** p, ctoon_node** node) {
    skip_whitespace(p);
    
    if (!**p) {
        doc->error = "Unexpected end of input";
        return 0;
    }
    
    // Check for quoted string
    if (**p == '"') {
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
    
    // Check for number
    if (isdigit((unsigned char)**p) || **p == '-' || **p == '+') {
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
    
    // Check for true/false/null
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
    
    // Unquoted string
    const char* start = *p;
    while (**p && **p != ',' && **p != '\n' && **p != '\r' && **p != ':' && !isspace((unsigned char)**p)) {
        (*p)++;
    }
    
    size_t len = *p - start;
    if (len == 0) {
        doc->error = "Expected value";
        return 0;
    }
    
    *node = ctoon_new_node(doc, CTOON_TYPE_STRING);
    if (!*node) return 0;
    
    (*node)->u.str = ctoon_alloc(doc, len + 1);
    if (!(*node)->u.str) return 0;
    
    memcpy((*node)->u.str, start, len);
    (*node)->u.str[len] = '\0';
    return 1;
}

static int parse_toon(ctoon_doc* doc, const char* data, size_t len) {
    const char* p = data;
    const char* end = data + len;
    
    skip_whitespace(&p);
    if (p >= end) {
        // Empty input
        return 0;
    }
    
    // Check if it's an array
    if (*p == '[') {
        // Parse array: [N]: value1,value2,...
        doc->root = ctoon_new_node(doc, CTOON_TYPE_ARRAY);
        if (!doc->root) return 0;
        
        doc->root->u.arr.count = 0;
        doc->root->u.arr.capacity = 4;
        doc->root->u.arr.items = ctoon_alloc(doc, sizeof(ctoon_node*) * 4);
        if (!doc->root->u.arr.items) return 0;
        
        p++; // Skip '['
        
        // Skip number
        while (p < end && *p != ']') p++;
        if (p >= end || *p != ']') {
            doc->error = "Unterminated array bracket";
            return 0;
        }
        p++; // Skip ']'
        
        skip_whitespace(&p);
        if (p >= end || *p != ':') {
            doc->error = "Expected ':' after array";
            return 0;
        }
        p++; // Skip ':'
        
        skip_whitespace(&p);
        
        // Parse values
        while (p < end && *p != '\n' && *p != '\r') {
            if (*p == ',') {
                p++;
                skip_whitespace(&p);
            }
            
            if (p >= end || *p == '\n' || *p == '\r') break;
            
            ctoon_node* item;
            if (!parse_primitive(doc, &p, &item)) return 0;
            
            // Resize if needed
            if (doc->root->u.arr.count >= doc->root->u.arr.capacity) {
                size_t new_cap = doc->root->u.arr.capacity * 2;
                ctoon_node** new_items = ctoon_alloc(doc, sizeof(ctoon_node*) * new_cap);
                if (!new_items) return 0;
                memcpy(new_items, doc->root->u.arr.items, sizeof(ctoon_node*) * doc->root->u.arr.count);
                doc->root->u.arr.items = new_items;
                doc->root->u.arr.capacity = new_cap;
            }
            
            doc->root->u.arr.items[doc->root->u.arr.count++] = item;
            skip_whitespace(&p);
        }
        
        return 1;
    }
    
    // Check if it's a single primitive (no colon)
    const char* line_end = p;
    while (line_end < end && *line_end != '\n' && *line_end != '\r') line_end++;
    
    int has_colon = 0;
    const char* tmp = p;
    while (tmp < line_end) {
        if (*tmp == ':') {
            has_colon = 1;
            break;
        }
        tmp++;
    }
    
    if (!has_colon) {
        // Single primitive
        return parse_primitive(doc, &p, &doc->root);
    }
    
    // Object with key-value pairs
    doc->root = ctoon_new_node(doc, CTOON_TYPE_OBJECT);
    if (!doc->root) return 0;
    
    doc->root->u.obj.count = 0;
    doc->root->u.obj.capacity = 4;
    doc->root->u.obj.keys = ctoon_alloc(doc, sizeof(char*) * 4);
    doc->root->u.obj.values = ctoon_alloc(doc, sizeof(ctoon_node*) * 4);
    if (!doc->root->u.obj.keys || !doc->root->u.obj.values) return 0;
    
    // Parse lines
    while (p < end) {
        skip_whitespace(&p);
        if (p >= end) break;
        
        // Parse key
        const char* key_start = p;
        while (p < end && *p != ':' && *p != '\n' && *p != '\r') p++;
        
        if (p >= end || *p != ':') {
            doc->error = "Expected ':' after key";
            return 0;
        }
        
        size_t key_len = p - key_start;
        char* key = ctoon_alloc(doc, key_len + 1);
        if (!key) return 0;
        
        memcpy(key, key_start, key_len);
        key[key_len] = '\0';
        
        // Trim trailing whitespace
        while (key_len > 0 && isspace((unsigned char)key[key_len - 1])) {
            key[--key_len] = '\0';
        }
        
        p++; // Skip ':'
        
        skip_whitespace(&p);
        
        // Parse value
        ctoon_node* value;
        if (!parse_primitive(doc, &p, &value)) return 0;
        
        // Add to object
        if (doc->root->u.obj.count >= doc->root->u.obj.capacity) {
            size_t new_cap = doc->root->u.obj.capacity * 2;
            char** new_keys = ctoon_alloc(doc, sizeof(char*) * new_cap);
            ctoon_node** new_values = ctoon_alloc(doc, sizeof(ctoon_node*) * new_cap);
            if (!new_keys || !new_values) return 0;
            
            memcpy(new_keys, doc->root->u.obj.keys, sizeof(char*) * doc->root->u.obj.count);
            memcpy(new_values, doc->root->u.obj.values, sizeof(ctoon_node*) * doc->root->u.obj.count);
            
            doc->root->u.obj.keys = new_keys;
            doc->root->u.obj.values = new_values;
            doc->root->u.obj.capacity = new_cap;
        }
        
        doc->root->u.obj.keys[doc->root->u.obj.count] = key;
        doc->root->u.obj.values[doc->root->u.obj.count] = value;
        doc->root->u.obj.count++;
        
        // Skip to next line
        while (p < end && *p != '\n' && *p != '\r') p++;
        while (p < end && (*p == '\n' || *p == '\r')) p++;
    }
    
    return 1;
}

/*==============================================================================
 * MARK: - TOON Writer Implementation
 *============================================================================*/

static size_t value_to_string(ctoon_node* node, char* buffer, size_t buffer_size, int indent) {
    if (!node) return 0;
    
    size_t written = 0;
    
    switch (node->type) {
        case CTOON_TYPE_NULL:
            if (buffer && written < buffer_size) {
                written += snprintf(buffer + written, buffer_size - written, "null");
            } else {
                written += 4;
            }
            break;
            
        case CTOON_TYPE_TRUE:
            if (buffer && written < buffer_size) {
                written += snprintf(buffer + written, buffer_size - written, "true");
            } else {
                written += 4;
            }
            break;
            
        case CTOON_TYPE_FALSE:
            if (buffer && written < buffer_size) {
                written += snprintf(buffer + written, buffer_size - written, "false");
            } else {
                written += 5;
            }
            break;
            
        case CTOON_TYPE_NUMBER: {
            char num_buf[64];
            snprintf(num_buf, sizeof(num_buf), "%.15g", node->u.num);
            size_t num_len = strlen(num_buf);
            if (buffer && written + num_len < buffer_size) {
                memcpy(buffer + written, num_buf, num_len);
            }
            written += num_len;
            break;
        }
            
        case CTOON_TYPE_STRING:
            if (node->u.str) {
                size_t str_len = strlen(node->u.str);
                // Check if string needs quoting
                int needs_quote = 0;
                for (size_t i = 0; i < str_len; i++) {
                    if (isspace((unsigned char)node->u.str[i]) || 
                        node->u.str[i] == ',' || 
                        node->u.str[i] == ':' ||
                        node->u.str[i] == '[' ||
                        node->u.str[i] == ']') {
                        needs_quote = 1;
                        break;
                    }
                }
                
                if (needs_quote) {
                    if (buffer && written + str_len + 2 < buffer_size) {
                        buffer[written++] = '"';
                        memcpy(buffer + written, node->u.str, str_len);
                        written += str_len;
                        buffer[written++] = '"';
                    } else {
                        written += str_len + 2;
                    }
                } else {
                    if (buffer && written + str_len < buffer_size) {
                        memcpy(buffer + written, node->u.str, str_len);
                    }
                    written += str_len;
                }
            }
            break;
            
        case CTOON_TYPE_ARRAY:
            if (buffer && written < buffer_size) {
                written += snprintf(buffer + written, buffer_size - written, "[%zu]: ", node->u.arr.count);
            } else {
                written += 10; // Approximate
            }
            
            for (size_t i = 0; i < node->u.arr.count; i++) {
                if (i > 0) {
                    if (buffer && written < buffer_size) buffer[written] = ',';
                    written++;
                }
                written += value_to_string(node->u.arr.items[i], 
                                          buffer ? buffer + written : NULL,
                                          buffer ? buffer_size - written : 0,
                                          0);
            }
            break;
            
        case CTOON_TYPE_OBJECT:
            for (size_t i = 0; i < node->u.obj.count; i++) {
                if (i > 0) {
                    if (buffer && written < buffer_size) buffer[written] = '\n';
                    written++;
                }
                
                // Add indentation
                for (int j = 0; j < indent; j++) {
                    if (buffer && written < buffer_size) buffer[written] = ' ';
                    written++;
                }
                
                // Key
                char* key = node->u.obj.keys[i];
                size_t key_len = strlen(key);
                if (buffer && written + key_len < buffer_size) {
                    memcpy(buffer + written, key, key_len);
                }
                written += key_len;
                
                // Colon
                if (buffer && written < buffer_size) buffer[written] = ':';
                written++;
                
                // Space
                if (buffer && written < buffer_size) buffer[written] = ' ';
                written++;
                
                // Value
                written += value_to_string(node->u.obj.values[i],
                                          buffer ? buffer + written : NULL,
                                          buffer ? buffer_size - written : 0,
                                          indent + 2);
            }
            break;
            
        default:
            break;
    }
    
    return written;
}

/*==============================================================================
 * MARK: - TOON Read/Write Implementation
 *============================================================================*/

ctoon_doc* ctoon_read_toon(const char* dat, size_t len, ctoon_toon_read_flag flg) {
    // flg parameter is kept for API compatibility but not used in this implementation
    ctoon_doc* doc = ctoon_doc_new(dat, len, 0);
    if (!doc) return NULL;
    
    if (!parse_toon(doc, dat, len)) {
        ctoon_doc_free(doc);
        return NULL;
    }
    
    return doc;
}

ctoon_doc* ctoon_read_toon_opts(const char* dat, size_t len, ctoon_toon_read_flag flg,
                                ctoon_alc* alc, ctoon_err* err) {
    (void)alc;
    (void)err;
    return ctoon_read_toon(dat, len, flg);
}

ctoon_doc* ctoon_read_toon_file(const char* path, ctoon_toon_read_flag flg,
                                ctoon_alc* alc, ctoon_err* err) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        if (err) {
            err->msg = "Failed to open file";
            err->pos = 0;
            err->code = 1;
        }
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    
    size_t read_bytes = fread(buffer, 1, file_size, f);
    buffer[read_bytes] = '\0';
    fclose(f);
    
    ctoon_doc* doc = ctoon_read_toon(buffer, read_bytes, flg);
    free(buffer);
    return doc;
}

char* ctoon_write_toon(ctoon_doc* doc, ctoon_toon_write_flag flg, size_t* len) {
    (void)flg; // Flags not used in simple implementation
    
    if (!doc || !doc->root) {
        if (len) *len = 0;
        return NULL;
    }
    
    // First pass: calculate size
    size_t needed = value_to_string(doc->root, NULL, 0, 0);
    
    // Allocate buffer
    char* buffer = malloc(needed + 1);
    if (!buffer) {
        if (len) *len = 0;
        return NULL;
    }
    
    // Second pass: write
    size_t written = value_to_string(doc->root, buffer, needed + 1, 0);
    buffer[written] = '\0';
    
    if (len) *len = written;
    return buffer;
}

char* ctoon_write_toon_opts(ctoon_doc* doc, ctoon_toon_write_flag flg,
                            ctoon_alc* alc, size_t* len, ctoon_err* err) {
    (void)alc;
    (void)err;
    return ctoon_write_toon(doc, flg, len);
}

bool ctoon_write_toon_file(const char* path, ctoon_doc* doc,
                           ctoon_toon_write_flag flg, ctoon_err* err) {
    size_t len;
    char* data = ctoon_write_toon(doc, flg, &len);
    if (!data) {
        if (err) {
            err->msg = "Failed to serialize TOON";
            err->pos = 0;
            err->code = 2;
        }
        return false;
    }
    
    FILE* f = fopen(path, "wb");
    if (!f) {
        free(data);
        if (err) {
            err->msg = "Failed to open file for writing";
            err->pos = 0;
            err->code = 3;
        }
        return false;
    }
    
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    free(data);
    
    if (written != len) {
        if (err) {
            err->msg = "Failed to write all data";
            err->pos = 0;
            err->code = 4;
        }
        return false;
    }
    
    return true;
}

ctoon_val* ctoon_doc_root(ctoon_doc* doc) {
    return ctoon_doc_get_root(doc);
}

double ctoon_get_double(ctoon_val* val) {
    return ctoon_get_real(val);
}
