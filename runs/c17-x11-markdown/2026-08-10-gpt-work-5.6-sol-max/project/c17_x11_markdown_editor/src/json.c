#include "mdedit/json.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *text;
    size_t len;
    size_t at;
    size_t line;
    size_t column;
    unsigned depth;
    MdJsonError *error;
} JsonParser;

static void json_fail(JsonParser *p, const char *message) {
    if (p->error == NULL || p->error->message[0] != '\0') return;
    p->error->offset = p->at;
    p->error->line = p->line;
    p->error->column = p->column;
    (void)snprintf(p->error->message, sizeof(p->error->message), "%s", message);
}

static char json_peek(const JsonParser *p) {
    return p->at < p->len ? p->text[p->at] : '\0';
}

static char json_take(JsonParser *p) {
    if (p->at >= p->len) return '\0';
    char c = p->text[p->at++];
    if (c == '\n') {
        ++p->line;
        p->column = 1U;
    } else {
        ++p->column;
    }
    return c;
}

static void json_space(JsonParser *p) {
    while (p->at < p->len) {
        char c = json_peek(p);
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
        (void)json_take(p);
    }
}

static MdJson *json_new(MdJsonType type) {
    MdJson *v = calloc(1U, sizeof(*v));
    if (v != NULL) v->type = type;
    return v;
}

void md_json_free(MdJson *value) {
    if (value == NULL) return;
    if (value->type == MD_JSON_STRING) {
        free(value->as.string);
    } else if (value->type == MD_JSON_ARRAY) {
        for (size_t i = 0U; i < value->as.array.len; ++i) md_json_free(value->as.array.items[i]);
        free(value->as.array.items);
    } else if (value->type == MD_JSON_OBJECT) {
        for (size_t i = 0U; i < value->as.object.len; ++i) {
            free(value->as.object.items[i].key);
            md_json_free(value->as.object.items[i].value);
        }
        free(value->as.object.items);
    }
    free(value);
}

static bool json_append_utf8(MdBuf *out, uint32_t cp) {
    char b[4];
    size_t n = 0U;
    if (cp <= 0x7FU) {
        b[n++] = (char)cp;
    } else if (cp <= 0x7FFU) {
        b[n++] = (char)(0xC0U | (cp >> 6U));
        b[n++] = (char)(0x80U | (cp & 0x3FU));
    } else if (cp <= 0xFFFFU) {
        if (cp >= 0xD800U && cp <= 0xDFFFU) return false;
        b[n++] = (char)(0xE0U | (cp >> 12U));
        b[n++] = (char)(0x80U | ((cp >> 6U) & 0x3FU));
        b[n++] = (char)(0x80U | (cp & 0x3FU));
    } else if (cp <= 0x10FFFFU) {
        b[n++] = (char)(0xF0U | (cp >> 18U));
        b[n++] = (char)(0x80U | ((cp >> 12U) & 0x3FU));
        b[n++] = (char)(0x80U | ((cp >> 6U) & 0x3FU));
        b[n++] = (char)(0x80U | (cp & 0x3FU));
    } else return false;
    return md_buf_append(out, b, n);
}

static int json_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool json_hex4(JsonParser *p, uint32_t *value) {
    uint32_t v = 0U;
    for (unsigned i = 0U; i < 4U; ++i) {
        if (p->at >= p->len) return false;
        int d = json_hex(json_take(p));
        if (d < 0) return false;
        v = (v << 4U) | (uint32_t)d;
    }
    *value = v;
    return true;
}

static char *json_string(JsonParser *p) {
    if (json_take(p) != '"') return NULL;
    MdBuf out;
    md_buf_init(&out);
    while (p->at < p->len) {
        char c = json_take(p);
        if (c == '"') return out.data == NULL ? md_strdup("") : out.data;
        if ((unsigned char)c < 0x20U) {
            json_fail(p, "Unescaped control character in string");
            md_buf_free(&out);
            return NULL;
        }
        if (c != '\\') {
            if (!md_buf_append_char(&out, c)) goto oom;
            continue;
        }
        if (p->at >= p->len) break;
        char e = json_take(p);
        switch (e) {
            case '"': case '\\': case '/': if (!md_buf_append_char(&out, e)) goto oom; break;
            case 'b': if (!md_buf_append_char(&out, '\b')) goto oom; break;
            case 'f': if (!md_buf_append_char(&out, '\f')) goto oom; break;
            case 'n': if (!md_buf_append_char(&out, '\n')) goto oom; break;
            case 'r': if (!md_buf_append_char(&out, '\r')) goto oom; break;
            case 't': if (!md_buf_append_char(&out, '\t')) goto oom; break;
            case 'u': {
                uint32_t cp = 0U;
                if (!json_hex4(p, &cp)) {
                    json_fail(p, "Invalid Unicode escape");
                    md_buf_free(&out);
                    return NULL;
                }
                if (cp >= 0xD800U && cp <= 0xDBFFU) {
                    if (p->at + 2U > p->len || json_take(p) != '\\' || json_take(p) != 'u') {
                        json_fail(p, "Missing low surrogate");
                        md_buf_free(&out);
                        return NULL;
                    }
                    uint32_t low = 0U;
                    if (!json_hex4(p, &low) || low < 0xDC00U || low > 0xDFFFU) {
                        json_fail(p, "Invalid low surrogate");
                        md_buf_free(&out);
                        return NULL;
                    }
                    cp = 0x10000U + ((cp - 0xD800U) << 10U) + (low - 0xDC00U);
                } else if (cp >= 0xDC00U && cp <= 0xDFFFU) {
                    json_fail(p, "Unexpected low surrogate");
                    md_buf_free(&out);
                    return NULL;
                }
                if (!json_append_utf8(&out, cp)) goto oom;
                break;
            }
            default:
                json_fail(p, "Invalid string escape");
                md_buf_free(&out);
                return NULL;
        }
    }
    json_fail(p, "Unterminated string");
    md_buf_free(&out);
    return NULL;
oom:
    json_fail(p, "Out of memory");
    md_buf_free(&out);
    return NULL;
}

static MdJson *json_value(JsonParser *p);

static bool json_array_push(MdJson *array, MdJson *item) {
    if (array->as.array.len == array->as.array.cap) {
        size_t next = array->as.array.cap == 0U ? 8U : array->as.array.cap * 2U;
        if (next < array->as.array.cap) return false;
        MdJson **items = realloc(array->as.array.items, next * sizeof(*items));
        if (items == NULL) return false;
        array->as.array.items = items;
        array->as.array.cap = next;
    }
    array->as.array.items[array->as.array.len++] = item;
    return true;
}

static bool json_object_push(MdJson *object, char *key, MdJson *item) {
    for (size_t i = 0U; i < object->as.object.len; ++i) {
        if (strcmp(object->as.object.items[i].key, key) == 0) return false;
    }
    if (object->as.object.len == object->as.object.cap) {
        size_t next = object->as.object.cap == 0U ? 8U : object->as.object.cap * 2U;
        if (next < object->as.object.cap) return false;
        MdJsonMember *items = realloc(object->as.object.items, next * sizeof(*items));
        if (items == NULL) return false;
        object->as.object.items = items;
        object->as.object.cap = next;
    }
    object->as.object.items[object->as.object.len].key = key;
    object->as.object.items[object->as.object.len].value = item;
    ++object->as.object.len;
    return true;
}

static MdJson *json_array(JsonParser *p) {
    (void)json_take(p);
    MdJson *array = json_new(MD_JSON_ARRAY);
    if (array == NULL) { json_fail(p, "Out of memory"); return NULL; }
    json_space(p);
    if (json_peek(p) == ']') { (void)json_take(p); return array; }
    for (;;) {
        MdJson *item = json_value(p);
        if (item == NULL || !json_array_push(array, item)) {
            if (item != NULL) md_json_free(item);
            if (p->error != NULL && p->error->message[0] == '\0') json_fail(p, "Out of memory");
            md_json_free(array);
            return NULL;
        }
        json_space(p);
        char c = json_take(p);
        if (c == ']') return array;
        if (c != ',') {
            json_fail(p, "Expected comma or closing bracket");
            md_json_free(array);
            return NULL;
        }
        json_space(p);
    }
}

static MdJson *json_object(JsonParser *p) {
    (void)json_take(p);
    MdJson *object = json_new(MD_JSON_OBJECT);
    if (object == NULL) { json_fail(p, "Out of memory"); return NULL; }
    json_space(p);
    if (json_peek(p) == '}') { (void)json_take(p); return object; }
    for (;;) {
        if (json_peek(p) != '"') {
            json_fail(p, "Expected object key string");
            md_json_free(object);
            return NULL;
        }
        char *key = json_string(p);
        if (key == NULL) { md_json_free(object); return NULL; }
        json_space(p);
        if (json_take(p) != ':') {
            json_fail(p, "Expected colon after object key");
            free(key);
            md_json_free(object);
            return NULL;
        }
        json_space(p);
        MdJson *item = json_value(p);
        if (item == NULL || !json_object_push(object, key, item)) {
            if (item != NULL) md_json_free(item);
            free(key);
            if (p->error != NULL && p->error->message[0] == '\0')
                json_fail(p, "Duplicate key or out of memory");
            md_json_free(object);
            return NULL;
        }
        json_space(p);
        char c = json_take(p);
        if (c == '}') return object;
        if (c != ',') {
            json_fail(p, "Expected comma or closing brace");
            md_json_free(object);
            return NULL;
        }
        json_space(p);
    }
}

static bool json_literal(JsonParser *p, const char *literal) {
    size_t n = strlen(literal);
    if (p->at + n > p->len || memcmp(p->text + p->at, literal, n) != 0) return false;
    for (size_t i = 0U; i < n; ++i) (void)json_take(p);
    return true;
}

static MdJson *json_number(JsonParser *p) {
    size_t start = p->at;
    if (json_peek(p) == '-') (void)json_take(p);
    if (json_peek(p) == '0') {
        (void)json_take(p);
        if (isdigit((unsigned char)json_peek(p))) { json_fail(p, "Leading zero in number"); return NULL; }
    } else if (json_peek(p) >= '1' && json_peek(p) <= '9') {
        while (isdigit((unsigned char)json_peek(p))) (void)json_take(p);
    } else { json_fail(p, "Invalid number"); return NULL; }
    if (json_peek(p) == '.') {
        (void)json_take(p);
        if (!isdigit((unsigned char)json_peek(p))) { json_fail(p, "Invalid fraction"); return NULL; }
        while (isdigit((unsigned char)json_peek(p))) (void)json_take(p);
    }
    if (json_peek(p) == 'e' || json_peek(p) == 'E') {
        (void)json_take(p);
        if (json_peek(p) == '+' || json_peek(p) == '-') (void)json_take(p);
        if (!isdigit((unsigned char)json_peek(p))) { json_fail(p, "Invalid exponent"); return NULL; }
        while (isdigit((unsigned char)json_peek(p))) (void)json_take(p);
    }
    char *text = md_strndup(p->text + start, p->at - start);
    if (text == NULL) { json_fail(p, "Out of memory"); return NULL; }
    errno = 0;
    char *end = NULL;
    double value = strtod(text, &end);
    bool valid = errno == 0 && end != NULL && *end == '\0' && isfinite(value);
    free(text);
    if (!valid) { json_fail(p, "Number is out of range"); return NULL; }
    MdJson *number = json_new(MD_JSON_NUMBER);
    if (number == NULL) { json_fail(p, "Out of memory"); return NULL; }
    number->as.number = value;
    return number;
}

static MdJson *json_value(JsonParser *p) {
    if (++p->depth > 128U) { json_fail(p, "JSON nesting is too deep"); --p->depth; return NULL; }
    json_space(p);
    char c = json_peek(p);
    MdJson *v = NULL;
    if (c == '{') v = json_object(p);
    else if (c == '[') v = json_array(p);
    else if (c == '"') {
        char *s = json_string(p);
        if (s != NULL) {
            v = json_new(MD_JSON_STRING);
            if (v == NULL) { free(s); json_fail(p, "Out of memory"); }
            else v->as.string = s;
        }
    } else if (c == 't' && json_literal(p, "true")) {
        v = json_new(MD_JSON_BOOL); if (v != NULL) v->as.boolean = true;
    } else if (c == 'f' && json_literal(p, "false")) {
        v = json_new(MD_JSON_BOOL); if (v != NULL) v->as.boolean = false;
    } else if (c == 'n' && json_literal(p, "null")) {
        v = json_new(MD_JSON_NULL);
    } else if (c == '-' || isdigit((unsigned char)c)) v = json_number(p);
    else json_fail(p, "Unexpected JSON token");
    if (v == NULL && p->error != NULL && p->error->message[0] == '\0') json_fail(p, "Out of memory");
    --p->depth;
    return v;
}

MdJson *md_json_parse(const char *text, size_t len, MdJsonError *error) {
    if (error != NULL) memset(error, 0, sizeof(*error));
    size_t bad = 0U;
    if (!md_utf8_validate(text, len, &bad)) {
        if (error != NULL) {
            error->offset = bad; error->line = 1U; error->column = bad + 1U;
            (void)snprintf(error->message, sizeof(error->message), "Invalid UTF-8");
        }
        return NULL;
    }
    JsonParser p = {text, len, 0U, 1U, 1U, 0U, error};
    MdJson *root = json_value(&p);
    if (root == NULL) return NULL;
    json_space(&p);
    if (p.at != p.len) {
        json_fail(&p, "Trailing data after JSON value");
        md_json_free(root);
        return NULL;
    }
    return root;
}

const MdJson *md_json_get(const MdJson *object, const char *key) {
    if (object == NULL || object->type != MD_JSON_OBJECT) return NULL;
    for (size_t i = 0U; i < object->as.object.len; ++i) {
        if (strcmp(object->as.object.items[i].key, key) == 0) return object->as.object.items[i].value;
    }
    return NULL;
}

const char *md_json_string(const MdJson *value) {
    return value != NULL && value->type == MD_JSON_STRING ? value->as.string : NULL;
}

bool md_json_bool(const MdJson *value, bool *out) {
    if (value == NULL || value->type != MD_JSON_BOOL) return false;
    *out = value->as.boolean;
    return true;
}

bool md_json_u64(const MdJson *value, uint64_t *out) {
    if (value == NULL || value->type != MD_JSON_NUMBER || value->as.number < 0.0 ||
        value->as.number > (double)UINT64_MAX || floor(value->as.number) != value->as.number) return false;
    *out = (uint64_t)value->as.number;
    return true;
}

bool md_json_write_escaped(MdBuf *out, const char *s, size_t len) {
    if (!md_buf_append_char(out, '"')) return false;
    size_t at = 0U;
    while (at < len) {
        unsigned char c = (unsigned char)s[at++];
        switch (c) {
            case '"': if (!md_buf_append_cstr(out, "\\\"")) return false; break;
            case '\\': if (!md_buf_append_cstr(out, "\\\\")) return false; break;
            case '\b': if (!md_buf_append_cstr(out, "\\b")) return false; break;
            case '\f': if (!md_buf_append_cstr(out, "\\f")) return false; break;
            case '\n': if (!md_buf_append_cstr(out, "\\n")) return false; break;
            case '\r': if (!md_buf_append_cstr(out, "\\r")) return false; break;
            case '\t': if (!md_buf_append_cstr(out, "\\t")) return false; break;
            default:
                if (c < 0x20U) {
                    if (!md_buf_appendf(out, "\\u%04x", (unsigned)c)) return false;
                } else if (!md_buf_append_char(out, (char)c)) return false;
                break;
        }
    }
    return md_buf_append_char(out, '"');
}

