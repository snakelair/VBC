#ifndef VBC_XML_H
#define VBC_XML_H

#include "vbc_runtime.h"
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VBC_XML_DOCUMENT = 1,
    VBC_XML_ELEMENT  = 2,
    VBC_XML_TEXT     = 3,
    VBC_XML_COMMENT  = 4
} vbc_xml_node_kind;

typedef struct vbc_xml_attr {
    vbc_string_t name;
    vbc_string_t value;
    struct vbc_xml_attr* next;
} vbc_xml_attr_t;

typedef struct vbc_xml_node {
    int kind;
    vbc_string_t name;
    vbc_string_t value;
    vbc_xml_attr_t* first_attr;
    struct vbc_xml_node* first_child;
    struct vbc_xml_node* last_child;
    struct vbc_xml_node* next_sibling;
    struct vbc_xml_node* parent;
} vbc_xml_node_t;

static inline vbc_xml_node_t* vbc_xml_alloc_node(vbc_stack_t* stack, int kind, vbc_string_t name, vbc_string_t value) {
    vbc_xml_node_t* n = (vbc_xml_node_t*)vbc_stack_alloc(stack, sizeof(vbc_xml_node_t), 8);
    if (!n) return NULL;
    n->kind = kind;
    n->name = name;
    n->value = value;
    n->first_attr = NULL;
    n->first_child = NULL;
    n->last_child = NULL;
    n->next_sibling = NULL;
    n->parent = NULL;
    return n;
}

static inline void vbc_xml_node_add_child(vbc_xml_node_t* parent, vbc_xml_node_t* child) {
    if (!parent || !child) return;
    child->parent = parent;
    child->next_sibling = NULL;
    if (!parent->first_child) {
        parent->first_child = child;
        parent->last_child = child;
    } else {
        parent->last_child->next_sibling = child;
        parent->last_child = child;
    }
}

static inline void vbc_xml_node_add_attr(vbc_stack_t* stack, vbc_xml_node_t* node, vbc_string_t name, vbc_string_t value) {
    if (!node) return;
    vbc_xml_attr_t* a = (vbc_xml_attr_t*)vbc_stack_alloc(stack, sizeof(vbc_xml_attr_t), 8);
    if (!a) return;
    a->name = name;
    a->value = value;
    a->next = NULL;
    if (!node->first_attr) {
        node->first_attr = a;
    } else {
        vbc_xml_attr_t* cur = node->first_attr;
        while (cur->next) cur = cur->next;
        cur->next = a;
    }
}

/* XML Parser */
typedef struct {
    const char* src;
    int64_t len;
    int64_t pos;
    vbc_stack_t* stack;
} vbc_xml_parser_t;

static inline void vbc_xml_skip_ws(vbc_xml_parser_t* p) {
    while (p->pos < p->len && (unsigned char)p->src[p->pos] <= ' ') {
        p->pos++;
    }
}

static inline vbc_string_t vbc_xml_parse_name(vbc_xml_parser_t* p) {
    vbc_xml_skip_ws(p);
    int64_t start = p->pos;
    while (p->pos < p->len) {
        char c = p->src[p->pos];
        if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == ':' || c == '.') {
            p->pos++;
        } else {
            break;
        }
    }
    int64_t nlen = p->pos - start;
    if (nlen <= 0) return vbc_str_empty();
    return vbc_str_substring(p->stack, vbc_str_from_slice(p->src, p->len), start, nlen);
}

static inline vbc_string_t vbc_xml_parse_attr_value(vbc_xml_parser_t* p) {
    vbc_xml_skip_ws(p);
    if (p->pos >= p->len) return vbc_str_empty();
    char quote = p->src[p->pos];
    if (quote != '"' && quote != '\'') return vbc_str_empty();
    p->pos++; // skip quote
    int64_t start = p->pos;
    while (p->pos < p->len && p->src[p->pos] != quote) {
        p->pos++;
    }
    int64_t vlen = p->pos - start;
    vbc_string_t res = vbc_str_substring(p->stack, vbc_str_from_slice(p->src, p->len), start, vlen);
    if (p->pos < p->len && p->src[p->pos] == quote) p->pos++;
    return res;
}

static inline vbc_xml_node_t* vbc_xml_parse_element(vbc_xml_parser_t* p);

static inline void vbc_xml_parse_children(vbc_xml_parser_t* p, vbc_xml_node_t* parent, vbc_string_t closing_tag) {
    while (p->pos < p->len) {
        vbc_xml_skip_ws(p);
        if (p->pos >= p->len) break;

        if (p->src[p->pos] == '<') {
            if (p->pos + 1 < p->len && p->src[p->pos + 1] == '/') {
                // Closing tag </tag>
                p->pos += 2;
                vbc_string_t close_name = vbc_xml_parse_name(p);
                (void)close_name;
                while (p->pos < p->len && p->src[p->pos] != '>') p->pos++;
                if (p->pos < p->len && p->src[p->pos] == '>') p->pos++;
                break;
            } else if (p->pos + 3 < p->len && memcmp(p->src + p->pos, "<!--", 4) == 0) {
                // Comment
                p->pos += 4;
                while (p->pos + 2 < p->len && memcmp(p->src + p->pos, "-->", 3) != 0) p->pos++;
                if (p->pos + 2 < p->len) p->pos += 3;
            } else if (p->pos + 1 < p->len && p->src[p->pos + 1] == '?') {
                // XML declaration <?xml ... ?>
                p->pos += 2;
                while (p->pos + 1 < p->len && memcmp(p->src + p->pos, "?>", 2) != 0) p->pos++;
                if (p->pos + 1 < p->len) p->pos += 2;
            } else {
                vbc_xml_node_t* child = vbc_xml_parse_element(p);
                if (child) vbc_xml_node_add_child(parent, child);
            }
        } else {
            // Text node
            int64_t start = p->pos;
            while (p->pos < p->len && p->src[p->pos] != '<') p->pos++;
            int64_t tlen = p->pos - start;
            if (tlen > 0) {
                vbc_string_t raw_text = vbc_str_substring(p->stack, vbc_str_from_slice(p->src, p->len), start, tlen);
                // Trim surrounding whitespace for inner text value
                int64_t ts = 0; while (ts < raw_text.len && (unsigned char)raw_text.data[ts] <= ' ') ts++;
                int64_t te = raw_text.len - 1; while (te >= ts && (unsigned char)raw_text.data[te] <= ' ') te--;
                if (te >= ts) {
                    vbc_string_t text_val = vbc_str_substring(p->stack, raw_text, ts, te - ts + 1);
                    if (parent->value.len == 0) {
                        parent->value = text_val;
                    }
                    vbc_xml_node_t* txt_node = vbc_xml_alloc_node(p->stack, VBC_XML_TEXT, vbc_str_empty(), text_val);
                    vbc_xml_node_add_child(parent, txt_node);
                }
            }
        }
    }
}

static inline vbc_xml_node_t* vbc_xml_parse_element(vbc_xml_parser_t* p) {
    if (p->pos >= p->len || p->src[p->pos] != '<') return NULL;
    p->pos++; // skip '<'

    vbc_string_t name = vbc_xml_parse_name(p);
    if (name.len == 0) return NULL;

    vbc_xml_node_t* elem = vbc_xml_alloc_node(p->stack, VBC_XML_ELEMENT, name, vbc_str_empty());

    // Parse attributes
    while (p->pos < p->len) {
        vbc_xml_skip_ws(p);
        if (p->pos >= p->len) break;
        if (p->src[p->pos] == '>' || (p->src[p->pos] == '/' && p->pos + 1 < p->len && p->src[p->pos + 1] == '>')) {
            break;
        }
        vbc_string_t attr_name = vbc_xml_parse_name(p);
        if (attr_name.len == 0) break;
        vbc_xml_skip_ws(p);
        vbc_string_t attr_val = vbc_str_empty();
        if (p->pos < p->len && p->src[p->pos] == '=') {
            p->pos++;
            attr_val = vbc_xml_parse_attr_value(p);
        }
        vbc_xml_node_add_attr(p->stack, elem, attr_name, attr_val);
    }

    vbc_xml_skip_ws(p);
    if (p->pos + 1 < p->len && p->src[p->pos] == '/' && p->src[p->pos + 1] == '>') {
        p->pos += 2; // self-closing <tag/>
        return elem;
    }

    if (p->pos < p->len && p->src[p->pos] == '>') {
        p->pos++; // skip '>'
        vbc_xml_parse_children(p, elem, name);
    }

    return elem;
}

static inline vbc_xml_node_t* vbc_xml_parse(vbc_stack_t* stack, vbc_string_t xml) {
    if (!xml.data || xml.len <= 0) return NULL;
    vbc_xml_parser_t parser;
    parser.src = xml.data;
    parser.len = xml.len;
    parser.pos = 0;
    parser.stack = stack;

    vbc_xml_node_t* doc = vbc_xml_alloc_node(stack, VBC_XML_DOCUMENT, vbc_str_lit("#document"), vbc_str_empty());

    while (parser.pos < parser.len) {
        vbc_xml_skip_ws(&parser);
        if (parser.pos >= parser.len) break;
        if (parser.pos + 1 < parser.len && parser.src[parser.pos] == '<' && parser.src[parser.pos + 1] == '?') {
            // <?xml ... ?>
            parser.pos += 2;
            while (parser.pos + 1 < parser.len && memcmp(parser.src + parser.pos, "?>", 2) != 0) parser.pos++;
            if (parser.pos + 1 < parser.len) parser.pos += 2;
            continue;
        }
        if (parser.pos + 3 < parser.len && memcmp(parser.src + parser.pos, "<!--", 4) == 0) {
            // <!-- comment -->
            parser.pos += 4;
            while (parser.pos + 2 < parser.len && memcmp(parser.src + parser.pos, "-->", 3) != 0) parser.pos++;
            if (parser.pos + 2 < parser.len) parser.pos += 3;
            continue;
        }
        vbc_xml_node_t* root = vbc_xml_parse_element(&parser);
        if (root) {
            vbc_xml_node_add_child(doc, root);
        } else {
            parser.pos++;
        }
    }
    return doc;
}

/* ========================================================================= */
/* XML Axis & LINQ Navigation Functions                                      */
/* ========================================================================= */

static inline vbc_xml_node_t* vbc_xml_get_root(vbc_xml_node_t* doc) {
    if (!doc) return NULL;
    if (doc->kind == VBC_XML_ELEMENT) return doc;
    vbc_xml_node_t* cur = doc->first_child;
    while (cur) {
        if (cur->kind == VBC_XML_ELEMENT) return cur;
        cur = cur->next_sibling;
    }
    return NULL;
}

static inline bool vbc_xml_name_equals(vbc_string_t a, const char* b_cstr) {
    if (!a.data || !b_cstr) return false;
    size_t blen = strlen(b_cstr);
    if ((size_t)a.len != blen) return false;
    for (size_t i = 0; i < blen; i++) {
        if (tolower((unsigned char)a.data[i]) != tolower((unsigned char)b_cstr[i])) return false;
    }
    return true;
}

/* Child axis: expr.<Name> */
static inline vbc_xml_node_t* vbc_xml_element_get_child(vbc_stack_t* stack, vbc_xml_node_t* node, const char* tag_name) {
    (void)stack;
    if (!node) return NULL;
    if (node->kind == VBC_XML_DOCUMENT) {
        node = vbc_xml_get_root(node);
        if (!node) return NULL;
        if (tag_name && vbc_xml_name_equals(node->name, tag_name)) {
            return node;
        }
    }
    vbc_xml_node_t* cur = node->first_child;
    while (cur) {
        if (cur->kind == VBC_XML_ELEMENT) {
            if (!tag_name || tag_name[0] == '\0' || vbc_xml_name_equals(cur->name, tag_name)) {
                return cur;
            }
        }
        cur = cur->next_sibling;
    }
    return NULL;
}

/* Attribute axis: expr.@attr */
static inline vbc_string_t vbc_xml_element_get_attr_val(vbc_stack_t* stack, vbc_xml_node_t* node, const char* attr_name) {
    (void)stack;
    if (!node) return vbc_str_empty();
    if (node->kind == VBC_XML_DOCUMENT) node = vbc_xml_get_root(node);
    if (!node || !attr_name) return vbc_str_empty();
    vbc_xml_attr_t* a = node->first_attr;
    while (a) {
        if (vbc_xml_name_equals(a->name, attr_name)) {
            return a->value;
        }
        a = a->next;
    }
    return vbc_str_empty();
}

/* Value property: elem.Value */
static inline vbc_string_t vbc_xml_element_get_value(vbc_xml_node_t* node) {
    if (!node) return vbc_str_empty();
    if (node->kind == VBC_XML_DOCUMENT) node = vbc_xml_get_root(node);
    if (!node) return vbc_str_empty();
    if (node->value.len > 0) return node->value;
    // Check first text child
    vbc_xml_node_t* ch = node->first_child;
    while (ch) {
        if (ch->kind == VBC_XML_TEXT && ch->value.len > 0) return ch->value;
        ch = ch->next_sibling;
    }
    return vbc_str_empty();
}

/* Name property: elem.Name */
static inline vbc_string_t vbc_xml_element_get_name(vbc_xml_node_t* node) {
    if (!node) return vbc_str_empty();
    if (node->kind == VBC_XML_DOCUMENT) node = vbc_xml_get_root(node);
    if (!node) return vbc_str_empty();
    return node->name;
}

/* Descendants helper */
static inline void vbc_xml_collect_descendants(vbc_stack_t* stack, vbc_slice_t* slice, vbc_xml_node_t* node, const char* tag_name) {
    if (!node) return;
    vbc_xml_node_t* ch = node->first_child;
    while (ch) {
        if (ch->kind == VBC_XML_ELEMENT) {
            if (!tag_name || tag_name[0] == '\0' || vbc_xml_name_equals(ch->name, tag_name)) {
                vbc_slice_push(stack, slice, &ch);
            }
            vbc_xml_collect_descendants(stack, slice, ch, tag_name);
        }
        ch = ch->next_sibling;
    }
}

/* Descendants axis: expr...<Tag> */
static inline vbc_slice_t vbc_xml_element_get_descendants(vbc_stack_t* stack, vbc_xml_node_t* node, const char* tag_name) {
    vbc_slice_t slice = vbc_slice_create(stack, 4, sizeof(vbc_xml_node_t*));
    if (!node) return slice;
    if (node->kind == VBC_XML_DOCUMENT) node = vbc_xml_get_root(node);
    vbc_xml_collect_descendants(stack, &slice, node, tag_name);
    return slice;
}

/* Elements list: elem.Elements(name) */
static inline vbc_slice_t vbc_xml_element_get_elements(vbc_stack_t* stack, vbc_xml_node_t* node, const char* tag_name) {
    vbc_slice_t slice = vbc_slice_create(stack, 4, sizeof(vbc_xml_node_t*));
    if (!node) return slice;
    if (node->kind == VBC_XML_DOCUMENT) node = vbc_xml_get_root(node);
    if (!node) return slice;
    vbc_xml_node_t* ch = node->first_child;
    while (ch) {
        if (ch->kind == VBC_XML_ELEMENT) {
            if (!tag_name || tag_name[0] == '\0' || vbc_xml_name_equals(ch->name, tag_name)) {
                vbc_slice_push(stack, &slice, &ch);
            }
        }
        ch = ch->next_sibling;
    }
    return slice;
}

/* Serializer */
static inline void vbc_xml_serialize_node(vbc_stack_t* stack, vbc_string_t* out, vbc_xml_node_t* node, int indent) {
    if (!node) return;
    if (node->kind == VBC_XML_DOCUMENT) {
        vbc_xml_node_t* r = vbc_xml_get_root(node);
        if (r) vbc_xml_serialize_node(stack, out, r, 0);
        return;
    }
    if (node->kind == VBC_XML_TEXT) {
        *out = vbc_str_concat(stack, *out, node->value);
        return;
    }
    if (node->kind == VBC_XML_ELEMENT) {
        *out = vbc_str_concat(stack, *out, vbc_str_lit("<"));
        *out = vbc_str_concat(stack, *out, node->name);
        vbc_xml_attr_t* a = node->first_attr;
        while (a) {
            *out = vbc_str_concat(stack, *out, vbc_str_lit(" "));
            *out = vbc_str_concat(stack, *out, a->name);
            *out = vbc_str_concat(stack, *out, vbc_str_lit("=\""));
            *out = vbc_str_concat(stack, *out, a->value);
            *out = vbc_str_concat(stack, *out, vbc_str_lit("\""));
            a = a->next;
        }
        if (!node->first_child && node->value.len == 0) {
            *out = vbc_str_concat(stack, *out, vbc_str_lit(" />"));
            return;
        }
        *out = vbc_str_concat(stack, *out, vbc_str_lit(">"));
        if (!node->first_child) {
            if (node->value.len > 0) {
                *out = vbc_str_concat(stack, *out, node->value);
            }
        } else {
            vbc_xml_node_t* ch = node->first_child;
            while (ch) {
                vbc_xml_serialize_node(stack, out, ch, indent + 1);
                ch = ch->next_sibling;
            }
        }
        *out = vbc_str_concat(stack, *out, vbc_str_lit("</"));
        *out = vbc_str_concat(stack, *out, node->name);
        *out = vbc_str_concat(stack, *out, vbc_str_lit(">"));
    }
}

static inline vbc_string_t vbc_xml_to_string(vbc_stack_t* stack, vbc_xml_node_t* node) {
    if (!node) return vbc_str_empty();
    vbc_string_t out = vbc_str_empty();
    vbc_xml_serialize_node(stack, &out, node, 0);
    return out;
}

#ifdef __cplusplus
}
#endif

#endif /* VBC_XML_H */
