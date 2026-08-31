/* locstat_core.c - testable core for the locstat line counter.
 *
 * Implements the contract declared in locstat_core.h. The CLI entry point
 * (locstat.c) and the unit tests (test_locstat.c) both depend only on that
 * surface; this file contains all real behaviour.
 *
 * Normative contracts:
 *   docs/03_DEV_TOOL_LOCSTAT.md                (primary workstream spec)
 *   docs/19_CANONICAL_FORMATS_AND_LIMITS.md    sections 2-5, 13 (canonical)
 * Where the two disagree, docs/19 wins.
 *
 * The JSON config parser is self-implemented; no third-party JSON library is
 * used anywhere in this tool.
 */
#include "locstat_core.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "common/sdk_common.h"
#include "common/sdk_sha256.h"
#include "common/sdk_win.h"

/* ================================================================== */
/* Physical line counting (docs/19 section 4.4)                       */
/* ================================================================== */

uint64_t loc_physical_lines(const uint8_t *data, size_t len) {
    size_t i = 0;
    uint64_t lines = 0;
    int has_terminator_at_end = 0;

    if (data == NULL || len == 0) {
        return 0;
    }
    /* Skip a leading UTF-8 BOM (EF BB BF) for content purposes; it never
     * contributes a line terminator. */
    if (len >= 3 && data[0] == 0xEFu && data[1] == 0xBBu && data[2] == 0xBFu) {
        i = 3;
    }
    for (; i < len; ++i) {
        uint8_t c = data[i];
        if (c == '\n') {
            ++lines;
            has_terminator_at_end = 1;
        } else if (c == '\r') {
            if (i + 1u < len && data[i + 1] == '\n') {
                ++lines;
                has_terminator_at_end = 1;
                ++i; /* consume the following LF as part of this terminator */
            } else {
                ++lines;
                has_terminator_at_end = 1;
            }
        } else {
            has_terminator_at_end = 0;
        }
    }
    /* A final partial line (no terminator at end) counts as one line. */
    if (!has_terminator_at_end) {
        ++lines;
    }
    return lines;
}

int loc_is_binary_like(const uint8_t *data, size_t len) {
    size_t i, lim;

    if (data == NULL || len == 0) {
        return 0;
    }
    lim = (len < 8192u) ? len : 8192u;
    for (i = 0; i < lim; ++i) {
        if (data[i] == 0) {
            return 1;
        }
    }
    return 0;
}

/* ================================================================== */
/* Lexical C classification (docs/03 section 7.2)                     */
/* ================================================================== */

loc_line_kind loc_classify_c_line(const uint8_t *line, size_t len,
                                  int *in_block) {
    size_t i = 0;
    int saw_code = 0;
    int saw_comment = 0;
    int in_string = 0;
    int in_char = 0;
    int prev_backslash = 0;
    int block = (in_block && *in_block) ? 1 : 0;

    while (i < len) {
        uint8_t c = line[i];

        if (in_string) {
            if (prev_backslash) {
                prev_backslash = 0;
                ++i;
                continue;
            }
            if (c == '\\') {
                prev_backslash = 1;
                ++i;
                continue;
            }
            if (c == '"') {
                in_string = 0;
            }
            ++i;
            continue;
        }
        if (in_char) {
            if (prev_backslash) {
                prev_backslash = 0;
                ++i;
                continue;
            }
            if (c == '\\') {
                prev_backslash = 1;
                ++i;
                continue;
            }
            if (c == '\'') {
                in_char = 0;
            }
            ++i;
            continue;
        }

        if (block) {
            saw_comment = 1;
            if (c == '*' && i + 1u < len && line[i + 1] == '/') {
                block = 0;
                i += 2;
                continue;
            }
            ++i;
            continue;
        }

        if (c == '/' && i + 1u < len && line[i + 1] == '/') {
            saw_comment = 1;
            break; /* rest of line is comment */
        }
        if (c == '/' && i + 1u < len && line[i + 1] == '*') {
            block = 1;
            saw_comment = 1;
            i += 2;
            continue;
        }
        if (c == '"') {
            in_string = 1;
            saw_code = 1;
            ++i;
            continue;
        }
        if (c == '\'') {
            in_char = 1;
            saw_code = 1;
            ++i;
            continue;
        }
        if (c != ' ' && c != '\t' && c != '\v' && c != '\f') {
            saw_code = 1;
        }
        ++i;
    }

    if (in_block) {
        *in_block = block;
    }

    if (saw_code && saw_comment) {
        return LOC_LINE_MIXED;
    }
    if (saw_code) {
        return LOC_LINE_CODE;
    }
    if (saw_comment) {
        return LOC_LINE_COMMENT_ONLY;
    }
    return LOC_LINE_BLANK;
}

loc_lexical loc_analyze_c(const uint8_t *data, size_t len) {
    loc_lexical lex;
    int in_block = 0;
    size_t i = 0;
    size_t line_start = 0;

    memset(&lex, 0, sizeof lex);
    lex.physical_lines = loc_physical_lines(data, len);

    if (data == NULL || len == 0) {
        return lex;
    }
    while (i < len) {
        uint8_t c = data[i];
        if (c == '\n' || c == '\r') {
            loc_line_kind k =
                loc_classify_c_line(data + line_start, i - line_start, &in_block);
            switch (k) {
            case LOC_LINE_BLANK:          ++lex.blank_lines; break;
            case LOC_LINE_COMMENT_ONLY:   ++lex.comment_only_lines; break;
            case LOC_LINE_CODE:           ++lex.code_lines; break;
            case LOC_LINE_MIXED:          ++lex.mixed_code_comment_lines; break;
            default: break;
            }
            /* Consume a possible CRLF pair as one terminator. */
            if (c == '\r' && i + 1u < len && data[i + 1] == '\n') {
                ++i;
            }
            ++i;
            line_start = i;
        } else {
            ++i;
        }
    }
    /* Trailing partial line. */
    if (line_start < len) {
        loc_line_kind k =
            loc_classify_c_line(data + line_start, len - line_start, &in_block);
        switch (k) {
        case LOC_LINE_BLANK:          ++lex.blank_lines; break;
        case LOC_LINE_COMMENT_ONLY:   ++lex.comment_only_lines; break;
        case LOC_LINE_CODE:           ++lex.code_lines; break;
        case LOC_LINE_MIXED:          ++lex.mixed_code_comment_lines; break;
        default: break;
        }
    }
    return lex;
}

/* ================================================================== */
/* Name / extension helpers                                           */
/* ================================================================== */

void loc_split_name(const char *name, char *ext_buf, size_t ext_cap,
                    char *base_buf, size_t base_cap) {
    size_t n, i, dot = (size_t)-1;

    if (name == NULL) {
        name = "";
    }
    n = strlen(name);
    if (ext_buf != NULL && ext_cap > 0) {
        ext_buf[0] = '\0';
    }
    if (base_buf != NULL && base_cap > 0) {
        size_t c = n < base_cap ? n : base_cap - 1u;
        memcpy(base_buf, name, c);
        base_buf[c] = '\0';
    }
    for (i = 0; i < n; ++i) {
        if (name[i] == '/') {
            dot = (size_t)-1;
        } else if (name[i] == '.') {
            dot = i;
        }
    }
    if (dot != (size_t)-1 && dot + 1u < n) {
        size_t j, k = 0;
        for (j = dot; j < n && k + 1u < ext_cap; ++j) {
            char ch = name[j];
            if (ch >= 'A' && ch <= 'Z') {
                ch = (char)(ch - 'A' + 'a');
            }
            ext_buf[k++] = ch;
        }
        if (ext_cap > 0) {
            ext_buf[k] = '\0';
        }
    }
}

int loc_ext_eq(const char *ext, const char *pattern) {
    if (ext == NULL || pattern == NULL) {
        return 0;
    }
    /* Case-insensitive, both expected to include the leading dot. */
    while (*ext && *pattern) {
        char a = *ext, b = *pattern;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) {
            return 0;
        }
        ++ext;
        ++pattern;
    }
    return (*ext == '\0' && *pattern == '\0');
}

int loc_base_eq(const char *name, const char *pattern) {
    if (name == NULL || pattern == NULL) {
        return 0;
    }
    while (*name && *pattern) {
        char a = *name, b = *pattern;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) {
            return 0;
        }
        ++name;
        ++pattern;
    }
    return (*name == '\0' && *pattern == '\0');
}

int loc_path_has_component(const char *rel_path, const char *comp) {
    size_t n, clen, i = 0;

    if (rel_path == NULL || comp == NULL) {
        return 0;
    }
    clen = strlen(comp);
    if (clen == 0) {
        return 0;
    }
    n = strlen(rel_path);
    while (i < n) {
        size_t start = i;
        while (i < n && rel_path[i] != '/') {
            ++i;
        }
        /* component is rel_path[start..i) */
        if ((i - start) == clen &&
            _strnicmp(rel_path + start, comp, clen) == 0) {
            return 1;
        }
        if (i < n) {
            ++i; /* skip separator */
        }
    }
    return 0;
}

/* ================================================================== */
/* Self-implemented JSON parser                                       */
/* ================================================================== */

typedef struct {
    const char *p;
    const char *end;
    loc_json_err *err;
    int          failed;
} jp;

static void jp_set_err(jp *j, const char *msg, const char *pos) {
    if (j->err == NULL || j->failed) {
        return;
    }
    {
        const char *cur = j->p;
        int line = 1;
        int col = 1;
        while (cur < pos && cur < j->end) {
            if (*cur == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
            ++cur;
        }
        j->err->line = line;
        j->err->column = col;
    }
    {
        size_t m = strlen(msg);
        size_t c = m < sizeof(j->err->msg) - 1u ? m : sizeof(j->err->msg) - 1u;
        memcpy(j->err->msg, msg, c);
        j->err->msg[c] = '\0';
        j->err->offset = (size_t)(pos - j->p);
    }
    j->failed = 1;
}

static void jp_skip_ws(jp *j) {
    while (j->p < j->end) {
        char c = *j->p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++j->p;
        } else {
            break;
        }
    }
}

static loc_json *jp_parse_value(jp *j, int depth);

static loc_json *jp_new(loc_json_type t) {
    loc_json *n = (loc_json *)calloc(1, sizeof *n);
    if (n) {
        n->type = t;
    }
    return n;
}

static void jp_free_node(loc_json *j) {
    size_t i;
    if (j == NULL) {
        return;
    }
    switch (j->type) {
    case LOC_JSON_STRING:
        free(j->u.string);
        break;
    case LOC_JSON_ARRAY:
        for (i = 0; i < j->u.array.count; ++i) {
            jp_free_node(j->u.array.items[i]);
        }
        free(j->u.array.items);
        break;
    case LOC_JSON_OBJECT:
        for (i = 0; i < j->u.object.count; ++i) {
            free(j->u.object.keys[i]);
            jp_free_node(j->u.object.vals[i]);
        }
        free(j->u.object.keys);
        free(j->u.object.vals);
        break;
    default:
        break;
    }
    free(j);
}

void loc_json_free(loc_json *j) {
    jp_free_node(j);
}

static int jp_push_array(jp *j, loc_json *arr, loc_json *v) {
    if (arr->u.array.count + 1u > arr->u.array.cap) {
        size_t nc = arr->u.array.cap ? arr->u.array.cap * 2u : 8u;
        loc_json **ni = (loc_json **)realloc(arr->u.array.items, nc * sizeof *ni);
        if (!ni) {
            return 0;
        }
        arr->u.array.items = ni;
        arr->u.array.cap = nc;
    }
    arr->u.array.items[arr->u.array.count++] = v;
    (void)j;
    return 1;
}

static int jp_push_member(jp *j, loc_json *obj, char *key, loc_json *v) {
    if (obj->u.object.count + 1u > obj->u.object.cap) {
        size_t nc = obj->u.object.cap ? obj->u.object.cap * 2u : 8u;
        char **nk = (char **)realloc(obj->u.object.keys, nc * sizeof *nk);
        loc_json **nv = (loc_json **)realloc(obj->u.object.vals, nc * sizeof *nv);
        if (!nk || !nv) {
            return 0;
        }
        obj->u.object.keys = nk;
        obj->u.object.vals = nv;
        obj->u.object.cap = nc;
    }
    obj->u.object.keys[obj->u.object.count] = key;
    obj->u.object.vals[obj->u.object.count] = v;
    ++obj->u.object.count;
    (void)j;
    return 1;
}

static loc_json *jp_parse_string(jp *j) {
    loc_json *s = jp_new(LOC_JSON_STRING);
    sdk_buf b;
    const char *start = j->p;

    if (!s) {
        jp_set_err(j, "out of memory", j->p);
        return NULL;
    }
    sdk_buf_init(&b);
    /* assume already on opening quote */
    ++j->p; /* skip " */
    while (j->p < j->end) {
        char c = *j->p;
        if (c == '"') {
            ++j->p;
            s->u.string = (char *)malloc(b.len + 1u);
            if (!s->u.string) {
                sdk_buf_free(&b);
                loc_json_free(s);
                jp_set_err(j, "out of memory", start);
                return NULL;
            }
            memcpy(s->u.string, b.data, b.len);
            s->u.string[b.len] = '\0';
            sdk_buf_free(&b);
            return s;
        }
        if (c == '\\') {
            ++j->p;
            if (j->p >= j->end) {
                sdk_buf_free(&b);
                loc_json_free(s);
                jp_set_err(j, "unterminated escape", start);
                return NULL;
            }
            {
                char outc;
                char e = *j->p;
                switch (e) {
                case '"':  outc = '"'; break;
                case '\\': outc = '\\'; break;
                case '/':  outc = '/'; break;
                case 'b':  outc = '\b'; break;
                case 'f':  outc = '\f'; break;
                case 'n':  outc = '\n'; break;
                case 'r':  outc = '\r'; break;
                case 't':  outc = '\t'; break;
                case 'u': {
                    int cp = 0, k;
                    ++j->p;
                    for (k = 0; k < 4 && j->p < j->end; ++k, ++j->p) {
                        char h = *j->p;
                        int d;
                        if (h >= '0' && h <= '9') d = h - '0';
                        else if (h >= 'a' && h <= 'f') d = h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') d = h - 'A' + 10;
                        else {
                            sdk_buf_free(&b);
                            loc_json_free(s);
                            jp_set_err(j, "bad \\u hex", start);
                            return NULL;
                        }
                        cp = (cp << 4) | d;
                    }
                    if (cp < 0x80) {
                        sdk_buf_append_u8(&b, (uint8_t)cp);
                    } else if (cp < 0x800) {
                        sdk_buf_append_u8(&b, (uint8_t)(0xC0 | (cp >> 6)));
                        sdk_buf_append_u8(&b, (uint8_t)(0x80 | (cp & 0x3F)));
                    } else {
                        sdk_buf_append_u8(&b, (uint8_t)(0xE0 | (cp >> 12)));
                        sdk_buf_append_u8(&b,
                                          (uint8_t)(0x80 | ((cp >> 6) & 0x3F)));
                        sdk_buf_append_u8(&b, (uint8_t)(0x80 | (cp & 0x3F)));
                    }
                    continue; /* j->p already advanced */
                }
                default:
                    sdk_buf_free(&b);
                    loc_json_free(s);
                    jp_set_err(j, "bad escape", start);
                    return NULL;
                }
                sdk_buf_append_u8(&b, (uint8_t)outc);
                ++j->p;
            }
            continue;
        }
        if ((unsigned char)c < 0x20u) {
            sdk_buf_free(&b);
            loc_json_free(s);
            jp_set_err(j, "control char in string", start);
            return NULL;
        }
        sdk_buf_append_u8(&b, (uint8_t)c);
        ++j->p;
    }
    sdk_buf_free(&b);
    loc_json_free(s);
    jp_set_err(j, "unterminated string", start);
    return NULL;
}

static loc_json *jp_parse_number(jp *j) {
    const char *start = j->p;
    int is_int = 1;
    loc_json *n;
    double val;

    if (j->p < j->end && (*j->p == '-' || *j->p == '+')) {
        ++j->p;
    }
    while (j->p < j->end && *j->p >= '0' && *j->p <= '9') {
        ++j->p;
    }
    if (j->p < j->end && *j->p == '.') {
        is_int = 0;
        ++j->p;
        while (j->p < j->end && *j->p >= '0' && *j->p <= '9') {
            ++j->p;
        }
    }
    if (j->p < j->end && (*j->p == 'e' || *j->p == 'E')) {
        is_int = 0;
        ++j->p;
        if (j->p < j->end && (*j->p == '-' || *j->p == '+')) {
            ++j->p;
        }
        while (j->p < j->end && *j->p >= '0' && *j->p <= '9') {
            ++j->p;
        }
    }
    if (j->p == start) {
        jp_set_err(j, "invalid number", start);
        return NULL;
    }
    {
        char buf[64];
        size_t len = (size_t)(j->p - start);
        if (len >= sizeof buf) {
            len = sizeof buf - 1u;
        }
        memcpy(buf, start, len);
        buf[len] = '\0';
        val = strtod(buf, NULL);
    }
    if (!isfinite(val)) {
        jp_set_err(j, "non-finite number", start);
        return NULL;
    }
    n = jp_new(LOC_JSON_NUMBER);
    if (!n) {
        jp_set_err(j, "out of memory", start);
        return NULL;
    }
    n->u.number = val;
    n->is_int = is_int;
    return n;
}

static loc_json *jp_parse_value(jp *j, int depth) {
    loc_json *v;
    if (depth > (int)SDK_LIMIT_JSON_DEPTH) {
        jp_set_err(j, "json nesting too deep", j->p);
        return NULL;
    }
    jp_skip_ws(j);
    if (j->p >= j->end) {
        jp_set_err(j, "unexpected end of input", j->p);
        return NULL;
    }
    {
        char c = *j->p;
        if (c == '{') {
            loc_json *obj = jp_new(LOC_JSON_OBJECT);
            if (!obj) {
                jp_set_err(j, "out of memory", j->p);
                return NULL;
            }
            ++j->p;
            jp_skip_ws(j);
            if (j->p < j->end && *j->p == '}') {
                ++j->p;
                return obj;
            }
            for (;;) {
                char *key;
                loc_json *val;
                jp_skip_ws(j);
                if (j->p >= j->end || *j->p != '"') {
                    loc_json_free(obj);
                    jp_set_err(j, "expected object key string", j->p);
                    return NULL;
                }
                {
                    loc_json *ks = jp_parse_string(j);
                    if (!ks) {
                        loc_json_free(obj);
                        return NULL;
                    }
                    key = ks->u.string;
                    free(ks); /* keep string buffer only */
                }
                jp_skip_ws(j);
                if (j->p >= j->end || *j->p != ':') {
                    free(key);
                    loc_json_free(obj);
                    jp_set_err(j, "expected ':'", j->p);
                    return NULL;
                }
                ++j->p;
                val = jp_parse_value(j, depth + 1);
                if (!val) {
                    free(key);
                    loc_json_free(obj);
                    return NULL;
                }
                if (!jp_push_member(j, obj, key, val)) {
                    free(key);
                    loc_json_free(val);
                    loc_json_free(obj);
                    jp_set_err(j, "out of memory", j->p);
                    return NULL;
                }
                jp_skip_ws(j);
                if (j->p >= j->end) {
                    loc_json_free(obj);
                    jp_set_err(j, "unterminated object", j->p);
                    return NULL;
                }
                if (*j->p == ',') {
                    ++j->p;
                    continue;
                }
                if (*j->p == '}') {
                    ++j->p;
                    break;
                }
                loc_json_free(obj);
                jp_set_err(j, "expected ',' or '}'", j->p);
                return NULL;
            }
            return obj;
        }
        if (c == '[') {
            loc_json *arr = jp_new(LOC_JSON_ARRAY);
            if (!arr) {
                jp_set_err(j, "out of memory", j->p);
                return NULL;
            }
            ++j->p;
            jp_skip_ws(j);
            if (j->p < j->end && *j->p == ']') {
                ++j->p;
                return arr;
            }
            for (;;) {
                loc_json *val = jp_parse_value(j, depth + 1);
                if (!val) {
                    loc_json_free(arr);
                    return NULL;
                }
                if (!jp_push_array(j, arr, val)) {
                    loc_json_free(val);
                    loc_json_free(arr);
                    jp_set_err(j, "out of memory", j->p);
                    return NULL;
                }
                jp_skip_ws(j);
                if (j->p >= j->end) {
                    loc_json_free(arr);
                    jp_set_err(j, "unterminated array", j->p);
                    return NULL;
                }
                if (*j->p == ',') {
                    ++j->p;
                    continue;
                }
                if (*j->p == ']') {
                    ++j->p;
                    break;
                }
                loc_json_free(arr);
                jp_set_err(j, "expected ',' or ']'", j->p);
                return NULL;
            }
            return arr;
        }
        if (c == '"') {
            return jp_parse_string(j);
        }
        if (c == 't') {
            if (j->end - j->p >= 4 && strncmp(j->p, "true", 4) == 0) {
                j->p += 4;
                v = jp_new(LOC_JSON_BOOL);
                if (!v) { jp_set_err(j, "out of memory", j->p); return NULL; }
                v->u.boolean = 1;
                return v;
            }
            jp_set_err(j, "invalid literal", j->p);
            return NULL;
        }
        if (c == 'f') {
            if (j->end - j->p >= 5 && strncmp(j->p, "false", 5) == 0) {
                j->p += 5;
                v = jp_new(LOC_JSON_BOOL);
                if (!v) { jp_set_err(j, "out of memory", j->p); return NULL; }
                v->u.boolean = 0;
                return v;
            }
            jp_set_err(j, "invalid literal", j->p);
            return NULL;
        }
        if (c == 'n') {
            if (j->end - j->p >= 4 && strncmp(j->p, "null", 4) == 0) {
                j->p += 4;
                v = jp_new(LOC_JSON_NULL);
                if (!v) { jp_set_err(j, "out of memory", j->p); return NULL; }
                return v;
            }
            jp_set_err(j, "invalid literal", j->p);
            return NULL;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return jp_parse_number(j);
        }
        jp_set_err(j, "unexpected character", j->p);
        return NULL;
    }
}

loc_json *loc_json_parse(const char *text, size_t len, loc_json_err *err) {
    jp j;
    loc_json *root;

    memset(&j, 0, sizeof j);
    j.p = text;
    j.end = text + len;
    j.err = err;
    if (err) {
        err->msg[0] = '\0';
        err->offset = 0;
        err->line = 0;
        err->column = 0;
    }
    root = jp_parse_value(&j, 0);
    if (j.failed) {
        if (root) {
            loc_json_free(root);
        }
        return NULL;
    }
    jp_skip_ws(&j);
    if (j.p != j.end) {
        if (root) {
            loc_json_free(root);
        }
        jp_set_err(&j, "trailing characters after value", j.p);
        return NULL;
    }
    return root;
}

const loc_json *loc_json_object_get(const loc_json *obj, const char *key) {
    size_t i;
    if (obj == NULL || obj->type != LOC_JSON_OBJECT) {
        return NULL;
    }
    for (i = 0; i < obj->u.object.count; ++i) {
        if (strcmp(obj->u.object.keys[i], key) == 0) {
            return obj->u.object.vals[i];
        }
    }
    return NULL;
}

const loc_json *loc_json_array_at(const loc_json *arr, size_t i) {
    if (arr == NULL || arr->type != LOC_JSON_ARRAY) {
        return NULL;
    }
    if (i >= arr->u.array.count) {
        return NULL;
    }
    return arr->u.array.items[i];
}

/* ================================================================== */
/* Configuration                                                      */
/* ================================================================== */

/* Fixed canonical default-config JSON. Its raw bytes (exactly as written) are
 * the config_digest_sha256 when built-in defaults are used. */
const char LOC_DEFAULT_CONFIG_JSON[] =
    "{\n"
    "  \"include_extensions\": [],\n"
    "  \"exclude_extensions\": [],\n"
    "  \"exclude_paths\": [],\n"
    "  \"categories\": {\n"
    "    \"source\": [\".c\", \".h\"],\n"
    "    \"docs\": [\".md\", \".txt\"],\n"
    "    \"config\": [\".json\", \".yaml\", \".yml\", \"build.cmd\", \".rc\", "
    "\".manifest\"]\n"
    "  },\n"
    "  \"max_file_bytes\": 67108864,\n"
    "  \"follow_reparse_points\": false\n"
    "}";

static int cfg_add_str(char ***arr, size_t *count, size_t *cap, char *s) {
    if (*count + 1u > *cap) {
        size_t nc = *cap ? *cap * 2u : 8u;
        char **ni = (char **)realloc(*arr, nc * sizeof *ni);
        if (!ni) {
            return 0;
        }
        *arr = ni;
        *cap = nc;
    }
    (*arr)[(*count)++] = s;
    return 1;
}

static int cfg_collect_string_array(const loc_json *node, char ***out,
                                    size_t *count, size_t *cap,
                                    loc_json_err *err, const char *ctx) {
    size_t i;
    if (node->type != LOC_JSON_ARRAY) {
        if (err) {
            snprintf(err->msg, sizeof err->msg, "%s must be an array", ctx);
            err->line = 0;
        }
        return 0;
    }
    for (i = 0; i < node->u.array.count; ++i) {
        loc_json *el = node->u.array.items[i];
        char *copy;
        if (el->type != LOC_JSON_STRING) {
            if (err) {
                snprintf(err->msg, sizeof err->msg,
                         "%s must contain only strings", ctx);
            }
            return 0;
        }
        copy = _strdup(el->u.string);
        if (!copy || !cfg_add_str(out, count, cap, copy)) {
            free(copy);
            return 0;
        }
    }
    return 1;
}

int loc_config_parse(const char *text, size_t len, loc_config *out,
                     loc_json_err *err) {
    loc_json *root;
    size_t i;
    int rc = SDK_ERR_DATA;
    size_t incap = 0, excap = 0, expcap = 0;

    memset(out, 0, sizeof *out);
    out->max_file_bytes = 67108864u;
    out->follow_reparse_points = 0;

    root = loc_json_parse(text, len, err);
    if (root == NULL) {
        return SDK_ERR_DATA;
    }
    if (root->type != LOC_JSON_OBJECT) {
        if (err) {
            snprintf(err->msg, sizeof err->msg,
                     "config root must be a JSON object");
        }
        loc_json_free(root);
        return SDK_ERR_DATA;
    }

    for (i = 0; i < root->u.object.count; ++i) {
        const char *key = root->u.object.keys[i];
        loc_json *val = root->u.object.vals[i];

        if (strcmp(key, "include_extensions") == 0) {
            if (!cfg_collect_string_array(val, &out->include_extensions,
                                          &out->include_count, &incap, err,
                                          "include_extensions")) {
                goto fail;
            }
        } else if (strcmp(key, "exclude_extensions") == 0) {
            if (!cfg_collect_string_array(val, &out->exclude_extensions,
                                          &out->exclude_count, &excap, err,
                                          "exclude_extensions")) {
                goto fail;
            }
        } else if (strcmp(key, "exclude_paths") == 0) {
            if (!cfg_collect_string_array(val, &out->exclude_paths,
                                          &out->exclude_paths_count, &expcap, err,
                                          "exclude_paths")) {
                goto fail;
            }
        } else if (strcmp(key, "categories") == 0) {
            size_t k;
            if (val->type != LOC_JSON_OBJECT) {
                if (err) {
                    snprintf(err->msg, sizeof err->msg,
                             "categories must be an object");
                }
                goto fail;
            }
            for (k = 0; k < val->u.object.count; ++k) {
                loc_category_rule r;
                loc_json *arr = val->u.object.vals[k];
                /* Capacity tracker MUST be per-category: r.extensions restarts
                 * at NULL for every category, so a stale capacity from the
                 * previous category would suppress the first allocation and
                 * write through a NULL pointer. */
                size_t rcap = 0;
                r.name = _strdup(val->u.object.keys[k]);
                r.extensions = NULL;
                r.count = 0;
                if (!r.name) {
                    goto fail;
                }
                if (arr->type != LOC_JSON_ARRAY) {
                    free(r.name);
                    if (err) {
                        snprintf(err->msg, sizeof err->msg,
                                 "category '%s' must be an array",
                                 val->u.object.keys[k]);
                    }
                    goto fail;
                }
                if (!cfg_collect_string_array(arr, &r.extensions, &r.count,
                                              &rcap, err, "category")) {
                    size_t z;
                    for (z = 0; z < r.count; ++z) {
                        free(r.extensions[z]);
                    }
                    free(r.extensions);
                    free(r.name);
                    goto fail;
                }
                {
                    loc_category_rule *nr = (loc_category_rule *)realloc(
                        out->categories,
                        (out->categories_count + 1u) * sizeof *nr);
                    if (!nr) {
                        size_t z;
                        for (z = 0; z < r.count; ++z) {
                            free(r.extensions[z]);
                        }
                        free(r.extensions);
                        free(r.name);
                        goto fail;
                    }
                    out->categories = nr;
                    out->categories[out->categories_count++] = r;
                }
            }
        } else if (strcmp(key, "max_file_bytes") == 0) {
            double d;
            if (val->type != LOC_JSON_NUMBER || !val->is_int) {
                if (err) {
                    snprintf(err->msg, sizeof err->msg,
                             "max_file_bytes must be an integer");
                }
                goto fail;
            }
            d = val->u.number;
            if (d < 1.0 || d > 67108864.0) {
                if (err) {
                    snprintf(err->msg, sizeof err->msg,
                             "max_file_bytes out of range 1..67108864");
                }
                goto fail;
            }
            out->max_file_bytes = (uint64_t)d;
            out->has_max_file_bytes = 1;
        } else if (strcmp(key, "follow_reparse_points") == 0) {
            if (val->type != LOC_JSON_BOOL) {
                if (err) {
                    snprintf(err->msg, sizeof err->msg,
                             "follow_reparse_points must be boolean");
                }
                goto fail;
            }
            out->has_follow_reparse = 1;
            out->follow_reparse_points = val->u.boolean;
            if (out->follow_reparse_points) {
                if (err) {
                    snprintf(err->msg, sizeof err->msg,
                             "follow_reparse_points=true is unsupported");
                }
                goto fail;
            }
        } else {
            if (err) {
                snprintf(err->msg, sizeof err->msg,
                         "unknown config key: '%s'", key);
            }
            goto fail;
        }
    }

    loc_json_free(root);
    return SDK_OK;

fail:
    loc_json_free(root);
    loc_config_free(out);
    return rc;
}

void loc_config_free(loc_config *cfg) {
    size_t i;
    if (cfg == NULL) {
        return;
    }
    for (i = 0; i < cfg->include_count; ++i) {
        free(cfg->include_extensions[i]);
    }
    free(cfg->include_extensions);
    for (i = 0; i < cfg->exclude_count; ++i) {
        free(cfg->exclude_extensions[i]);
    }
    free(cfg->exclude_extensions);
    for (i = 0; i < cfg->exclude_paths_count; ++i) {
        free(cfg->exclude_paths[i]);
    }
    free(cfg->exclude_paths);
    for (i = 0; i < cfg->categories_count; ++i) {
        size_t j;
        free(cfg->categories[i].name);
        for (j = 0; j < cfg->categories[i].count; ++j) {
            free(cfg->categories[i].extensions[j]);
        }
        free(cfg->categories[i].extensions);
    }
    free(cfg->categories);
    memset(cfg, 0, sizeof *cfg);
    cfg->max_file_bytes = 67108864u;
}

/* ================================================================== */
/* Exclusion decision (pure)                                          */
/* ================================================================== */

const char *loc_exclude_reason_str(loc_exclude_reason r) {
    switch (r) {
    case LOC_EXCLUDE_NONE:           return "none";
    case LOC_EXCLUDE_REPARSE:        return "reparse_point";
    case LOC_EXCLUDE_DEFAULT_DIR:    return "default_dir";
    case LOC_EXCLUDE_DEFAULT_EXT:    return "default_ext";
    case LOC_EXCLUDE_CONFIG_PATH:    return "config_path";
    case LOC_EXCLUDE_CONFIG_EXT:     return "config_ext";
    case LOC_EXCLUDE_INCLUDE_FILTER: return "include_filter";
    case LOC_EXCLUDE_OVERSIZE:       return "oversize";
    default:                         return "unknown";
    }
}

/* Fixed default-excluded directory component names (docs/03 section 4). */
static const char *kDefaultExcludeDirs[] = {
    ".git", ".tinyvcs", "build", "dist", "out", "logs", "results",
    "screenshots", "recordings", "tmp", "temp", "coverage"
};

/* Fixed default-excluded binary/object/archive/media extensions. */
static const char *kDefaultExcludeExts[] = {
    ".exe", ".dll", ".sys", ".drv", ".obj", ".o", ".lib", ".a", ".so", ".dylib",
    ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".tif", ".tiff", ".webp",
    ".wmf", ".emf", ".cur", ".ani",
    ".wav", ".mp3", ".mp4", ".avi", ".mov", ".mkv", ".webm", ".flac", ".aac",
    ".ogg", ".wma", ".m4a",
    ".zip", ".tar", ".gz", ".tgz", ".bz2", ".7z", ".rar", ".xz", ".lz4",
    ".iso", ".bin", ".dat", ".img", ".vhd", ".vhdx",
    ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
    ".db", ".sqlite", ".sqlite3", ".mdb", ".accdb",
    ".msi", ".cab", ".msix", ".appx",
    ".ttf", ".otf", ".woff", ".woff2", ".eot",
    ".class", ".pyc", ".jar", ".wasm"
};

static int in_string_list(const char *const *list, size_t n, const char *s) {
    size_t i;
    for (i = 0; i < n; ++i) {
        if (loc_ext_eq(list[i], s)) {
            return 1;
        }
    }
    return 0;
}

loc_exclude_reason loc_entry_excluded(const loc_config *cfg,
                                      int default_excludes,
                                      const char *rel_path,
                                      const char *name,
                                      const char *ext,
                                      int is_dir,
                                      int is_reparse_point,
                                      uint64_t file_bytes) {
    size_t i;

    if (is_reparse_point) {
        return LOC_EXCLUDE_REPARSE;
    }
    if (cfg != NULL && cfg->has_max_file_bytes && !is_dir &&
        file_bytes > cfg->max_file_bytes) {
        return LOC_EXCLUDE_OVERSIZE;
    }
    if (default_excludes) {
        if (is_dir) {
            for (i = 0; i < sizeof kDefaultExcludeDirs /
                             sizeof kDefaultExcludeDirs[0]; ++i) {
                if (loc_path_has_component(rel_path, kDefaultExcludeDirs[i])) {
                    return LOC_EXCLUDE_DEFAULT_DIR;
                }
            }
        } else {
            if (in_string_list(kDefaultExcludeExts,
                               sizeof kDefaultExcludeExts /
                                   sizeof kDefaultExcludeExts[0],
                               ext)) {
                return LOC_EXCLUDE_DEFAULT_EXT;
            }
        }
    }
    if (cfg != NULL) {
        for (i = 0; i < cfg->exclude_paths_count; ++i) {
            if (sdk_ignore_match(cfg->exclude_paths[i], rel_path, is_dir)) {
                return LOC_EXCLUDE_CONFIG_PATH;
            }
        }
        for (i = 0; i < cfg->exclude_count; ++i) {
            if (loc_ext_eq(ext, cfg->exclude_extensions[i])) {
                return LOC_EXCLUDE_CONFIG_EXT;
            }
        }
        if (cfg->include_count > 0 && !is_dir) {
            int hit = 0;
            for (i = 0; i < cfg->include_count; ++i) {
                if (loc_ext_eq(ext, cfg->include_extensions[i]) ||
                    loc_base_eq(name, cfg->include_extensions[i])) {
                    hit = 1;
                    break;
                }
            }
            if (!hit) {
                return LOC_EXCLUDE_INCLUDE_FILTER;
            }
        }
    }
    return LOC_EXCLUDE_NONE;
}

/* ================================================================== */
/* Category classification (pure)                                     */
/* ================================================================== */

static int category_rule_matches(const loc_category_rule *r, const char *name,
                                 const char *ext) {
    size_t i;
    for (i = 0; i < r->count; ++i) {
        const char *pat = r->extensions[i];
        if (pat == NULL || pat[0] == '\0') {
            continue;
        }
        if (pat[0] == '.') {
            if (loc_ext_eq(ext, pat)) {
                return 1;
            }
        } else {
            if (loc_base_eq(name, pat)) {
                return 1;
            }
        }
    }
    return 0;
}

const char *loc_classify_category(const loc_config *cfg,
                                  const char *rel_path,
                                  const char *name,
                                  const char *ext) {
    size_t i;

    /* 1. tests path rule */
    if ((loc_path_has_component(rel_path, "test") ||
         loc_path_has_component(rel_path, "tests")) &&
        (loc_ext_eq(ext, ".c") || loc_ext_eq(ext, ".h"))) {
        return "tests";
    }
    /* 2. explicit config categories in declaration order */
    if (cfg != NULL) {
        for (i = 0; i < cfg->categories_count; ++i) {
            if (category_rule_matches(&cfg->categories[i], name, ext)) {
                return cfg->categories[i].name;
            }
        }
    }
    /* 3. built-in source */
    if (loc_ext_eq(ext, ".c") || loc_ext_eq(ext, ".h")) {
        return "source";
    }
    /* 4. built-in docs */
    if (loc_ext_eq(ext, ".md") || loc_ext_eq(ext, ".txt")) {
        return "docs";
    }
    /* 5. built-in config */
    if (loc_ext_eq(ext, ".json") || loc_ext_eq(ext, ".yaml") ||
        loc_ext_eq(ext, ".yml") || loc_ext_eq(ext, ".rc") ||
        loc_ext_eq(ext, ".manifest")) {
        return "config";
    }
    if (loc_base_eq(name, "build.cmd")) {
        return "config";
    }
    /* 6. unclassified */
    return "unclassified";
}

void loc_classify_category_detail(const loc_config *cfg,
                                  const char *rel_path,
                                  const char *name,
                                  const char *ext,
                                  char *sel_buf, size_t sel_cap,
                                  char *rules_buf, size_t rules_cap) {
    int first_rule = 1;
    const char *selected = NULL;

    if (sel_buf && sel_cap > 0) {
        sel_buf[0] = '\0';
    }
    if (rules_buf && rules_cap > 0) {
        rules_buf[0] = '\0';
    }

    if ((loc_path_has_component(rel_path, "test") ||
         loc_path_has_component(rel_path, "tests")) &&
        (loc_ext_eq(ext, ".c") || loc_ext_eq(ext, ".h"))) {
        selected = "tests";
        if (rules_buf && rules_cap > 0) {
            strcat_s(rules_buf, rules_cap, "test_path");
        }
        first_rule = 0;
    }
    if (cfg != NULL) {
        size_t i;
        for (i = 0; i < cfg->categories_count; ++i) {
            if (category_rule_matches(&cfg->categories[i], name, ext)) {
                if (selected == NULL) {
                    selected = cfg->categories[i].name;
                }
                if (rules_buf && rules_cap > 0) {
                    char tmp[64];
                    if (!first_rule) {
                        strcat_s(rules_buf, rules_cap, ",");
                    }
                    snprintf(tmp, sizeof tmp, "config:%s",
                             cfg->categories[i].name);
                    strcat_s(rules_buf, rules_cap, tmp);
                    first_rule = 0;
                }
            }
        }
    }
    if (loc_ext_eq(ext, ".c") || loc_ext_eq(ext, ".h")) {
        if (selected == NULL) {
            selected = "source";
        }
        if (rules_buf && rules_cap > 0) {
            if (!first_rule) {
                strcat_s(rules_buf, rules_cap, ",");
            }
            strcat_s(rules_buf, rules_cap, "builtin_source");
            first_rule = 0;
        }
    }
    if (loc_ext_eq(ext, ".md") || loc_ext_eq(ext, ".txt")) {
        if (selected == NULL) {
            selected = "docs";
        }
        if (rules_buf && rules_cap > 0) {
            if (!first_rule) {
                strcat_s(rules_buf, rules_cap, ",");
            }
            strcat_s(rules_buf, rules_cap, "builtin_docs");
            first_rule = 0;
        }
    }
    if (loc_ext_eq(ext, ".json") || loc_ext_eq(ext, ".yaml") ||
        loc_ext_eq(ext, ".yml") || loc_ext_eq(ext, ".rc") ||
        loc_ext_eq(ext, ".manifest") || loc_base_eq(name, "build.cmd")) {
        if (selected == NULL) {
            selected = "config";
        }
        if (rules_buf && rules_cap > 0) {
            if (!first_rule) {
                strcat_s(rules_buf, rules_cap, ",");
            }
            strcat_s(rules_buf, rules_cap, "builtin_config");
            first_rule = 0;
        }
    }
    if (selected == NULL) {
        selected = "unclassified";
    }
    if (sel_buf && sel_cap > 0) {
        strncpy_s(sel_buf, sel_cap, selected, _TRUNCATE);
    }
}

/* ================================================================== */
/* Report + scan                                                      */
/* ================================================================== */

void loc_report_init(loc_report *rep) {
    memset(rep, 0, sizeof *rep);
}

static void add_str_dyn(char ***arr, size_t *count, size_t *cap,
                        const char *s) {
    char *copy = _strdup(s);
    if (copy == NULL) {
        return;
    }
    if (*count + 1u > *cap) {
        size_t nc = *cap ? *cap * 2u : 16u;
        char **ni = (char **)realloc(*arr, nc * sizeof *ni);
        if (!ni) {
            free(copy);
            return;
        }
        *arr = ni;
        *cap = nc;
    }
    (*arr)[(*count)++] = copy;
}

void loc_report_free(loc_report *rep) {
    size_t i;
    if (rep == NULL) {
        return;
    }
    for (i = 0; i < rep->files_count; ++i) {
        free(rep->files[i].path);
        free(rep->files[i].category);
        free(rep->files[i].matched_rules);
    }
    free(rep->files);
    for (i = 0; i < rep->excluded_count; ++i) {
        free(rep->excluded[i].path);
    }
    free(rep->excluded);
    for (i = 0; i < rep->warnings_count; ++i) {
        free(rep->warnings[i]);
    }
    free(rep->warnings);
    for (i = 0; i < rep->errors_count; ++i) {
        free(rep->errors[i]);
    }
    free(rep->errors);
    for (i = 0; i < rep->cats_count; ++i) {
        free(rep->cats[i].name);
    }
    free(rep->cats);
    free(rep->root);
}

static loc_excluded_record *push_excluded(loc_report *rep, const char *path,
                                          loc_exclude_reason reason) {
    if (rep->excluded_count + 1u > rep->excluded_cap) {
        size_t nc = rep->excluded_cap ? rep->excluded_cap * 2u : 64u;
        loc_excluded_record *ni = (loc_excluded_record *)realloc(
            rep->excluded, nc * sizeof *ni);
        if (!ni) {
            return NULL;
        }
        rep->excluded = ni;
        rep->excluded_cap = nc;
    }
    {
        loc_excluded_record *r = &rep->excluded[rep->excluded_count];
        r->path = _strdup(path);
        r->reason = reason;
        if (r->path == NULL) {
            return NULL;
        }
        ++rep->excluded_count;
        return r;
    }
}

static loc_cat_agg *find_or_add_cat(loc_report *rep, const char *name) {
    size_t i;
    for (i = 0; i < rep->cats_count; ++i) {
        if (strcmp(rep->cats[i].name, name) == 0) {
            return &rep->cats[i];
        }
    }
    if (rep->cats_count + 1u > rep->cats_cap) {
        size_t nc = rep->cats_cap ? rep->cats_cap * 2u : 16u;
        loc_cat_agg *ni = (loc_cat_agg *)realloc(rep->cats, nc * sizeof *ni);
        if (!ni) {
            return NULL;
        }
        rep->cats = ni;
        rep->cats_cap = nc;
    }
    {
        loc_cat_agg *c = &rep->cats[rep->cats_count];
        memset(c, 0, sizeof *c);
        c->name = _strdup(name);
        if (c->name == NULL) {
            return NULL;
        }
        ++rep->cats_count;
        return c;
    }
}

/* Resolve canonical UTF-8 '/' separated root for reporting. */
static char *canon_root_utf8(const wchar_t *abs_w) {
    wchar_t *full = sdk_full_path_w(abs_w);
    char *p;
    wchar_t *stripped;
    char *u8;
    size_t i;

    if (full == NULL) {
        return NULL;
    }
    stripped = (wchar_t *)sdk_strip_longpath_prefix(full);
    /* strip_longpath_prefix returns a pointer into full; copy to own buffer */
    {
        size_t n = wcslen(stripped);
        wchar_t *own = (wchar_t *)malloc((n + 1u) * sizeof *own);
        if (!own) {
            free(full);
            return NULL;
        }
        memcpy(own, stripped, (n + 1u) * sizeof *own);
        free(full);
        stripped = own;
    }
    u8 = sdk_utf16_to_utf8(stripped, wcslen(stripped), NULL);
    free(stripped);
    if (u8 == NULL) {
        return NULL;
    }
    for (p = u8; *p; ++p) {
        if (*p == '\\') {
            *p = '/';
        }
    }
    /* strip trailing slash (keep root "/" handling simple) */
    i = strlen(u8);
    while (i > 1 && u8[i - 1] == '/') {
        u8[--i] = '\0';
    }
    return u8;
}

static int is_c_cat(const char *cat) {
    return (strcmp(cat, "source") == 0 || strcmp(cat, "tests") == 0);
}

static void scan_dir(loc_report *rep, const loc_config *cfg, int default_excludes,
                     const wchar_t *abs_dir_w, const char *rel_prefix,
                     int depth, int *out_rc, int fail_on_error) {
    sdk_dirlist list;
    size_t i;
    uint32_t win32err = 0;
    sdk_status st;

    if (*out_rc != SDK_EXIT_OK) {
        return;
    }
    if ((uint32_t)depth > SDK_LIMIT_LOCSTAT_PATH_DEPTH) {
        add_str_dyn(&rep->warnings, &rep->warnings_count, &rep->warnings_cap,
                    "path depth limit reached; subtree skipped");
        return;
    }

    st = sdk_dir_list_w(abs_dir_w, &list, NULL, &win32err);
    if (st != SDK_OK) {
        char msg[256];
        snprintf(msg, sizeof msg,
                 "cannot enumerate directory '%s' (win32=0x%08X)",
                 rel_prefix[0] ? rel_prefix : ".", (unsigned)win32err);
        add_str_dyn(&rep->errors, &rep->errors_count, &rep->errors_cap, msg);
        *out_rc = fail_on_error ? SDK_EXIT_IO : SDK_EXIT_OK;
        return;
    }

    for (i = 0; i < list.count; ++i) {
        sdk_dirent *de = &list.items[i];
        wchar_t *child_abs;
        char *child_rel;
        char ext[64];
        char base[256];
        const char *catname;
        int is_dir = de->info.is_directory;
        int is_reparse = de->info.is_reparse_point;
        uint64_t fsize = (uint64_t)de->info.size;
        loc_exclude_reason reason;

        child_abs = sdk_wpath_join(abs_dir_w, de->name_w);
        if (child_abs == NULL) {
            sdk_dirlist_free(&list);
            *out_rc = SDK_EXIT_INTERNAL;
            return;
        }
        if (rel_prefix[0] == '\0') {
            child_rel = _strdup(de->name_u8);
        } else {
            size_t plen = strlen(rel_prefix);
            size_t nlen = strlen(de->name_u8);
            child_rel = (char *)malloc(plen + 1u + nlen + 1u);
            if (child_rel) {
                memcpy(child_rel, rel_prefix, plen);
                child_rel[plen] = '/';
                memcpy(child_rel + plen + 1u, de->name_u8, nlen);
                child_rel[plen + 1u + nlen] = '\0';
            }
        }
        if (child_rel == NULL) {
            free(child_abs);
            sdk_dirlist_free(&list);
            *out_rc = SDK_EXIT_INTERNAL;
            return;
        }

        loc_split_name(de->name_u8, ext, sizeof ext, base, sizeof base);

        reason = loc_entry_excluded(cfg, default_excludes, child_rel,
                                    de->name_u8, ext, is_dir, is_reparse,
                                    fsize);
        if (reason != LOC_EXCLUDE_NONE) {
            push_excluded(rep, child_rel, reason);
            rep->total_excluded++;
            free(child_abs);
            free(child_rel);
            continue;
        }

        if (is_dir) {
            if (rep->total_files + rep->total_excluded >=
                SDK_LIMIT_LOCSTAT_FILES) {
                free(child_abs);
                free(child_rel);
                continue;
            }
            scan_dir(rep, cfg, default_excludes, child_abs, child_rel,
                     depth + 1, out_rc, fail_on_error);
            free(child_abs);
            free(child_rel);
            if (*out_rc != SDK_EXIT_OK) {
                sdk_dirlist_free(&list);
                return;
            }
            continue;
        }

        /* Regular file: read and classify. */
        {
            uint8_t *data = NULL;
            size_t dlen = 0;
            uint32_t ferr = 0;
            sdk_status rs = sdk_file_read_all_w(child_abs,
                                               (size_t)cfg->max_file_bytes,
                                               &data, &dlen, &ferr);
            loc_file_record rec;
            loc_lexical lex;
            int binary = 0;
            int enc_warn = 0;

            memset(&rec, 0, sizeof rec);
            memset(&lex, 0, sizeof lex);

            if (rs != SDK_OK) {
                char msg[256];
                snprintf(msg, sizeof msg,
                         "unreadable file '%s' (win32=0x%08X)",
                         child_rel, (unsigned)ferr);
                add_str_dyn(&rep->errors, &rep->errors_count,
                            &rep->errors_cap, msg);
                rep->total_errors++;
                if (fail_on_error) {
                    *out_rc = SDK_EXIT_IO;
                }
                free(child_abs);
                free(child_rel);
                free(data);
                continue;
            }

            binary = loc_is_binary_like(data, dlen);
            if (binary) {
                /* Exclude line statistics for binary-like content. */
                if (loc_ext_eq(ext, ".c") || loc_ext_eq(ext, ".h") ||
                    loc_ext_eq(ext, ".md") || loc_ext_eq(ext, ".txt")) {
                    char msg[256];
                    snprintf(msg, sizeof msg,
                             "binary-like content in '%s'; line statistics "
                             "excluded", child_rel);
                    add_str_dyn(&rep->warnings, &rep->warnings_count,
                                &rep->warnings_cap, msg);
                    rep->total_warnings++;
                }
            } else {
                if (!sdk_utf8_validate((const char *)data, dlen)) {
                    enc_warn = 1;
                    rep->total_warnings++;
                }
            }

            rec.path = _strdup(child_rel);
            catname = loc_classify_category(cfg, child_rel, de->name_u8, ext);
            rec.category = _strdup(catname);
            {
                char sel[32];
                char rules[256];
                loc_classify_category_detail(cfg, child_rel, de->name_u8, ext,
                                            sel, sizeof sel, rules,
                                            sizeof rules);
                rec.matched_rules = _strdup(rules);
            }
            rec.bytes = (uint64_t)dlen;
            if (binary) {
                rec.physical_lines = 0;
                memset(&lex, 0, sizeof lex);
            } else {
                lex = loc_analyze_c(data, dlen);
                rec.physical_lines = lex.physical_lines;
            }
            rec.lex = lex;
            rec.encoding_warning = enc_warn;
            rec.binary_like = binary;

            /* append */
            if (rep->files_count + 1u > rep->files_cap) {
                size_t nc = rep->files_cap ? rep->files_cap * 2u : 256u;
                loc_file_record *ni = (loc_file_record *)realloc(
                    rep->files, nc * sizeof *ni);
                if (!ni) {
                    free(child_abs);
                    free(child_rel);
                    free(data);
                    free(rec.path);
                    free(rec.category);
                    sdk_dirlist_free(&list);
                    *out_rc = SDK_EXIT_INTERNAL;
                    return;
                }
                rep->files = ni;
                rep->files_cap = nc;
            }
            rep->files[rep->files_count++] = rec;
            rep->total_files++;
            rep->total_bytes += rec.bytes;
            rep->totals.physical_lines += rec.physical_lines;
            rep->totals.blank_lines += lex.blank_lines;
            rep->totals.comment_only_lines += lex.comment_only_lines;
            rep->totals.code_lines += lex.code_lines;
            rep->totals.mixed_code_comment_lines += lex.mixed_code_comment_lines;

            {
                loc_cat_agg *c = find_or_add_cat(rep, catname);
                if (c) {
                    c->file_count++;
                    c->lex.physical_lines += rec.physical_lines;
                    c->lex.blank_lines += lex.blank_lines;
                    c->lex.comment_only_lines += lex.comment_only_lines;
                    c->lex.code_lines += lex.code_lines;
                    c->lex.mixed_code_comment_lines +=
                        lex.mixed_code_comment_lines;
                }
            }

            free(child_abs);
            free(child_rel);
            free(data);
        }
    }

    sdk_dirlist_free(&list);
}

int loc_scan(const loc_options *opts, loc_report *rep) {
    int rc = SDK_EXIT_OK;
    sdk_fileinfo root_info;
    wchar_t *root_w = NULL;
    wchar_t *resolved = NULL;
    char *cfg_text = NULL;
    size_t cfg_len = 0;
    uint8_t *cfg_bytes = NULL;
    size_t cfg_bytes_len = 0;
    uint8_t digest[SDK_SHA256_DIGEST_LEN];
    loc_config cfg;
    int default_excludes = opts->no_default_excludes ? 0 : 1;
    int used_default = 0;
    loc_json_err jerr;

    rep->scan_started_epoch_ms = sdk_now_epoch_ms();

    /* Resolve root. */
    root_w = sdk_wcsdup_n(opts->root, wcslen(opts->root));
    if (root_w == NULL) {
        rc = SDK_EXIT_USAGE;
        goto done;
    }
    if (sdk_stat_w(root_w, &root_info) != SDK_OK || !root_info.exists) {
        fprintf(stderr,
                "[locstat] scan root not found or inaccessible: %ls\n",
                root_w);
        rc = SDK_EXIT_USAGE;
        goto done;
    }
    if (!root_info.is_directory) {
        fprintf(stderr, "[locstat] scan root is not a directory: %ls\n",
                root_w);
        rc = SDK_EXIT_USAGE;
        goto done;
    }
    /* Root reparse resolution: resolve final path once (docs/19 section 4.2). */
    if (root_info.is_reparse_point) {
        resolved = sdk_final_directory_path_w(root_w);
        if (resolved != NULL) {
            free(root_w);
            root_w = resolved;
            resolved = NULL;
        }
    }

    /* Load config. */
    if (opts->config_path != NULL) {
        sdk_status rs = sdk_file_read_all_w(opts->config_path, 1u * 1024u * 1024u,
                                            &cfg_bytes, &cfg_bytes_len, NULL);
        if (rs != SDK_OK) {
            fprintf(stderr, "[locstat] config file unreadable: %ls\n",
                    opts->config_path);
            rc = SDK_EXIT_DATA;
            goto done;
        }
        cfg_text = (char *)malloc(cfg_bytes_len + 1u);
        if (!cfg_text) {
            rc = SDK_EXIT_INTERNAL;
            goto done;
        }
        memcpy(cfg_text, cfg_bytes, cfg_bytes_len);
        cfg_text[cfg_bytes_len] = '\0';
        cfg_len = cfg_bytes_len;
        sdk_sha256(cfg_bytes, cfg_bytes_len, digest);
    } else {
        cfg_len = strlen(LOC_DEFAULT_CONFIG_JSON);
        cfg_text = (char *)LOC_DEFAULT_CONFIG_JSON; /* no copy; static */
        sdk_sha256((const uint8_t *)LOC_DEFAULT_CONFIG_JSON, cfg_len, digest);
        used_default = 1;
    }

    if (loc_config_parse(cfg_text, cfg_len, &cfg, &jerr) != SDK_OK) {
        if (opts->config_path != NULL) {
            fprintf(stderr,
                    "[locstat] malformed config at line %d column %d: %s\n",
                    jerr.line, jerr.column, jerr.msg);
        } else {
            fprintf(stderr, "[locstat] internal: default config rejected: %s\n",
                    jerr.msg);
        }
        rc = SDK_EXIT_DATA;
        if (opts->config_path != NULL) {
            free(cfg_text);
        }
        free(cfg_bytes);
        goto done;
    }

    sdk_hex_encode(digest, SDK_SHA256_DIGEST_LEN, rep->config_digest_hex);
    rep->used_default_config = used_default;
    rep->root = canon_root_utf8(root_w);

    /* Traverse. */
    scan_dir(rep, &cfg, default_excludes, root_w, "", 0, &rc,
             opts->fail_on_error);

    rep->scanned = 1;

    loc_config_free(&cfg);
    if (opts->config_path != NULL) {
        free(cfg_text);
    }
    free(cfg_bytes);
    free(root_w);

    rep->scan_duration_ms = sdk_now_epoch_ms() - rep->scan_started_epoch_ms;
    return rc;

done:
    rep->scanned = 0;
    if (opts->config_path != NULL) {
        free(cfg_text);
    }
    free(cfg_bytes);
    free(resolved);
    free(root_w);
    rep->scan_duration_ms = sdk_now_epoch_ms() - rep->scan_started_epoch_ms;
    return rc;
}

/* ================================================================== */
/* Human-readable output (docs/03 section 8)                         */
/* ================================================================== */

void loc_write_text(const loc_report *rep, const loc_options *opts, FILE *out) {
    size_t i;
    const char *filter = opts ? opts->category_filter : NULL;
    uint64_t docs_total = 0;
    uint64_t src_code = 0, src_blank = 0, src_comment = 0, src_mixed = 0;

    fprintf(out, "locstat report\n");
    fprintf(out, "scan root: %s\n", rep->root ? rep->root : "(unknown)");
    fprintf(out, "config: %s (digest %s)\n",
            rep->used_default_config ? "built-in defaults"
                                     : "file",
            rep->config_digest_hex);
    fprintf(out, "files scanned: %llu\n", (unsigned long long)rep->total_files);
    fprintf(out, "files excluded: %llu\n",
            (unsigned long long)rep->total_excluded);
    fprintf(out, "warnings: %llu   errors: %llu\n",
            (unsigned long long)rep->total_warnings,
            (unsigned long long)rep->total_errors);

    fprintf(out, "\nper-category:\n");
    for (i = 0; i < rep->cats_count; ++i) {
        const loc_cat_agg *c = &rep->cats[i];
        if (filter && strcmp(c->name, filter) != 0) {
            continue;
        }
        fprintf(out,
                "  %-12s files=%-6llu physical=%-8llu "
                "blank=%-7llu comment=%-7llu code=%-7llu mixed=%-7llu\n",
                c->name, (unsigned long long)c->file_count,
                (unsigned long long)c->lex.physical_lines,
                (unsigned long long)c->lex.blank_lines,
                (unsigned long long)c->lex.comment_only_lines,
                (unsigned long long)c->lex.code_lines,
                (unsigned long long)c->lex.mixed_code_comment_lines);
        if (is_c_cat(c->name)) {
            src_code += c->lex.code_lines;
            src_blank += c->lex.blank_lines;
            src_comment += c->lex.comment_only_lines;
            src_mixed += c->lex.mixed_code_comment_lines;
        }
        if (strcmp(c->name, "docs") == 0) {
            docs_total += c->lex.physical_lines;
        }
    }

    fprintf(out, "\nsource/test code statistics:\n");
    fprintf(out,
            "  code=%llu  comment_only=%llu  blank=%llu  mixed=%llu\n",
            (unsigned long long)src_code, (unsigned long long)src_comment,
            (unsigned long long)src_blank, (unsigned long long)src_mixed);

    fprintf(out, "\nall human-readable docs total lines: %llu\n",
            (unsigned long long)docs_total);

    fprintf(out, "\nper-file:\n");
    for (i = 0; i < rep->files_count; ++i) {
        const loc_file_record *f = &rep->files[i];
        if (filter && strcmp(f->category, filter) != 0) {
            continue;
        }
        fprintf(out,
                "  %s  category=%s  bytes=%llu  lines=%llu  "
                "blank=%llu comment=%llu code=%llu mixed=%llu enc_warn=%d%s"
                "  rules=%s\n",
                f->path, f->category, (unsigned long long)f->bytes,
                (unsigned long long)f->physical_lines,
                (unsigned long long)f->lex.blank_lines,
                (unsigned long long)f->lex.comment_only_lines,
                (unsigned long long)f->lex.code_lines,
                (unsigned long long)f->lex.mixed_code_comment_lines,
                f->encoding_warning,
                f->binary_like ? "  [binary-like]" : "",
                f->matched_rules ? f->matched_rules : "");
    }

    if (rep->errors_count > 0) {
        fprintf(out, "\nunreadable / errors:\n");
        for (i = 0; i < rep->errors_count; ++i) {
            fprintf(out, "  %s\n", rep->errors[i]);
        }
    }
    if (rep->warnings_count > 0) {
        fprintf(out, "\nwarnings:\n");
        for (i = 0; i < rep->warnings_count; ++i) {
            fprintf(out, "  %s\n", rep->warnings[i]);
        }
    }
    fprintf(out, "\n");
}

/* ================================================================== */
/* Canonical JSON (docs/19 section 4.7)                              */
/* ================================================================== */

static void json_str(sdk_buf *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    sdk_buf_append_u8(b, '"');
    for (; *p; ++p) {
        switch (*p) {
        case '"':  sdk_buf_append(b, "\\\"", 2); break;
        case '\\': sdk_buf_append(b, "\\\\", 2); break;
        case '\n': sdk_buf_append(b, "\\n", 2); break;
        case '\r': sdk_buf_append(b, "\\r", 2); break;
        case '\t': sdk_buf_append(b, "\\t", 2); break;
        default:
            if (*p < 0x20u) {
                sdk_buf_appendf(b, "\\u%04x", (unsigned)*p);
            } else {
                sdk_buf_append_u8(b, *p);
            }
            break;
        }
    }
    sdk_buf_append_u8(b, '"');
}

void loc_report_to_json(const loc_report *rep, const loc_options *opts,
                        sdk_buf *buf) {
    size_t i;
    const char *filter = opts ? opts->category_filter : NULL;

    sdk_buf_append_cstr(buf, "{\n");
    sdk_buf_appendf(buf, "  \"schema_version\": 1,\n");
    sdk_buf_append_cstr(buf, "  \"tool_version\": ");
    json_str(buf, "1.0.0");
    sdk_buf_append_cstr(buf, ",\n");
    sdk_buf_append_cstr(buf, "  \"root\": ");
    json_str(buf, rep->root ? rep->root : "");
    sdk_buf_append_cstr(buf, ",\n");
    sdk_buf_append_cstr(buf, "  \"config_digest_sha256\": ");
    json_str(buf, rep->config_digest_hex);
    sdk_buf_append_cstr(buf, ",\n");
    sdk_buf_appendf(buf, "  \"scan_started_epoch_ms\": %lld,\n",
                    (long long)rep->scan_started_epoch_ms);
    sdk_buf_appendf(buf, "  \"scan_duration_ms\": %lld,\n",
                    (long long)rep->scan_duration_ms);

    /* categories */
    sdk_buf_append_cstr(buf, "  \"categories\": [");
    {
        int first = 1;
        for (i = 0; i < rep->cats_count; ++i) {
            const loc_cat_agg *c = &rep->cats[i];
            if (filter && strcmp(c->name, filter) != 0) {
                continue;
            }
            if (!first) {
                sdk_buf_append_u8(buf, ',');
            }
            first = 0;
            sdk_buf_append_cstr(buf, "{\n");
            sdk_buf_append_cstr(buf, "    \"name\": ");
            json_str(buf, c->name);
            sdk_buf_appendf(buf, ",\n    \"file_count\": %llu,\n",
                            (unsigned long long)c->file_count);
            sdk_buf_appendf(buf, "    \"physical_lines\": %llu,\n",
                            (unsigned long long)c->lex.physical_lines);
            sdk_buf_appendf(buf, "    \"blank_lines\": %llu,\n",
                            (unsigned long long)c->lex.blank_lines);
            sdk_buf_appendf(buf, "    \"comment_only_lines\": %llu,\n",
                            (unsigned long long)c->lex.comment_only_lines);
            sdk_buf_appendf(buf, "    \"code_lines\": %llu,\n",
                            (unsigned long long)c->lex.code_lines);
            sdk_buf_appendf(buf, "    \"mixed_code_comment_lines\": %llu\n",
                            (unsigned long long)
                                c->lex.mixed_code_comment_lines);
            sdk_buf_append_cstr(buf, "  }");
        }
    }
    sdk_buf_append_cstr(buf, "],\n");

    /* files */
    sdk_buf_append_cstr(buf, "  \"files\": [");
    {
        int first = 1;
        for (i = 0; i < rep->files_count; ++i) {
            const loc_file_record *f = &rep->files[i];
            if (filter && strcmp(f->category, filter) != 0) {
                continue;
            }
            if (!first) {
                sdk_buf_append_u8(buf, ',');
            }
            first = 0;
            sdk_buf_append_cstr(buf, "{\n");
            sdk_buf_append_cstr(buf, "    \"path\": ");
            json_str(buf, f->path);
            sdk_buf_append_cstr(buf, ",\n    \"category\": ");
            json_str(buf, f->category);
            sdk_buf_appendf(buf, ",\n    \"bytes\": %llu,\n",
                            (unsigned long long)f->bytes);
            sdk_buf_appendf(buf, "    \"physical_lines\": %llu,\n",
                            (unsigned long long)f->physical_lines);
            sdk_buf_appendf(buf, "    \"blank_lines\": %llu,\n",
                            (unsigned long long)f->lex.blank_lines);
            sdk_buf_appendf(buf, "    \"comment_only_lines\": %llu,\n",
                            (unsigned long long)f->lex.comment_only_lines);
            sdk_buf_appendf(buf, "    \"code_lines\": %llu,\n",
                            (unsigned long long)f->lex.code_lines);
            sdk_buf_appendf(buf, "    \"mixed_code_comment_lines\": %llu,\n",
                            (unsigned long long)
                                f->lex.mixed_code_comment_lines);
            sdk_buf_appendf(buf, "    \"encoding_warning\": %s\n",
                            f->encoding_warning ? "true" : "false");
            sdk_buf_append_cstr(buf, "  }");
        }
    }
    sdk_buf_append_cstr(buf, "],\n");

    /* excluded */
    sdk_buf_append_cstr(buf, "  \"excluded\": [");
    {
        int first = 1;
        for (i = 0; i < rep->excluded_count; ++i) {
            if (!first) {
                sdk_buf_append_u8(buf, ',');
            }
            first = 0;
            sdk_buf_append_cstr(buf, "{\n    \"path\": ");
            json_str(buf, rep->excluded[i].path);
            sdk_buf_append_cstr(buf, ",\n    \"reason\": ");
            json_str(buf, loc_exclude_reason_str(rep->excluded[i].reason));
            sdk_buf_append_cstr(buf, "\n  }");
        }
    }
    sdk_buf_append_cstr(buf, "],\n");

    /* warnings */
    sdk_buf_append_cstr(buf, "  \"warnings\": [");
    {
        int first = 1;
        for (i = 0; i < rep->warnings_count; ++i) {
            if (!first) {
                sdk_buf_append_u8(buf, ',');
            }
            first = 0;
            json_str(buf, rep->warnings[i]);
        }
    }
    sdk_buf_append_cstr(buf, "],\n");

    /* errors */
    sdk_buf_append_cstr(buf, "  \"errors\": [");
    {
        int first = 1;
        for (i = 0; i < rep->errors_count; ++i) {
            if (!first) {
                sdk_buf_append_u8(buf, ',');
            }
            first = 0;
            json_str(buf, rep->errors[i]);
        }
    }
    sdk_buf_append_cstr(buf, "],\n");

    /* totals */
    sdk_buf_append_cstr(buf, "  \"totals\": {\n");
    sdk_buf_appendf(buf, "    \"files\": %llu,\n",
                    (unsigned long long)rep->total_files);
    sdk_buf_appendf(buf, "    \"physical_lines\": %llu,\n",
                    (unsigned long long)rep->totals.physical_lines);
    sdk_buf_appendf(buf, "    \"blank_lines\": %llu,\n",
                    (unsigned long long)rep->totals.blank_lines);
    sdk_buf_appendf(buf, "    \"comment_only_lines\": %llu,\n",
                    (unsigned long long)rep->totals.comment_only_lines);
    sdk_buf_appendf(buf, "    \"code_lines\": %llu,\n",
                    (unsigned long long)rep->totals.code_lines);
    sdk_buf_appendf(buf, "    \"mixed_code_comment_lines\": %llu,\n",
                    (unsigned long long)rep->totals.mixed_code_comment_lines);
    sdk_buf_appendf(buf, "    \"bytes\": %llu,\n",
                    (unsigned long long)rep->total_bytes);
    sdk_buf_appendf(buf, "    \"excluded\": %llu,\n",
                    (unsigned long long)rep->total_excluded);
    sdk_buf_appendf(buf, "    \"warnings\": %llu,\n",
                    (unsigned long long)rep->total_warnings);
    sdk_buf_appendf(buf, "    \"errors\": %llu\n",
                    (unsigned long long)rep->total_errors);
    sdk_buf_append_cstr(buf, "  }\n");
    sdk_buf_append_cstr(buf, "}\n");
}

int loc_emit(const loc_report *rep, const loc_options *opts) {
    if (opts->json_path == NULL) {
        loc_write_text(rep, opts, stdout);
        return SDK_EXIT_OK;
    }
    {
        sdk_buf buf;
        int rc = SDK_EXIT_OK;
        sdk_buf_init(&buf);
        loc_report_to_json(rep, opts, &buf);

        if (wcscmp(opts->json_path, L"-") == 0) {
            fwrite(buf.data, 1, buf.len, stdout);
            if (buf.len > 0 && buf.data[buf.len - 1] != '\n') {
                fputc('\n', stdout);
            }
            fflush(stdout);
            loc_write_text(rep, opts, stderr);
        } else {
            sdk_status st = sdk_file_write_all_w(opts->json_path, buf.data,
                                                 buf.len, NULL);
            if (st != SDK_OK) {
                fprintf(stderr, "[locstat] failed to write JSON report\n");
                rc = SDK_EXIT_IO;
            }
            loc_write_text(rep, opts, stdout);
        }
        sdk_buf_free(&buf);
        return rc;
    }
}
