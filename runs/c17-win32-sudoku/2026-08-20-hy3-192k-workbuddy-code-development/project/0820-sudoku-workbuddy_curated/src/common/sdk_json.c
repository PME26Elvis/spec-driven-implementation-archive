/* sdk_json.c - strict recursive-descent JSON parser with byte accurate
 * line/column diagnostics.
 */

#include "common/sdk_json.h"
#include "common/sdk_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct parser {
    const char     *s;
    size_t          len;
    size_t          pos;
    int             line;
    int             col;
    unsigned        depth;
    sdk_json_error *err;
    int             failed;
} parser;

static void fail(parser *p, const char *msg) {
    if (p->failed) {
        return;
    }
    p->failed = 1;
    if (p->err) {
        p->err->line = p->line;
        p->err->column = p->col;
        snprintf(p->err->message, sizeof p->err->message, "%s", msg);
    }
}

static int at_end(const parser *p) {
    return p->pos >= p->len;
}

static int peek(const parser *p) {
    return at_end(p) ? -1 : (unsigned char)p->s[p->pos];
}

static int advance(parser *p) {
    if (at_end(p)) {
        return -1;
    }
    int c = (unsigned char)p->s[p->pos++];
    if (c == '\n') {
        p->line++;
        p->col = 1;
    } else {
        p->col++;
    }
    return c;
}

static void skip_ws(parser *p) {
    for (;;) {
        int c = peek(p);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance(p);
        } else {
            return;
        }
    }
}

static sdk_json_value *alloc_value(parser *p, sdk_json_type t) {
    sdk_json_value *v = (sdk_json_value *)calloc(1, sizeof *v);
    if (!v) {
        fail(p, "out of memory");
        return NULL;
    }
    v->type = t;
    return v;
}

static sdk_json_value *parse_value(parser *p);

/* Appends the UTF-8 encoding of a scalar code point. */
static int append_utf8(sdk_buf *b, uint32_t cp) {
    if (cp <= 0x7Fu) {
        return sdk_buf_append_u8(b, (uint8_t)cp);
    }
    if (cp <= 0x7FFu) {
        return sdk_buf_append_u8(b, (uint8_t)(0xC0u | (cp >> 6))) &&
               sdk_buf_append_u8(b, (uint8_t)(0x80u | (cp & 0x3Fu)));
    }
    if (cp <= 0xFFFFu) {
        return sdk_buf_append_u8(b, (uint8_t)(0xE0u | (cp >> 12))) &&
               sdk_buf_append_u8(b, (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu))) &&
               sdk_buf_append_u8(b, (uint8_t)(0x80u | (cp & 0x3Fu)));
    }
    return sdk_buf_append_u8(b, (uint8_t)(0xF0u | (cp >> 18))) &&
           sdk_buf_append_u8(b, (uint8_t)(0x80u | ((cp >> 12) & 0x3Fu))) &&
           sdk_buf_append_u8(b, (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu))) &&
           sdk_buf_append_u8(b, (uint8_t)(0x80u | (cp & 0x3Fu)));
}

static int parse_hex4(parser *p, uint32_t *out) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        int c = advance(p);
        int d;
        if (c >= '0' && c <= '9')      d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else {
            fail(p, "invalid \\u escape: expected 4 hexadecimal digits");
            return 0;
        }
        v = (v << 4) | (uint32_t)d;
    }
    *out = v;
    return 1;
}

/* Parses a string literal; the leading quote must already be consumed by the
 * caller check but not advanced. */
static char *parse_string_raw(parser *p, size_t *out_len) {
    if (peek(p) != '"') {
        fail(p, "expected '\"' at start of string");
        return NULL;
    }
    advance(p);

    sdk_buf b;
    sdk_buf_init(&b);

    for (;;) {
        if (at_end(p)) {
            fail(p, "unterminated string");
            sdk_buf_free(&b);
            return NULL;
        }
        int c = advance(p);
        if (c == '"') {
            break;
        }
        if (c == '\\') {
            int e = advance(p);
            switch (e) {
            case '"':  sdk_buf_append_u8(&b, '"');  break;
            case '\\': sdk_buf_append_u8(&b, '\\'); break;
            case '/':  sdk_buf_append_u8(&b, '/');  break;
            case 'b':  sdk_buf_append_u8(&b, '\b'); break;
            case 'f':  sdk_buf_append_u8(&b, '\f'); break;
            case 'n':  sdk_buf_append_u8(&b, '\n'); break;
            case 'r':  sdk_buf_append_u8(&b, '\r'); break;
            case 't':  sdk_buf_append_u8(&b, '\t'); break;
            case 'u': {
                uint32_t hi;
                if (!parse_hex4(p, &hi)) {
                    sdk_buf_free(&b);
                    return NULL;
                }
                if (hi >= 0xD800u && hi <= 0xDBFFu) {
                    if (peek(p) != '\\') {
                        fail(p, "high surrogate is not followed by a low surrogate");
                        sdk_buf_free(&b);
                        return NULL;
                    }
                    advance(p);
                    if (advance(p) != 'u') {
                        fail(p, "high surrogate is not followed by a \\u escape");
                        sdk_buf_free(&b);
                        return NULL;
                    }
                    uint32_t lo;
                    if (!parse_hex4(p, &lo)) {
                        sdk_buf_free(&b);
                        return NULL;
                    }
                    if (lo < 0xDC00u || lo > 0xDFFFu) {
                        fail(p, "invalid low surrogate in \\u escape pair");
                        sdk_buf_free(&b);
                        return NULL;
                    }
                    uint32_t cp = 0x10000u + ((hi - 0xD800u) << 10) + (lo - 0xDC00u);
                    append_utf8(&b, cp);
                } else if (hi >= 0xDC00u && hi <= 0xDFFFu) {
                    fail(p, "unpaired low surrogate in \\u escape");
                    sdk_buf_free(&b);
                    return NULL;
                } else {
                    append_utf8(&b, hi);
                }
                break;
            }
            default:
                fail(p, "unsupported escape sequence");
                sdk_buf_free(&b);
                return NULL;
            }
        } else if (c < 0x20) {
            fail(p, "unescaped control character in string");
            sdk_buf_free(&b);
            return NULL;
        } else {
            sdk_buf_append_u8(&b, (uint8_t)c);
        }

        if (b.len > SDK_LIMIT_JSON_STRING_BYTES) {
            fail(p, "string exceeds the 1 MiB canonical limit");
            sdk_buf_free(&b);
            return NULL;
        }
        if (b.failed) {
            fail(p, "out of memory while decoding string");
            sdk_buf_free(&b);
            return NULL;
        }
    }

    if (!sdk_buf_append_u8(&b, 0)) {
        fail(p, "out of memory while decoding string");
        sdk_buf_free(&b);
        return NULL;
    }
    *out_len = b.len - 1;
    return (char *)b.data; /* ownership transferred */
}

static sdk_json_value *parse_number(parser *p) {
    size_t start = p->pos;
    int is_int = 1;

    if (peek(p) == '-') {
        advance(p);
    }
    int c = peek(p);
    if (c == '0') {
        advance(p);
        if (peek(p) >= '0' && peek(p) <= '9') {
            fail(p, "leading zeros are not allowed in numbers");
            return NULL;
        }
    } else if (c >= '1' && c <= '9') {
        while (peek(p) >= '0' && peek(p) <= '9') {
            advance(p);
        }
    } else {
        fail(p, "invalid number");
        return NULL;
    }
    if (peek(p) == '.') {
        is_int = 0;
        advance(p);
        if (!(peek(p) >= '0' && peek(p) <= '9')) {
            fail(p, "expected digit after decimal point");
            return NULL;
        }
        while (peek(p) >= '0' && peek(p) <= '9') {
            advance(p);
        }
    }
    if (peek(p) == 'e' || peek(p) == 'E') {
        is_int = 0;
        advance(p);
        if (peek(p) == '+' || peek(p) == '-') {
            advance(p);
        }
        if (!(peek(p) >= '0' && peek(p) <= '9')) {
            fail(p, "expected digit in exponent");
            return NULL;
        }
        while (peek(p) >= '0' && peek(p) <= '9') {
            advance(p);
        }
    }

    size_t n = p->pos - start;
    sdk_json_value *v = alloc_value(p, SDK_JSON_NUMBER);
    if (!v) {
        return NULL;
    }
    v->number_text = (char *)malloc(n + 1);
    if (!v->number_text) {
        fail(p, "out of memory");
        free(v);
        return NULL;
    }
    memcpy(v->number_text, p->s + start, n);
    v->number_text[n] = '\0';
    v->number = strtod(v->number_text, NULL);
    v->number_is_integer = is_int;
    if (is_int) {
        /* strtoll is adequate: canonical integer fields are far below 2^63. */
        v->number_integer = (int64_t)strtoll(v->number_text, NULL, 10);
    }
    return v;
}

static int expect_word(parser *p, const char *word) {
    size_t n = strlen(word);
    if (p->len - p->pos < n || memcmp(p->s + p->pos, word, n) != 0) {
        return 0;
    }
    for (size_t i = 0; i < n; ++i) {
        advance(p);
    }
    return 1;
}

static sdk_json_value *parse_array(parser *p) {
    advance(p); /* '[' */
    if (++p->depth > SDK_LIMIT_JSON_DEPTH) {
        fail(p, "nesting depth exceeds the canonical limit of 64");
        return NULL;
    }
    sdk_json_value *v = alloc_value(p, SDK_JSON_ARRAY);
    if (!v) {
        return NULL;
    }
    skip_ws(p);
    if (peek(p) == ']') {
        advance(p);
        p->depth--;
        return v;
    }
    size_t cap = 0;
    for (;;) {
        skip_ws(p);
        sdk_json_value *item = parse_value(p);
        if (!item) {
            sdk_json_free(v);
            return NULL;
        }
        if (v->item_count == cap) {
            size_t ncap = cap ? cap * 2 : 8;
            sdk_json_value **np =
                (sdk_json_value **)realloc(v->items, ncap * sizeof *np);
            if (!np) {
                fail(p, "out of memory");
                sdk_json_free(item);
                sdk_json_free(v);
                return NULL;
            }
            v->items = np;
            cap = ncap;
        }
        v->items[v->item_count++] = item;

        skip_ws(p);
        int c = peek(p);
        if (c == ',') {
            advance(p);
            skip_ws(p);
            if (peek(p) == ']') {
                fail(p, "trailing comma in array");
                sdk_json_free(v);
                return NULL;
            }
            continue;
        }
        if (c == ']') {
            advance(p);
            p->depth--;
            return v;
        }
        fail(p, "expected ',' or ']' in array");
        sdk_json_free(v);
        return NULL;
    }
}

static sdk_json_value *parse_object(parser *p) {
    advance(p); /* '{' */
    if (++p->depth > SDK_LIMIT_JSON_DEPTH) {
        fail(p, "nesting depth exceeds the canonical limit of 64");
        return NULL;
    }
    sdk_json_value *v = alloc_value(p, SDK_JSON_OBJECT);
    if (!v) {
        return NULL;
    }
    skip_ws(p);
    if (peek(p) == '}') {
        advance(p);
        p->depth--;
        return v;
    }
    size_t cap = 0;
    for (;;) {
        skip_ws(p);
        if (peek(p) != '"') {
            fail(p, "expected a quoted member name");
            sdk_json_free(v);
            return NULL;
        }
        size_t klen = 0;
        char *key = parse_string_raw(p, &klen);
        if (!key) {
            sdk_json_free(v);
            return NULL;
        }
        skip_ws(p);
        if (peek(p) != ':') {
            fail(p, "expected ':' after member name");
            free(key);
            sdk_json_free(v);
            return NULL;
        }
        advance(p);
        skip_ws(p);
        sdk_json_value *val = parse_value(p);
        if (!val) {
            free(key);
            sdk_json_free(v);
            return NULL;
        }
        /* Duplicate keys are rejected: silently keeping one is exactly the
         * class of typo failure docs/19 section 4.6 wants surfaced. */
        for (size_t i = 0; i < v->member_count; ++i) {
            if (strcmp(v->members[i].key, key) == 0) {
                fail(p, "duplicate member name in object");
                free(key);
                sdk_json_free(val);
                sdk_json_free(v);
                return NULL;
            }
        }
        if (v->member_count == cap) {
            size_t ncap = cap ? cap * 2 : 8;
            sdk_json_member *nm =
                (sdk_json_member *)realloc(v->members, ncap * sizeof *nm);
            if (!nm) {
                fail(p, "out of memory");
                free(key);
                sdk_json_free(val);
                sdk_json_free(v);
                return NULL;
            }
            v->members = nm;
            cap = ncap;
        }
        v->members[v->member_count].key = key;
        v->members[v->member_count].key_len = klen;
        v->members[v->member_count].value = val;
        v->member_count++;

        skip_ws(p);
        int c = peek(p);
        if (c == ',') {
            advance(p);
            skip_ws(p);
            if (peek(p) == '}') {
                fail(p, "trailing comma in object");
                sdk_json_free(v);
                return NULL;
            }
            continue;
        }
        if (c == '}') {
            advance(p);
            p->depth--;
            return v;
        }
        fail(p, "expected ',' or '}' in object");
        sdk_json_free(v);
        return NULL;
    }
}

static sdk_json_value *parse_value(parser *p) {
    int c = peek(p);
    switch (c) {
    case '{': return parse_object(p);
    case '[': return parse_array(p);
    case '"': {
        sdk_json_value *v = alloc_value(p, SDK_JSON_STRING);
        if (!v) {
            return NULL;
        }
        v->string = parse_string_raw(p, &v->string_len);
        if (!v->string) {
            free(v);
            return NULL;
        }
        return v;
    }
    case 't':
        if (!expect_word(p, "true")) {
            fail(p, "invalid literal; expected true");
            return NULL;
        }
        {
            sdk_json_value *v = alloc_value(p, SDK_JSON_BOOL);
            if (v) v->boolean = 1;
            return v;
        }
    case 'f':
        if (!expect_word(p, "false")) {
            fail(p, "invalid literal; expected false");
            return NULL;
        }
        {
            sdk_json_value *v = alloc_value(p, SDK_JSON_BOOL);
            if (v) v->boolean = 0;
            return v;
        }
    case 'n':
        if (!expect_word(p, "null")) {
            fail(p, "invalid literal; expected null");
            return NULL;
        }
        return alloc_value(p, SDK_JSON_NULL);
    case -1:
        fail(p, "unexpected end of input");
        return NULL;
    default:
        if (c == '-' || (c >= '0' && c <= '9')) {
            return parse_number(p);
        }
        fail(p, "unexpected character");
        return NULL;
    }
}

sdk_json_value *sdk_json_parse(const char *text, size_t len, sdk_json_error *err) {
    parser p;
    p.s = text;
    p.len = len;
    p.pos = 0;
    p.line = 1;
    p.col = 1;
    p.depth = 0;
    p.err = err;
    p.failed = 0;

    if (err) {
        err->line = 1;
        err->column = 1;
        err->message[0] = '\0';
    }

    /* A UTF-8 BOM is tolerated before the root value. */
    if (len >= 3 && (unsigned char)text[0] == 0xEF &&
        (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) {
        p.pos = 3;
        p.col = 1;
    }

    skip_ws(&p);
    sdk_json_value *root = parse_value(&p);
    if (!root) {
        return NULL;
    }
    skip_ws(&p);
    if (!at_end(&p)) {
        fail(&p, "unexpected trailing content after the root value");
        sdk_json_free(root);
        return NULL;
    }
    return root;
}

void sdk_json_free(sdk_json_value *v) {
    if (!v) {
        return;
    }
    switch (v->type) {
    case SDK_JSON_STRING:
        free(v->string);
        break;
    case SDK_JSON_NUMBER:
        free(v->number_text);
        break;
    case SDK_JSON_ARRAY:
        for (size_t i = 0; i < v->item_count; ++i) {
            sdk_json_free(v->items[i]);
        }
        free(v->items);
        break;
    case SDK_JSON_OBJECT:
        for (size_t i = 0; i < v->member_count; ++i) {
            free(v->members[i].key);
            sdk_json_free(v->members[i].value);
        }
        free(v->members);
        break;
    default:
        break;
    }
    free(v);
}

const sdk_json_value *sdk_json_object_get(const sdk_json_value *v, const char *key) {
    if (!v || v->type != SDK_JSON_OBJECT) {
        return NULL;
    }
    for (size_t i = 0; i < v->member_count; ++i) {
        if (strcmp(v->members[i].key, key) == 0) {
            return v->members[i].value;
        }
    }
    return NULL;
}
